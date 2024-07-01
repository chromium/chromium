// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/almanac_api_client/device_info_manager.h"

#include <optional>
#include <string_view>

#include "base/files/file_util.h"
#include "base/functional/callback.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/apps/almanac_api_client/proto/client_context.pb.h"
#include "chrome/browser/apps/user_type_filter.h"
#include "chrome/browser/ash/arc/arc_util.h"
#include "chrome/browser/ash/borealis/borealis_features.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/channel_info.h"
#include "chromeos/ash/components/system/statistics_provider.h"
#include "chromeos/version/version_loader.h"
#include "components/language/core/browser/pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/version_info/version_info.h"

namespace {

apps::proto::ClientDeviceContext::Channel ConvertChannelTypeToProto(
    const version_info::Channel channel) {
  switch (channel) {
    case version_info::Channel::CANARY:
      return apps::proto::ClientDeviceContext::CHANNEL_CANARY;
    case version_info::Channel::DEV:
      return apps::proto::ClientDeviceContext::CHANNEL_DEV;
    case version_info::Channel::BETA:
      return apps::proto::ClientDeviceContext::CHANNEL_BETA;
    case version_info::Channel::STABLE:
      return apps::proto::ClientDeviceContext::CHANNEL_STABLE;
    case version_info::Channel::UNKNOWN:
      // The default channel is used for builds without a channel (e.g.
      // local builds).
      return apps::proto::ClientDeviceContext::CHANNEL_DEFAULT;
  }
}

apps::proto::ClientUserContext::UserType ConvertStringUserTypeToProto(
    const std::string& user_type) {
  if (user_type == apps::kUserTypeUnmanaged) {
    return apps::proto::ClientUserContext::USERTYPE_UNMANAGED;
  } else if (user_type == apps::kUserTypeManaged) {
    return apps::proto::ClientUserContext::USERTYPE_MANAGED;
  } else if (user_type == apps::kUserTypeChild) {
    return apps::proto::ClientUserContext::USERTYPE_CHILD;
  } else if (user_type == apps::kUserTypeGuest) {
    return apps::proto::ClientUserContext::USERTYPE_GUEST;
  } else if (user_type == apps::kUserTypeManagedGuest) {
    return apps::proto::ClientUserContext::USERTYPE_MANAGED_GUEST;
  }
  return apps::proto::ClientUserContext::USERTYPE_UNKNOWN;
}

// Adds version numbers and custom label tag to `info`, returning the updated
// object. Called on a background thread, since loading these values may block.
apps::DeviceInfo LoadVersionAndCustomLabel(apps::DeviceInfo info,
                                           bool arc_enabled) {
  info.version_info.ash_chrome = version_info::GetVersionNumber();
  std::optional<std::string> platform_version =
      chromeos::version_loader::GetVersion(
          chromeos::version_loader::VERSION_SHORT);
  info.version_info.platform = platform_version.value_or("");
  info.version_info.channel = chrome::GetChannel();
  // Only set arc_sdk if it is enabled.
  if (arc_enabled) {
    base::StringToInt(
        chromeos::version_loader::GetArcAndroidSdkVersion().value_or(
            std::string()),
        &info.version_info.arc_sdk);
  }

  // Load device identifiers from chromeos-config, as per
  // https://chromium.googlesource.com/chromiumos/platform2/+/HEAD/chromeos-config/README.md#identity.
  constexpr char kCustomLabelFileName[] =
      "/run/chromeos-config/v1/identity/custom-label-tag";
  constexpr char kCustomizationIdFileName[] =
      "/run/chromeos-config/v1/identity/customization-id";

  // Attempt to load the custom-label tag from kCustomLabelFileName first. Older
  // devices may instead have a "customization id" stored at
  // kCustomizationIdFileName. If neither file exists, the device has no
  // custom-label tag, and `info.custom_label_tag` should remain unset.
  std::string custom_label;
  if (base::ReadFileToString(base::FilePath(kCustomLabelFileName),
                             &custom_label)) {
    info.custom_label_tag = custom_label;
  } else if (base::ReadFileToString(base::FilePath(kCustomizationIdFileName),
                                    &custom_label)) {
    info.custom_label_tag = custom_label;
  }

  return info;
}

}  // namespace

