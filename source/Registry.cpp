/*
AppleWin : An Apple //e emulator for Windows

Copyright (C) 1994-1996, Michael O'Brien
Copyright (C) 1999-2001, Oliver Schmidt
Copyright (C) 2002-2005, Tom Charlesworth
Copyright (C) 2006-2007, Tom Charlesworth, Michael Pohoreski

AppleWin is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

AppleWin is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with AppleWin; if not, write to the Free Software
Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

/* Description: Registry module
 *
 * Author: Various
 */

#include "StdAfx.h"

#include "Registry.h"
#include "CmdLine.h"

namespace _ini {
	//===========================================================================
	BOOL RegLoadString(LPCTSTR section, LPCTSTR key, BOOL /*peruser*/, LPTSTR buffer, DWORD chars)
	{
		DWORD n = GetPrivateProfileString(section, key, NULL, buffer, chars, g_sConfigFile.c_str());
		return n > 0;
	}

	//===========================================================================
	void RegSaveString(LPCTSTR section, LPCTSTR key, BOOL /*peruser*/, const std::string& buffer)
	{
		BOOL updated = WritePrivateProfileString(section, key, buffer.c_str(), g_sConfigFile.c_str());
		_ASSERT(updated || GetLastError() == 0);
	}

	//===========================================================================
	void RegDeleteString(LPCTSTR section, BOOL /*peruser*/)
	{
		BOOL updated = WritePrivateProfileString(section, NULL, NULL, g_sConfigFile.c_str());
		_ASSERT(updated || GetLastError() == 0);
	}
}

//===========================================================================
BOOL RegLoadString (LPCTSTR section, LPCTSTR key, BOOL peruser, LPTSTR buffer, DWORD chars)
{
	if (!g_sConfigFile.empty())
		return _ini::RegLoadString(section, key, peruser, buffer, chars);

	TCHAR fullkeyname[256];
	StringCbPrintf(fullkeyname, 256, TEXT("Software\\AppleWin\\CurrentVersion\\%s"), section);

	BOOL success = FALSE;
	HKEY keyhandle;
	LSTATUS status = RegOpenKeyEx(
		(peruser ? HKEY_CURRENT_USER : HKEY_LOCAL_MACHINE),
		fullkeyname,
		0,
		KEY_READ,
		&keyhandle);
	if (status == ERROR_SUCCESS)
	{
		DWORD type;
		DWORD size = chars;
		status = RegQueryValueEx(keyhandle, key, NULL, &type, (LPBYTE)buffer, &size);
		if (status == 0 && size != 0)
			success = TRUE;
	}

	RegCloseKey(keyhandle);

	return success;
}

//===========================================================================
BOOL RegLoadString (LPCTSTR section, LPCTSTR key, BOOL peruser, LPTSTR buffer, DWORD chars, LPCTSTR defaultValue)
{
	BOOL success = RegLoadString(section, key, peruser, buffer, chars);
	if (!success)
		StringCbCopy(buffer, chars, defaultValue);
	return success;
}

//===========================================================================
BOOL RegLoadValue (LPCTSTR section, LPCTSTR key, BOOL peruser, DWORD* value) {
	TCHAR buffer[32];
	if (!RegLoadString(section, key, peruser, buffer, 32))
	{
		return FALSE;
	}

	*value = (DWORD)_ttoi(buffer);
	return TRUE;
}

//===========================================================================
BOOL RegLoadValue (LPCTSTR section, LPCTSTR key, BOOL peruser, DWORD* value, DWORD defaultValue) {
	BOOL success = RegLoadValue(section, key, peruser, value);
	if (!success)
		*value = defaultValue;
	return success;
}

//===========================================================================
void RegSaveString (LPCTSTR section, LPCTSTR key, BOOL peruser, const std::string & buffer) {
	if (!g_sConfigFile.empty())
		return _ini::RegSaveString(section, key, peruser, buffer);

	TCHAR fullkeyname[256];
	StringCbPrintf(fullkeyname, 256, TEXT("Software\\AppleWin\\CurrentVersion\\%s"), section);

	HKEY  keyhandle;
	DWORD disposition;
	LSTATUS status = RegCreateKeyEx(
		(peruser ? HKEY_CURRENT_USER : HKEY_LOCAL_MACHINE),
		fullkeyname,
		0,
		NULL,
		REG_OPTION_NON_VOLATILE,
		KEY_READ | KEY_WRITE,
		(LPSECURITY_ATTRIBUTES)NULL,
		&keyhandle,
		&disposition);
	if (status == ERROR_SUCCESS)
	{
		RegSetValueEx(
			keyhandle,
			key,
			0,
			REG_SZ,
			(CONST LPBYTE)buffer.c_str(),
			(buffer.size() + 1) * sizeof(TCHAR));
		RegCloseKey(keyhandle);
	}
}

//===========================================================================
void RegSaveValue (LPCTSTR section, LPCTSTR key, BOOL peruser, DWORD value) {
	TCHAR buffer[32] = TEXT("");
	StringCbPrintf(buffer, 32, "%d", value);
	RegSaveString(section, key, peruser, buffer);
}

//===========================================================================
static std::string& RegGetSlotSection(UINT slot)
{
	static std::string section;
	if (slot == SLOT_AUX)
	{
		section = REG_CONFIG_SLOT_AUX;
	}
	else
	{
		section = REG_CONFIG_SLOT;
		section += (char)('0' + slot);
	}
	return section;
}

std::string& RegGetConfigSlotSection(UINT slot)
{
	static std::string section;
	section = REG_CONFIG "\\";
	section += RegGetSlotSection(slot);
	return section;
}

void RegDeleteConfigSlotSection(UINT slot)
{
	BOOL peruser = TRUE;

	if (!g_sConfigFile.empty())
	{
		std::string& section = RegGetConfigSlotSection(slot);
		return _ini::RegDeleteString(section.c_str(), peruser);
	}

	TCHAR fullkeyname[256];
	StringCbPrintf(fullkeyname, 256, TEXT("Software\\AppleWin\\CurrentVersion\\%s"), REG_CONFIG);

	HKEY keyhandle;
	LSTATUS status = RegOpenKeyEx(
		(peruser ? HKEY_CURRENT_USER : HKEY_LOCAL_MACHINE),
		fullkeyname,
		0,
		KEY_READ,
		&keyhandle);
	if (status == ERROR_SUCCESS)
	{
		std::string& section = RegGetSlotSection(slot);
		LSTATUS status2 = RegDeleteKey(keyhandle, section.c_str());
		if (status2 != ERROR_SUCCESS && status2 != ERROR_FILE_NOT_FOUND)
			_ASSERT(0);
	}

	RegCloseKey(keyhandle);
}

void RegSetConfigSlotNewCardType(UINT slot, SS_CARDTYPE type)
{
	RegDeleteConfigSlotSection(slot);

	std::string regSection;
	regSection = RegGetConfigSlotSection(slot);

	RegSaveValue(regSection.c_str(), REGVALUE_CARD_TYPE, TRUE, type);
}
