// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/device_trust/key_management/installer/management_service/chrome_management_service.h"

#include <grp.h>
#include <sys/stat.h>
#include <unistd.h>

#include <cstdint>
#include <utility>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/memory/scoped_refptr.h"
#include "base/path_service.h"
#include "base/syslog_logging.h"
#include "chrome/browser/enterprise/connectors/device_trust/key_management/core/network/mojo_key_network_delegate.h"
#include "chrome/browser/enterprise/connectors/device_trust/key_management/core/shared_command_constants.h"
#include "chrome/browser/enterprise/connectors/device_trust/key_management/installer/key_rotation_manager.h"
#include "chrome/browser/enterprise/connectors/device_trust/key_management/installer/management_service/metrics_utils.h"
#include "chrome/browser/enterprise/connectors/device_trust/key_management/installer/management_service/mojo_helper/mojo_helper.h"
#include "chrome/browser/enterprise/connectors/device_trust/key_management/installer/management_service/rotate_util.h"
#include "chrome/common/channel_info.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/platform/platform_channel.h"
#include "mojo/public/cpp/system/invitation.h"
#include "mojo/public/cpp/system/message_pipe.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"

namespace enterprise_connectors {

namespace {

// Records an UMA metric when a failure occurs and logs a failure error message.
enterprise_connectors::Status RecordFailure(ManagementServiceError error,
                                            const std::string& log_message) {
  RecordError(error);
  SYSLOG(ERROR) << log_message;
  return kFailure;
}

// Verifies group permissions for the chrome-management-service binary.
bool CheckBinaryPermissions() {
  base::FilePath exe_path;
  if (!base::PathService::Get(base::DIR_EXE, &exe_path)) {
    RecordFailure(ManagementServiceError::kFilePathResolutionFailure,
                  "The chrome-management-service failed. Could not get the "
                  "path to the chrome-management-service.");
    return false;
  }
  exe_path = exe_path.Append(constants::kBinaryFileName);

  // Gets the chromemgmt group gid stored on the device.
  struct group* chrome_mgmt_group = getgrnam(constants::kGroupName);
  if (!chrome_mgmt_group) {
    RecordFailure(ManagementServiceError::kManagementGroupIdDoesNotExist,
                  "The chrome-management-service failed. Device missing the "
                  "necessary group permissions to run the command.");
    return false;
  }
  gid_t chrome_mgmt_gid = chrome_mgmt_group->gr_gid;

  // Gets the gid of the chrome-management-service binary file.
  struct stat st;
  stat(exe_path.value().c_str(), &st);
  gid_t binary_gid = st.st_gid;

  if (getegid() != chrome_mgmt_gid || binary_gid != chrome_mgmt_gid) {
    RecordFailure(ManagementServiceError::kBinaryMissingManagementGroupID,
                  "The chrome-management-service failed. Incorrect permissions "
                  "for the chrome-management-service.");
    return false;
  }
  return true;
}

int KeyRotationResultToExitCode(KeyRotationResult result) {
  switch (result) {
    case KeyRotationResult::kSucceeded:
      return kSuccess;
    case KeyRotationResult::kFailed:
      return kFailure;
    case KeyRotationResult::kInsufficientPermissions:
      return kFailedInsufficientPermissions;
    case KeyRotationResult::kFailedKeyConflict:
      return kFailedKeyConflict;
  }
}

}  // namespace

ChromeManagementService::ChromeManagementService()
    : permissions_callback_(base::BindOnce(&CheckBinaryPermissions)),
      rotation_callback_(base::BindOnce(&ChromeManagementService::StartRotation,
                                        base::Unretained(this))),
      mojo_helper_(MojoHelper::Create()) {}

ChromeManagementService::ChromeManagementService(
    PermissionsCallback permissions_callback,
    RotationCallback rotation_callback,
    std::unique_ptr<MojoHelper> mojo_helper)
    : permissions_callback_(std::move(permissions_callback)),
      rotation_callback_(std::move(rotation_callback)),
      mojo_helper_(std::move(mojo_helper)) {
  DCHECK(permissions_callback_);
  DCHECK(rotation_callback_);
  DCHECK(mojo_helper_);
}

ChromeManagementService::~ChromeManagementService() = default;

int ChromeManagementService::Run(const base::CommandLine* command_line,
                                 uint64_t pipe_name) {
  if (!command_line || !command_line->HasSwitch(switches::kRotateDTKey)) {
    return RecordFailure(
        ManagementServiceError::kCommandMissingRotateDTKey,
        "Device trust key rotation failed. Command missing rotate key switch.");
  }

  if (!std::move(permissions_callback_).Run()) {
    return kFailedInsufficientPermissions;
  }

  auto platform_channel_endpoint =
      mojo_helper_->GetEndpointFromCommandLine(*command_line);
  if (!platform_channel_endpoint.is_valid()) {
    return RecordFailure(
        ManagementServiceError::kInvalidPlatformChannelEndpoint,
        "Device trust key rotation failed. Invalid platform channel endpoint "
        "in command line.");
  }

  mojo::IncomingInvitation invitation =
      mojo_helper_->AcceptMojoInvitation(std::move(platform_channel_endpoint));
  if (!invitation.is_valid()) {
    return RecordFailure(
        ManagementServiceError::kInvalidMojoInvitation,
        "Device trust key rotation failed. The mojo invitation is invalid.");
  }

  mojo::ScopedMessagePipeHandle pipe =
      mojo_helper_->ExtractMojoMessage(std::move(invitation), pipe_name);
  if (!pipe.is_valid()) {
    return RecordFailure(ManagementServiceError::kInvalidMessagePipeHandle,
                         "Device trust key rotation failed. Invalid message "
                         "pipe in mojo invitation.");
  }

  auto pending_remote_url_loader_factory =
      mojo_helper_->CreatePendingRemote(std::move(pipe));

  if (!pending_remote_url_loader_factory.is_valid()) {
    return RecordFailure(
        ManagementServiceError::kInvalidPendingUrlLoaderFactory,
        "Device trust key rotation failed. Invalid url pending remote loader "
        "factory.");
  }

  mojo_helper_->BindRemote(remote_url_loader_factory_,
                           std::move(pending_remote_url_loader_factory));
  if (!remote_url_loader_factory_.is_bound()) {
    return RecordFailure(ManagementServiceError::kUnBoundUrlLoaderFactory,
                         "Device trust key rotation failed. The url loader "
                         "factory failed to bind to the browser process.");
  }

  if (!mojo_helper_->CheckRemoteConnection(remote_url_loader_factory_)) {
    return RecordFailure(ManagementServiceError::kDisconnectedUrlLoaderFactory,
                         "Device trust key rotation failed. The url loader "
                         "factory failed to connect to the browser process.");
  }

  return std::move(rotation_callback_).Run(command_line);
}

int ChromeManagementService::StartRotation(
    const base::CommandLine* command_line) {
  auto key_rotation_manager =
      KeyRotationManager::Create(std::make_unique<MojoKeyNetworkDelegate>(
          base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
              remote_url_loader_factory_.get())));
  return KeyRotationResultToExitCode(RotateDeviceTrustKey(
      std::move(key_rotation_manager), *command_line, chrome::GetChannel()));
}

}  // namespace enterprise_connectors