namespace apps {

VersionInfo::VersionInfo() = default;

VersionInfo::VersionInfo(const VersionInfo& other) = default;

VersionInfo& VersionInfo::operator=(const VersionInfo& other) = default;

VersionInfo::~VersionInfo() = default;

DeviceInfo::DeviceInfo() = default;

DeviceInfo::DeviceInfo(const DeviceInfo& other) = default;

DeviceInfo& DeviceInfo::operator=(const DeviceInfo& other) = default;

DeviceInfo::~DeviceInfo() = default;

proto::ClientDeviceContext DeviceInfo::ToDeviceContext() const {
  proto::ClientDeviceContext device_context;

  device_context.set_board(board);
  device_context.set_model(model);
  device_context.set_channel(ConvertChannelTypeToProto(version_info.channel));
  device_context.mutable_versions()->set_chrome_ash(version_info.ash_chrome);
  device_context.mutable_versions()->set_chrome_os_platform(
      version_info.platform);
  device_context.mutable_versions()->set_arc_sdk(version_info.arc_sdk);
  device_context.mutable_versions()->set_steam_client(
      version_info.steam_client);
  device_context.set_hardware_id(hardware_id);
  if (custom_label_tag.has_value()) {
    device_context.set_custom_label_tag(custom_label_tag.value());
  }

  return device_context;
}

proto::ClientUserContext DeviceInfo::ToUserContext() const {
  proto::ClientUserContext user_context;

  user_context.set_language(locale);
  user_context.set_user_type(ConvertStringUserTypeToProto(user_type));

  return user_context;
}

DeviceInfoManager::DeviceInfoManager(Profile* profile) : profile_(profile) {}

DeviceInfoManager::~DeviceInfoManager() = default;

// This method populates:
//  - board
//  - user_type
//  - hardware_id
// The method then asynchronously populates:
//  - version_info and custom_label_tag (LoadVersionAndCustomLabel)
//  - model (OnModelInfo)
void DeviceInfoManager::GetDeviceInfo(
    base::OnceCallback<void(DeviceInfo)> callback) {
  if (cached_info_) {
    std::move(callback).Run(*cached_info_);
    return;
  }

  bool is_first_get = pending_callbacks_.empty();
  pending_callbacks_.push_back(std::move(callback));

  if (!is_first_get) {
    return;
  }

  // Start loading the Device Info so that we can cache it and deliver to
  // pending callbacks.

  DeviceInfo device_info;

  device_info.board = base::ToLowerASCII(base::SysInfo::HardwareModelName());
  device_info.user_type = apps::DetermineUserType(profile_);

  ash::system::StatisticsProvider* provider =
      ash::system::StatisticsProvider::GetInstance();
  std::optional<std::string_view> hwid =
      provider->GetMachineStatistic(ash::system::kHardwareClassKey);
  device_info.hardware_id = std::string(hwid.value_or(""));
  // Set steam_client to "TRUE" to indicate it is enabled.
  if (borealis::BorealisFeatures(profile_).IsEnabled()) {
    device_info.version_info.steam_client = "TRUE";
  }

  // Locale
  PrefService* prefs = profile_->GetPrefs();
  DCHECK(prefs);
  device_info.locale = prefs->GetString(language::prefs::kApplicationLocale);
  // If there's no stored locale preference, fall back to the current UI
  // language.
  if (device_info.locale.empty()) {
    device_info.locale = g_browser_process->GetApplicationLocale();
  }

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock()},
      base::BindOnce(&LoadVersionAndCustomLabel, std::move(device_info),
                     arc::IsArcPlayStoreEnabledForProfile(profile_)),
      base::BindOnce(&DeviceInfoManager::OnLoadedVersionAndCustomLabel,
                     weak_ptr_factory_.GetWeakPtr()));
}

void DeviceInfoManager::OnLoadedVersionAndCustomLabel(DeviceInfo device_info) {
  base::SysInfo::GetHardwareInfo(base::BindOnce(&DeviceInfoManager::OnModelInfo,
                                                weak_ptr_factory_.GetWeakPtr(),
                                                std::move(device_info)));
}

void DeviceInfoManager::OnModelInfo(DeviceInfo device_info,
                                    base::SysInfo::HardwareInfo hardware_info) {
  device_info.model = hardware_info.model;

  cached_info_ = std::move(device_info);

  for (auto& callback : std::exchange(pending_callbacks_, {})) {
    std::move(callback).Run(*cached_info_);
  }
}

std::ostream& operator<<(std::ostream& os, const DeviceInfo& device_info) {
  os << "Device Info: " << std::endl;
  os << "- Board: " << device_info.board << std::endl;
  os << "- Model: " << device_info.model << std::endl;
  os << "- Hardware ID: " << device_info.hardware_id << std::endl;
  os << "- Custom Label: " << device_info.custom_label_tag.value_or("(empty)")
     << std::endl;
  os << "- User Type: " << device_info.user_type << std::endl;
  os << "- Locale: " << device_info.locale << std::endl;
  os << device_info.version_info;
  return os;
}

std::ostream& operator<<(std::ostream& os, const VersionInfo& version_info) {
  os << "- Version Info: " << std::endl;
  os << "  - Ash Chrome: " << version_info.ash_chrome << std::endl;
  os << "  - Platform: " << version_info.platform << std::endl;
  os << "  - ARC SDK: " << version_info.arc_sdk << std::endl;
  os << "  - Steam client: " << version_info.steam_client << std::endl;
  os << "  - Channel: " << version_info::GetChannelString(version_info.channel)
     << std::endl;
  return os;
}

}  // namespace apps
