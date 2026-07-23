// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/remote_commands/device_commands_factory_ash.h"

#include <memory>

#include "base/check.h"
#include "base/check_deref.h"
#include "base/notreached.h"
#include "chrome/browser/ash/policy/core/browser_policy_connector_ash.h"
#include "chrome/browser/ash/policy/core/device_cloud_policy_manager_ash.h"
#include "chrome/browser/ash/policy/remote_commands/crd/device_command_fetch_crd_availability_info_job.h"
#include "chrome/browser/ash/policy/remote_commands/crd/device_command_start_crd_session_job.h"
#include "chrome/browser/ash/policy/remote_commands/device_command_fetch_status_job.h"
#include "chrome/browser/ash/policy/remote_commands/device_command_fetch_support_packet_job.h"
#include "chrome/browser/ash/policy/remote_commands/device_command_get_available_routines_job.h"
#include "chrome/browser/ash/policy/remote_commands/device_command_get_routine_update_job.h"
#include "chrome/browser/ash/policy/remote_commands/device_command_query_geolocation_job.h"
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
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "components/policy/core/common/remote_commands/remote_command_job.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace policy {

using enterprise_management::RemoteCommand;

bool DeviceCommandsFactoryAsh::device_commands_test_ = false;

DeviceCommandsFactoryAsh::DeviceCommandsFactoryAsh(
    PrefService* local_state,
    scoped_refptr<network::SharedURLLoaderFactory> shared_url_loader_factory,
    ash::attestation::MachineCertificateUploader* certificate_uploader,
    StartCrdSessionJobDelegate& crd_delegate)
    : local_state_(CHECK_DEREF(local_state)),
      shared_url_loader_factory_(std::move(shared_url_loader_factory)),
      machine_certificate_uploader_(certificate_uploader),
      crd_delegate_(crd_delegate) {
  CHECK(shared_url_loader_factory_);
}

DeviceCommandsFactoryAsh::~DeviceCommandsFactoryAsh() = default;

std::unique_ptr<RemoteCommandJob> DeviceCommandsFactoryAsh::BuildJobForType(
    RemoteCommand::Type type,
    RemoteCommandsService* service) {
  // TODO(crbug.com/404133022): Inject global objects into this class.
  BrowserPolicyConnectorAsh* browser_policy_connector_ash =
      g_browser_process->platform_part()->browser_policy_connector_ash();

  switch (type) {
    case RemoteCommand::DEVICE_REBOOT:
      return std::make_unique<DeviceCommandRebootJob>();
    case RemoteCommand::DEVICE_SCREENSHOT:
      return std::make_unique<DeviceCommandScreenshotJob>(
          CreateScreenshotDelegate(shared_url_loader_factory_,
                                   browser_policy_connector_ash));
    case RemoteCommand::DEVICE_SET_VOLUME:
      return std::make_unique<DeviceCommandSetVolumeJob>();
    case RemoteCommand::DEVICE_START_CRD_SESSION:
      return std::make_unique<DeviceCommandStartCrdSessionJob>(
          &local_state_.get(), *crd_delegate_);
    case RemoteCommand::DEVICE_FETCH_STATUS:
      return std::make_unique<DeviceCommandFetchStatusJob>(
          browser_policy_connector_ash);
    case RemoteCommand::DEVICE_WIPE_USERS:
      return std::make_unique<DeviceCommandWipeUsersJob>(&local_state_.get(),
                                                         service);
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
      return std::make_unique<DeviceCommandFetchCrdAvailabilityInfoJob>(
          &local_state_.get());
    case RemoteCommand::FETCH_SUPPORT_PACKET:
      return std::make_unique<DeviceCommandFetchSupportPacketJob>();
    case RemoteCommand::QUERY_GEOLOCATION: {
      DeviceCloudPolicyManagerAsh& policy_manager = CHECK_DEREF(
          browser_policy_connector_ash->GetDeviceCloudPolicyManager());
      return std::make_unique<DeviceCommandQueryGeolocationJob>(
          &local_state_.get(), policy_manager.core()->store());
    }

    case RemoteCommand::COMMAND_ECHO_TEST:
    case RemoteCommand::USER_ARC_COMMAND:
    case RemoteCommand::BROWSER_CLEAR_BROWSING_DATA:
    case RemoteCommand::BROWSER_ROTATE_ATTESTATION_CREDENTIAL:
    case RemoteCommand::BROWSER_EXTENSION_UPDATE_CHECK:
      // These types of commands should be sent to `UserCommandsFactoryAsh`
      // instead of here.
      NOTREACHED();
  }
}

void DeviceCommandsFactoryAsh::set_commands_for_testing(
    bool device_commands_test) {
  device_commands_test_ = device_commands_test;
}

std::unique_ptr<DeviceCommandScreenshotJob::Delegate>
DeviceCommandsFactoryAsh::CreateScreenshotDelegate(
    scoped_refptr<network::SharedURLLoaderFactory> shared_url_loader_factory,
    BrowserPolicyConnectorAsh* browser_policy_connector_ash) {
  if (device_commands_test_) {
    return std::make_unique<FakeScreenshotDelegate>();
  }
  return std::make_unique<ScreenshotDelegate>(
      std::move(shared_url_loader_factory), browser_policy_connector_ash);
}

}  // namespace policy
