// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/remote_commands/device_commands_factory_ash.h"

#include <memory>

#include "base/notreached.h"
#include "chrome/browser/ash/policy/core/device_cloud_policy_manager_ash.h"
#include "chrome/browser/ash/policy/remote_commands/crd/device_command_fetch_crd_availability_info_job.h"
#include "chrome/browser/ash/policy/remote_commands/crd/device_command_start_crd_session_job.h"
#include "chrome/browser/ash/policy/remote_commands/device_command_fetch_status_job.h"
#include "chrome/browser/ash/policy/remote_commands/device_command_fetch_support_packet_job.h"
#include "chrome/browser/ash/policy/remote_commands/device_command_get_available_routines_job.h"
#include "chrome/browser/ash/policy/remote_commands/device_command_get_routine_update_job.h"
#include "chrome/browser/ash/policy/remote_commands/device_command_reboot_job.h"
#include "chrome/browser/ash/policy/remote_commands/device_command_refresh_machine_certificate_job.h"
#include "chrome/browser/ash/policy/remote_commands/device_command_remote_powerwash_job.h"
#include "chrome/browser/ash/policy/remote_commands/device_command_reset_euicc_job.h"
#include "chrome/browser/ash/policy/remote_commands/device_command_run_routine_job.h"
#include "chrome/browser/ash/policy/remote_commands/device_command_screenshot_job.h"
#include "chrome/browser/ash/policy/remote_commands/device_command_set_volume_job.h"
#include "chrome/browser/ash/policy/remote_commands/device_command_wipe_users_job.h"
#include "chrome/browser/ash/policy/remote_commands/fake_screenshot_delegate.h"
#include "chrome/browser/ash/policy/remote_commands/screenshot_delegate.h"
#include "components/policy/core/common/remote_commands/remote_command_job.h"
#include "components/policy/proto/device_management_backend.pb.h"

namespace policy {

using enterprise_management::RemoteCommand;

bool DeviceCommandsFactoryAsh::device_commands_test_ = false;

DeviceCommandsFactoryAsh::DeviceCommandsFactoryAsh(
    ash::attestation::MachineCertificateUploader* certificate_uploader,
    StartCrdSessionJobDelegate& crd_delegate)
    : machine_certificate_uploader_(certificate_uploader),
      crd_delegate_(crd_delegate) {}

DeviceCommandsFactoryAsh::~DeviceCommandsFactoryAsh() = default;

std::unique_ptr<RemoteCommandJob> DeviceCommandsFactoryAsh::BuildJobForType(
    RemoteCommand::Type type,
    RemoteCommandsService* service) {
  switch (type) {
    case RemoteCommand::DEVICE_REBOOT:
      return std::make_unique<DeviceCommandRebootJob>();
    case RemoteCommand::DEVICE_SCREENSHOT:
      return std::make_unique<DeviceCommandScreenshotJob>(
          CreateScreenshotDelegate());
    case RemoteCommand::DEVICE_SET_VOLUME:
      return std::make_unique<DeviceCommandSetVolumeJob>();
    case RemoteCommand::DEVICE_START_CRD_SESSION:
      return std::make_unique<DeviceCommandStartCrdSessionJob>(*crd_delegate_);
    case RemoteCommand::DEVICE_FETCH_STATUS:
      return std::make_unique<DeviceCommandFetchStatusJob>();
    case RemoteCommand::DEVICE_WIPE_USERS:
      return std::make_unique<DeviceCommandWipeUsersJob>(service);
    case RemoteCommand::DEVICE_REFRESH_ENTERPRISE_MACHINE_CERTIFICATE:
      return std::make_unique<DeviceCommandRefreshMachineCertificateJob>(
          machine_certificate_uploader_);
    case RemoteCommand::DEVICE_REMOTE_POWERWASH:
      return std::make_unique<DeviceCommandRemotePowerwashJob>(service);
    case RemoteCommand::DEVICE_GET_AVAILABLE_DIAGNOSTIC_ROUTINES:
      return std::make_unique<DeviceCommandGetAvailableRoutinesJob>();
    case RemoteCommand::DEVICE_RUN_DIAGNOSTIC_ROUTINE:
      return std::make_unique<DeviceCommandRunRoutineJob>();
    case RemoteCommand::DEVICE_GET_DIAGNOSTIC_ROUTINE_UPDATE:
      return std::make_unique<DeviceCommandGetRoutineUpdateJob>();
    case RemoteCommand::DEVICE_RESET_EUICC:
      return std::make_unique<DeviceCommandResetEuiccJob>();
    case RemoteCommand::FETCH_CRD_AVAILABILITY_INFO:
      return std::make_unique<DeviceCommandFetchCrdAvailabilityInfoJob>();
    case RemoteCommand::FETCH_SUPPORT_PACKET:
      return std::make_unique<DeviceCommandFetchSupportPacketJob>();

    case RemoteCommand::COMMAND_ECHO_TEST:
    case RemoteCommand::USER_ARC_COMMAND:
    case RemoteCommand::BROWSER_CLEAR_BROWSING_DATA:
    case RemoteCommand::BROWSER_ROTATE_ATTESTATION_CREDENTIAL:
      // These types of commands should be sent to `UserCommandsFactoryAsh`
      // instead of here.
      NOTREACHED_IN_MIGRATION();
      return nullptr;
  }
}

void DeviceCommandsFactoryAsh::set_commands_for_testing(
    bool device_commands_test) {
  device_commands_test_ = device_commands_test;
}

std::unique_ptr<DeviceCommandScreenshotJob::Delegate>
DeviceCommandsFactoryAsh::CreateScreenshotDelegate() {
  if (device_commands_test_) {
    return std::make_unique<FakeScreenshotDelegate>();
  }
  return std::make_unique<ScreenshotDelegate>();
}

}  // namespace policy
