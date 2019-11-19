// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/remote_commands/device_commands_factory_chromeos.h"

#include "base/logging.h"
#include "chrome/browser/chromeos/policy/device_cloud_policy_manager_chromeos.h"
#include "chrome/browser/chromeos/policy/remote_commands/crd_host_delegate.h"
#include "chrome/browser/chromeos/policy/remote_commands/device_command_fetch_status_job.h"
#include "chrome/browser/chromeos/policy/remote_commands/device_command_reboot_job.h"
#include "chrome/browser/chromeos/policy/remote_commands/device_command_refresh_machine_certificate_job.h"
#include "chrome/browser/chromeos/policy/remote_commands/device_command_remote_powerwash_job.h"
#include "chrome/browser/chromeos/policy/remote_commands/device_command_screenshot_job.h"
#include "chrome/browser/chromeos/policy/remote_commands/device_command_set_volume_job.h"
#include "chrome/browser/chromeos/policy/remote_commands/device_command_start_crd_session_job.h"
#include "chrome/browser/chromeos/policy/remote_commands/device_command_wipe_users_job.h"
#include "chrome/browser/chromeos/policy/remote_commands/screenshot_delegate.h"
#include "chromeos/dbus/power/power_manager_client.h"
#include "components/policy/core/common/remote_commands/remote_command_job.h"
#include "components/policy/proto/device_management_backend.pb.h"

namespace em = enterprise_management;

namespace policy {

DeviceCommandsFactoryChromeOS::DeviceCommandsFactoryChromeOS(
    DeviceCloudPolicyManagerChromeOS* policy_manager)
    : policy_manager_(policy_manager) {}

DeviceCommandsFactoryChromeOS::~DeviceCommandsFactoryChromeOS() = default;

std::unique_ptr<RemoteCommandJob>
DeviceCommandsFactoryChromeOS::BuildJobForType(em::RemoteCommand_Type type,
                                               RemoteCommandsService* service) {
  switch (type) {
    case em::RemoteCommand_Type_DEVICE_REBOOT:
      return std::make_unique<DeviceCommandRebootJob>(
          chromeos::PowerManagerClient::Get());
    case em::RemoteCommand_Type_DEVICE_SCREENSHOT:
      return std::make_unique<DeviceCommandScreenshotJob>(
          std::make_unique<ScreenshotDelegate>());
    case em::RemoteCommand_Type_DEVICE_SET_VOLUME:
      return std::make_unique<DeviceCommandSetVolumeJob>();
    case em::RemoteCommand_Type_DEVICE_START_CRD_SESSION:
      return std::make_unique<DeviceCommandStartCRDSessionJob>(
          GetCRDHostDelegate());
    case em::RemoteCommand_Type_DEVICE_FETCH_STATUS:
      return std::make_unique<DeviceCommandFetchStatusJob>();
    case em::RemoteCommand_Type_DEVICE_WIPE_USERS:
      return std::make_unique<DeviceCommandWipeUsersJob>(service);
    case em::RemoteCommand_Type_DEVICE_REFRESH_ENTERPRISE_MACHINE_CERTIFICATE:
      return std::make_unique<DeviceCommandRefreshMachineCertificateJob>(
          policy_manager_->GetMachineCertificateUploader());
    case em::RemoteCommand_Type_DEVICE_REMOTE_POWERWASH:
      return std::make_unique<DeviceCommandRemotePowerwashJob>(service);
    default:
      // Other types of commands should be sent to UserCommandsFactoryChromeOS
      // instead of here.
      NOTREACHED();
      return nullptr;
  }
}

CRDHostDelegate* DeviceCommandsFactoryChromeOS::GetCRDHostDelegate() {
  if (!crd_host_delegate_) {
    crd_host_delegate_ = std::make_unique<CRDHostDelegate>();
  }
  return crd_host_delegate_.get();
}

}  // namespace policy
