// Copyright 2022 The Chromium Authors. All rights reserved.
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
#include "base/path_service.h"
#include "base/syslog_logging.h"
#include "chrome/browser/enterprise/connectors/device_trust/key_management/core/network/mojo_key_network_delegate.h"
#include "chrome/browser/enterprise/connectors/device_trust/key_management/core/shared_command_constants.h"
#include "chrome/browser/enterprise/connectors/device_trust/key_management/installer/key_rotation_manager.h"
#include "chrome/browser/enterprise/connectors/device_trust/key_management/installer/management_service/rotate_util.h"
#include "chrome/common/channel_info.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/platform/platform_channel.h"
#include "mojo/public/cpp/system/invitation.h"
#include "mojo/public/cpp/system/message_pipe.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"

namespace enterprise_connectors {

namespace {

// Verifies group permissions for the chrome-management-service binary.
bool CheckBinaryPermissions() {
  base::FilePath exe_path;
  if (!base::PathService::Get(base::DIR_EXE, &exe_path)) {
    SYSLOG(ERROR) << "The chrome-management-service failed. Could not get the "
                  << "path to the chrome-management-service.";
    return false;
  }
  exe_path = exe_path.Append(constants::kBinaryFileName);

  // Gets the chromemgmt group gid stored on the device.
  struct group* chrome_mgmt_group = getgrnam(constants::kGroupName);
  if (!chrome_mgmt_group) {
    SYSLOG(ERROR) << "The chrome-management-service failed. Device missing the "
                  << "necessary group permissions to run the command.";
    return false;
  }
  gid_t chrome_mgmt_gid = chrome_mgmt_group->gr_gid;

  // Gets the gid of the chrome-management-service binary file.
  struct stat st;
  stat(exe_path.value().c_str(), &st);
  gid_t binary_gid = st.st_gid;

  if (getegid() != chrome_mgmt_gid || binary_gid != chrome_mgmt_gid) {
    SYSLOG(ERROR) << "The chrome-management-service failed. Incorrect "
                  << "permissions for the chrome-management-service.";
    return false;
  }
  return true;
}

}  // namespace

ChromeManagementService::ChromeManagementService()
    : permissions_callback_(base::BindOnce(&CheckBinaryPermissions)),
      rotation_callback_(base::BindOnce(&ChromeManagementService::StartRotation,
                                        base::Unretained(this))) {}

ChromeManagementService::ChromeManagementService(
    PermissionsCallback permissions_callback,
    RotationCallback rotation_callback)
    : permissions_callback_(std::move(permissions_callback)),
      rotation_callback_(std::move(rotation_callback)) {
  DCHECK(permissions_callback_);
  DCHECK(rotation_callback_);
}

ChromeManagementService::~ChromeManagementService() = default;

int ChromeManagementService::Run(const base::CommandLine* command_line,
                                 uint64_t pipe_name) {
  if (!command_line || !command_line->HasSwitch(switches::kRotateDTKey)) {
    SYSLOG(ERROR)
        << "Device trust key rotation failed. Command missing rotate details.";
    return kFailure;
  }

  if (!std::move(permissions_callback_).Run())
    return kFailure;

  mojo::IncomingInvitation invitation = mojo::IncomingInvitation::Accept(
      mojo::PlatformChannel::RecoverPassedEndpointFromCommandLine(
          *command_line));

  mojo::ScopedMessagePipeHandle pipe = invitation.ExtractMessagePipe(pipe_name);

  auto pending_remote_url_loader_factory =
      mojo::PendingRemote<network::mojom::URLLoaderFactory>(std::move(pipe), 0);
  if (!pending_remote_url_loader_factory.is_valid()) {
    SYSLOG(ERROR) << "Device trust key rotation failed. Could not "
                     "connect to the browser process.";
    return kFailure;
  }

  remote_url_loader_factory_.Bind(std::move(pending_remote_url_loader_factory));
  if (!remote_url_loader_factory_.is_bound()) {
    SYSLOG(ERROR) << "Device trust key rotation failed. Could not "
                     "connect to the browser process.";
    return kFailure;
  }

  return std::move(rotation_callback_).Run(command_line);
}

int ChromeManagementService::StartRotation(
    const base::CommandLine* command_line) {
  auto key_rotation_manager =
      KeyRotationManager::Create(std::make_unique<MojoKeyNetworkDelegate>(
          remote_url_loader_factory_.get()));
  return RotateDeviceTrustKey(std::move(key_rotation_manager), *command_line,
                              chrome::GetChannel())
             ? kSuccess
             : kFailure;
}

}  // namespace enterprise_connectors
