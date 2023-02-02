// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_preload_service/device_info_manager.h"

#include "base/functional/callback.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/apps/user_type_filter.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/channel_info.h"
#include "chromeos/version/version_loader.h"
#include "components/language/core/browser/pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/version_info/version_info.h"

namespace apps {

DeviceInfo::DeviceInfo() = default;

DeviceInfo::DeviceInfo(const DeviceInfo& other) = default;

DeviceInfo& DeviceInfo::operator=(const DeviceInfo& other) = default;

DeviceInfo::~DeviceInfo() = default;

DeviceInfoManager::DeviceInfoManager(Profile* profile) : profile_(profile) {}

DeviceInfoManager::~DeviceInfoManager() = default;

// This method populates:
//  - board
//  - version_info.ash_chrome
//  - user_type
//  - channel
// The method then asynchronously populates:
//  - version_info.platform (OnPlatformVersionNumber)
//  - model (OnModelInfo)
void DeviceInfoManager::GetDeviceInfo(
    base::OnceCallback<void(DeviceInfo)> callback) {
  if (device_info_ != absl::nullopt) {
    std::move(callback).Run(device_info_.value());
    return;
  }

  DeviceInfo device_info;

  device_info.board = base::SysInfo::HardwareModelName();
  device_info.version_info.ash_chrome = version_info::GetVersionNumber();
  device_info.user_type = apps::DetermineUserType(profile_);
  device_info.version_info.channel = chrome::GetChannel();

  // Locale
  PrefService* prefs = profile_->GetPrefs();
  DCHECK(prefs);
  device_info.locale = prefs->GetString(language::prefs::kApplicationLocale);

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock()},
      base::BindOnce(&chromeos::version_loader::GetVersion,
                     chromeos::version_loader::VERSION_SHORT),
      base::BindOnce(&DeviceInfoManager::OnPlatformVersionNumber,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback),
                     device_info));
}

void DeviceInfoManager::OnPlatformVersionNumber(
    base::OnceCallback<void(DeviceInfo)> callback,
    DeviceInfo device_info,
    const absl::optional<std::string>& version) {
  device_info.version_info.platform = version.value_or("unknown");
  base::SysInfo::GetHardwareInfo(base::BindOnce(
      &DeviceInfoManager::OnModelInfo, weak_ptr_factory_.GetWeakPtr(),
      std::move(callback), device_info));
}

void DeviceInfoManager::OnModelInfo(
    base::OnceCallback<void(DeviceInfo)> callback,
    DeviceInfo device_info,
    base::SysInfo::HardwareInfo hardware_info) {
  device_info.model = hardware_info.model;
  device_info_ = device_info;
  std::move(callback).Run(device_info);
}

std::ostream& operator<<(std::ostream& os, const DeviceInfo& device_info) {
  os << "Device Info: " << std::endl;
  os << "- Board: " << device_info.board << std::endl;
  os << "- Model: " << device_info.model << std::endl;
  os << "- User Type: " << device_info.user_type << std::endl;
  os << "- Locale: " << device_info.locale << std::endl;
  os << device_info.version_info;
  return os;
}

std::ostream& operator<<(std::ostream& os, const VersionInfo& version_info) {
  os << "- Version Info: " << std::endl;
  os << "  - Ash Chrome: " << version_info.ash_chrome << std::endl;
  os << "  - Platform: " << version_info.platform << std::endl;
  os << "  - Channel: " << version_info::GetChannelString(version_info.channel)
     << std::endl;
  return os;
}

}  // namespace apps
