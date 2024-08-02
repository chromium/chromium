// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/browser/ash/login/version_info_updater.h"

#include <string_view>
#include <vector>

#include "ash/constants/ash_features.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/system/sys_info.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/ash/policy/core/browser_policy_connector_ash.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/grit/branded_strings.h"
#include "chrome/grit/generated_resources.h"
#include "chromeos/ash/components/settings/cros_settings.h"
#include "chromeos/ash/components/settings/cros_settings_names.h"
#include "chromeos/ash/components/system/statistics_provider.h"
#include "chromeos/strings/grit/chromeos_strings.h"
#include "chromeos/version/version_loader.h"
#include "components/version_info/version_info.h"
#include "device/bluetooth/bluetooth_adapter.h"
#include "device/bluetooth/bluetooth_adapter_factory.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/chromeos/devicetype_utils.h"

namespace ash {
namespace {

const char* const kReportingFlags[] = {
    kReportDeviceVersionInfo, kReportDeviceActivityTimes, kReportDeviceBootMode,
    kReportDeviceLocation, kDeviceLoginScreenSystemInfoEnforced};

// Strings used to generate the serial number part of the version string.
const char kSerialNumberPrefix[] = "SN:";

// Strings used to generate the ZTE info string. The mark after "ZTE" indicates
// that the device is ready for zero-touch enrollment as far as it can tell.
const char kZteReady[] = "ZTE\xF0\x9F\x97\xB9";
const char kAttestedDeviceIdPrefix[] = "ADID:";

// Strings used to generate the bluetooth device name.
const char kBluetoothDeviceNamePrefix[] = "Bluetooth device name: ";

}  // namespace

///////////////////////////////////////////////////////////////////////////////
// VersionInfoUpdater public:

VersionInfoUpdater::VersionInfoUpdater(Delegate* delegate)
    : cros_settings_(CrosSettings::Get()), delegate_(delegate) {}

VersionInfoUpdater::~VersionInfoUpdater() {
  policy::BrowserPolicyConnectorAsh* connector =
      g_browser_process->platform_part()->browser_policy_connector_ash();
  policy::DeviceCloudPolicyManagerAsh* policy_manager =
      connector->GetDeviceCloudPolicyManager();
  if (policy_manager)
    policy_manager->core()->store()->RemoveObserver(this);
}

void VersionInfoUpdater::StartUpdate(bool is_chrome_branded) {
  if (base::SysInfo::IsRunningOnChromeOS()) {
    base::ThreadPool::PostTaskAndReplyWithResult(
        FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_VISIBLE},
        base::BindOnce(&chromeos::version_loader::GetVersion,
                       is_chrome_branded
                           ? chromeos::version_loader::VERSION_SHORT_WITH_DATE
                           : chromeos::version_loader::VERSION_FULL),
        base::BindOnce(&VersionInfoUpdater::OnVersion,
                       weak_pointer_factory_.GetWeakPtr()));
  } else {
    OnVersion("linux-chromeos");
  }

  policy::BrowserPolicyConnectorAsh* connector =
      g_browser_process->platform_part()->browser_policy_connector_ash();
  policy::DeviceCloudPolicyManagerAsh* policy_manager =
      connector->GetDeviceCloudPolicyManager();
  if (policy_manager) {
    if (!policy_manager->core()->store()->HasObserver(this)) {
      policy_manager->core()->store()->AddObserver(this);
    }

    // Ensure that we have up-to-date enterprise info in case enterprise policy
    // is already fetched and has finished initialization.
    UpdateEnterpriseInfo();
  }

  // Watch for changes to the reporting flags.
  auto callback = base::BindRepeating(&VersionInfoUpdater::UpdateEnterpriseInfo,
                                      base::Unretained(this));
  for (unsigned int i = 0; i < std::size(kReportingFlags); ++i) {
    subscriptions_.push_back(
        cros_settings_->AddSettingsObserver(kReportingFlags[i], callback));
  }

  // Update device bluetooth info.
  device::BluetoothAdapterFactory::Get()->GetAdapter(base::BindOnce(
      &VersionInfoUpdater::OnGetAdapter, weak_pointer_factory_.GetWeakPtr()));

  // Get ADB sideloading status if supported on device. Otherwise, default is to
  // not show.
  if (base::FeatureList::IsEnabled(features::kArcAdbSideloadingFeature)) {
    SessionManagerClient* client = SessionManagerClient::Get();
    client->QueryAdbSideload(
        base::BindOnce(&VersionInfoUpdater::OnQueryAdbSideload,
                       weak_pointer_factory_.GetWeakPtr()));
  }
}

