// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/display/quirks_manager_delegate_impl.h"

#include "ash/constants/ash_paths.h"
#include "base/path_service.h"
#include "base/system/sys_info.h"
#include "chrome/common/chrome_paths.h"
#include "chromeos/ash/components/settings/cros_settings.h"
#include "google_apis/google_api_keys.h"

namespace {

const char kUserDataDisplayProfilesDirectory[] = "display_profiles";

}  // namespace

namespace quirks {

std::string QuirksManagerDelegateImpl::GetApiKey() const {
  return google_apis::GetAPIKey();
}

// On chrome device, returns /var/cache/display_profiles.
// On Linux desktop, returns {DIR_USER_DATA}/display_profiles.
base::FilePath QuirksManagerDelegateImpl::GetDisplayProfileDirectory() const {
  base::FilePath directory;
  if (base::SysInfo::IsRunningOnChromeOS()) {
    base::PathService::Get(ash::DIR_DEVICE_DISPLAY_PROFILES, &directory);
  } else {
    base::PathService::Get(chrome::DIR_USER_DATA, &directory);
    directory = directory.Append(kUserDataDisplayProfilesDirectory);
  }
  return directory;
}

bool QuirksManagerDelegateImpl::DevicePolicyEnabled() const {
  bool quirks_enabled = true;
  ash::CrosSettings::Get()->GetBoolean(ash::kDeviceQuirksDownloadEnabled,
                                       &quirks_enabled);
  return quirks_enabled;
}

}  // namespace quirks
