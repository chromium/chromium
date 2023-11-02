// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// State is stored in the Windows registry in a value of the key
// HKEY_CURRENT_USER\Software\<browser>\IncidentsSent for each profile. The
// value names are profile directory BaseNames.

#include "chrome/browser/safe_browsing/incident_reporting/platform_state_store.h"

#include <windows.h>

#include "base/files/file_path.h"
#include "base/numerics/safe_conversions.h"
#include "base/win/registry.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/install_static/install_util.h"

namespace safe_browsing {
namespace platform_state_store {

namespace {

// Returns the path to the registry key holding profile-specific state values.
std::wstring GetStateStoreKeyName() {
  return install_static::GetRegistryPath().append(L"\\IncidentsSent");
}

// Returns the name of the registry value for |profile|'s state.
std::wstring GetValueNameForProfile(Profile* profile) {
  return profile->GetBaseName().value();
}

// Clears |profile|'s state.
PlatformStateStoreLoadResult ClearStoreData(Profile* profile) {
  base::win::RegKey key;

  if (key.Open(HKEY_CURRENT_USER, GetStateStoreKeyName().c_str(),
               KEY_QUERY_VALUE | KEY_SET_VALUE | KEY_WOW64_32KEY) ==
      ERROR_SUCCESS) {
    std::wstring value_name(GetValueNameForProfile(profile));
    if (key.HasValue(value_name.c_str())) {
      if (key.DeleteValue(value_name.c_str()) == ERROR_SUCCESS)
        return PlatformStateStoreLoadResult::CLEARED_DATA;
      return PlatformStateStoreLoadResult::DATA_CLEAR_FAILED;
    }
  }
  return PlatformStateStoreLoadResult::CLEARED_NO_DATA;
}

}  // namespace

PlatformStateStoreLoadResult ReadStoreData(Profile* profile,
                                           std::string* data) {
  // Clear any old state if this is a new profile.
  if (profile->IsNewProfile()) {
    data->clear();
    return ClearStoreData(profile);
  }

  std::wstring value_name(GetValueNameForProfile(profile));
  base::win::RegKey key;
  if (key.Open(HKEY_CURRENT_USER, GetStateStoreKeyName().c_str(),
               KEY_QUERY_VALUE | KEY_WOW64_32KEY) == ERROR_SUCCESS) {
    while (true) {
      void* buffer = data->empty() ? nullptr : &(*data)[0];
      DWORD buffer_size = base::saturated_cast<DWORD>(data->size());
      LONG result =
          key.ReadValue(value_name.c_str(), buffer, &buffer_size, nullptr);
      // Trim the output string and return if all data was read.
      if (result == ERROR_SUCCESS && buffer_size <= data->size()) {
        data->resize(buffer_size);
        return PlatformStateStoreLoadResult::SUCCESS;
      }
      // Increase the buffer and retry if more space was needed. Otherwise bail.
      if (buffer_size > data->size())
        data->resize(buffer_size);
      else
        return PlatformStateStoreLoadResult::READ_FAILED;
    }
  }

  return PlatformStateStoreLoadResult::OPEN_FAILED;
}

void WriteStoreData(Profile* profile, const std::string& data) {
  base::win::RegKey key;
  if (key.Create(HKEY_CURRENT_USER, GetStateStoreKeyName().c_str(),
                 KEY_SET_VALUE | KEY_WOW64_32KEY) == ERROR_SUCCESS) {
    if (data.empty()) {
      key.DeleteValue(GetValueNameForProfile(profile).c_str());
    } else {
      key.WriteValue(GetValueNameForProfile(profile).c_str(), &data[0],
                     base::saturated_cast<DWORD>(data.size()),
                     REG_BINARY);
    }
  }
}

}  // namespace platform_state_store
}  // namespace safe_browsing