std::optional<bool> VersionInfoUpdater::IsSystemInfoEnforced() const {
  bool is_system_info_enforced = false;
  if (cros_settings_->GetBoolean(kDeviceLoginScreenSystemInfoEnforced,
                                 &is_system_info_enforced)) {
    return is_system_info_enforced;
  }
  return std::nullopt;
}

void VersionInfoUpdater::UpdateVersionLabel() {
  if (!version_text_.has_value())
    return;

  std::string label_text = l10n_util::GetStringFUTF8(
      IDS_LOGIN_VERSION_LABEL_FORMAT,
      l10n_util::GetStringUTF16(IDS_PRODUCT_NAME),
      base::UTF8ToUTF16(version_info::GetVersionNumber()),
      base::UTF8ToUTF16(version_text_.value()),
      base::UTF8ToUTF16(GetDeviceIdsLabel()));

  if (delegate_)
    delegate_->OnOSVersionLabelTextUpdated(label_text);
}

void VersionInfoUpdater::UpdateEnterpriseInfo() {
  policy::BrowserPolicyConnectorAsh* connector =
      g_browser_process->platform_part()->browser_policy_connector_ash();
  SetEnterpriseInfo(connector->GetEnterpriseDomainManager(),
                    connector->GetDeviceAssetID());
}

void VersionInfoUpdater::SetEnterpriseInfo(
    const std::string& enterprise_manager,
    const std::string& asset_id) {
  // Update the notification about device status reporting.
  if (delegate_ && !enterprise_manager.empty()) {
    std::string enterprise_info;
    enterprise_info = l10n_util::GetStringFUTF8(
        IDS_ASH_ENTERPRISE_DEVICE_MANAGED_BY, ui::GetChromeOSDeviceName(),
        base::UTF8ToUTF16(enterprise_manager));
    delegate_->OnEnterpriseInfoUpdated(enterprise_info, asset_id);
  }
}

std::string VersionInfoUpdater::GetDeviceIdsLabel() {
  std::string device_ids_text;

  // Get the attested device ID and add the ZTE indication and the ID if needed.
  const std::optional<std::string_view> attested_device_id =
      system::StatisticsProvider::GetInstance()->GetMachineStatistic(
          system::kAttestedDeviceIdKey);
  // Start with the ZTE indication and the attested device ID if it exists.
  if (attested_device_id && !attested_device_id->empty()) {
    device_ids_text.append(kZteReady);
    // Always append the attested device ID.
    device_ids_text.append(" ");
    device_ids_text.append(kAttestedDeviceIdPrefix);
    device_ids_text.append(std::string(attested_device_id.value()));
  }

  // Get the serial number and add it.
  const std::optional<std::string_view> serial_number =
      system::StatisticsProvider::GetInstance()->GetMachineID();
  if (serial_number && !serial_number->empty()) {
    if (!device_ids_text.empty())
      device_ids_text.append(" ");
    // Append the serial number.
    device_ids_text.append(kSerialNumberPrefix);
    device_ids_text.append(std::string(serial_number.value()));
  }

  return device_ids_text;
}
void VersionInfoUpdater::OnVersion(const std::optional<std::string>& version) {
  version_text_ = version;
  UpdateVersionLabel();
}

void VersionInfoUpdater::OnGetAdapter(
    scoped_refptr<device::BluetoothAdapter> adapter) {
  if (delegate_ && adapter->IsDiscoverable() && !adapter->GetName().empty()) {
    delegate_->OnDeviceInfoUpdated(kBluetoothDeviceNamePrefix +
                                   adapter->GetName());
  }
}

void VersionInfoUpdater::OnStoreLoaded(policy::CloudPolicyStore* store) {
  UpdateEnterpriseInfo();
}

void VersionInfoUpdater::OnStoreError(policy::CloudPolicyStore* store) {
  UpdateEnterpriseInfo();
}

void VersionInfoUpdater::OnQueryAdbSideload(
    SessionManagerClient::AdbSideloadResponseCode response_code,
    bool enabled) {
  switch (response_code) {
    case SessionManagerClient::AdbSideloadResponseCode::SUCCESS:
      break;
    case SessionManagerClient::AdbSideloadResponseCode::FAILED:
      // Pretend to be enabled to show warning at login screen conservatively.
      LOG(WARNING) << "Failed to query adb sideload status";
      enabled = true;
      break;
    case SessionManagerClient::AdbSideloadResponseCode::NEED_POWERWASH:
      // This can only happen on device initialized before M74, i.e. not
      // powerwashed since then. Treat it as powerwash disabled to not show the
      // message.
      enabled = false;
      break;
  }

  if (delegate_)
    delegate_->OnAdbSideloadStatusUpdated(enabled);
}

}  // namespace ash
