// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/device_trust/key_management/browser/commands/linux_key_rotation_command.h"

#include <string>
#include <utility>

#include "base/base64.h"
#include "base/base_paths.h"
#include "base/check.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/path_service.h"
#include "base/process/process.h"
#include "base/rand_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/syslog_logging.h"
#include "base/task/thread_pool.h"
#include "base/threading/thread_restrictions.h"
#include "base/time/time.h"
#include "chrome/browser/enterprise/connectors/device_trust/common/device_trust_constants.h"
#include "chrome/browser/enterprise/connectors/device_trust/key_management/browser/commands/metrics_utils.h"
#include "chrome/browser/enterprise/connectors/device_trust/key_management/core/shared_command_constants.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/platform/platform_channel.h"
#include "mojo/public/cpp/system/invitation.h"
#include "mojo/public/cpp/system/message_pipe.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace enterprise_connectors {

namespace {

base::FilePath GetBinaryFilePath() {
  base::FilePath exe_path;
  if (base::PathService::Get(base::DIR_EXE, &exe_path)) {
    return exe_path.Append(constants::kBinaryFileName);
  }
  return exe_path;
}

// Builds the command line needed to launch the service. The  `params` specify
// the needed KeyRotationCommandParams. 'pipe_name` is the name of the pipe to
//  connect to.
base::CommandLine GetCommandLine(const KeyRotationCommand::Params& params,
                                 const std::string& pipe_name) {
  base::FilePath exe_path = GetBinaryFilePath();

  base::CommandLine command_line(exe_path);
  std::string token_base64 = base::Base64Encode(params.dm_token);
  std::string nonce_base64 = base::Base64Encode(params.nonce);

  command_line.AppendSwitchNative(switches::kRotateDTKey, token_base64);
  command_line.AppendSwitchNative(switches::kDmServerUrl, params.dm_server_url);
  command_line.AppendSwitchNative(switches::kNonce, nonce_base64);
  command_line.AppendSwitchASCII(switches::kPipeName, pipe_name);
  return command_line;
}

// `command_line` is the command line we get from the GetCommandLine function,
// and `options` are the launch options we need to launch the process.
base::Process Launch(const base::CommandLine& command_line,
                     const base::LaunchOptions& options) {
  return base::LaunchProcess(command_line, options);
}

}  // namespace

LinuxKeyRotationCommand::LinuxKeyRotationCommand(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory)
    : LinuxKeyRotationCommand(base::BindRepeating(&Launch),
                              std::move(url_loader_factory)) {}

LinuxKeyRotationCommand::LinuxKeyRotationCommand(
    LaunchCallback launch_callback,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory)
    : launch_callback_(std::move(launch_callback)),
      url_loader_factory_(std::move(url_loader_factory)) {
  DCHECK(launch_callback_);
  DCHECK(url_loader_factory_);
}

LinuxKeyRotationCommand::~LinuxKeyRotationCommand() = default;

void LinuxKeyRotationCommand::Trigger(const Params& params, Callback callback) {
  DCHECK(callback);

  uint64_t pipe_name = base::RandUint64();
  base::CommandLine command_line =
      GetCommandLine(params, base::NumberToString(pipe_name));

  mojo::OutgoingInvitation invitation;
  mojo::ScopedMessagePipeHandle pipe = invitation.AttachMessagePipe(pipe_name);
  auto pending_receiver =
      mojo::PendingReceiver<network::mojom::URLLoaderFactory>(std::move(pipe));
  url_loader_factory_->Clone(std::move(pending_receiver));

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE,
      {base::MayBlock(), base::TaskPriority::USER_BLOCKING,
       base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
      base::BindOnce(
          [](base::CommandLine command_line, LaunchCallback launch_callback,
             mojo::OutgoingInvitation invitation) {
            if (!base::PathExists(GetBinaryFilePath())) {
              SYSLOG(ERROR)
                  << "Device trust key rotation failed. Could not find "
                     "management service executable.";
              LogKeyRotationCommandError(
                  KeyRotationCommandError::kMissingManagementService);
              return KeyRotationCommand::Status::FAILED_INVALID_INSTALLATION;
            }

            mojo::PlatformChannel channel;
            base::LaunchOptions options;
            options.allow_new_privs = true;
            channel.PrepareToPassRemoteEndpoint(&options, &command_line);

            base::Process process = launch_callback.Run(command_line, options);
            if (!process.IsValid()) {
              SYSLOG(ERROR) << "Device trust key rotation failed. Could not "
                               "launch the ChromeManagementService process.";
              LogKeyRotationCommandError(
                  KeyRotationCommandError::kProcessInvalid);
              return KeyRotationCommand::Status::FAILED;
            }

            channel.RemoteProcessLaunchAttempted();
            mojo::OutgoingInvitation::Send(std::move(invitation),
                                           process.Handle(),
                                           channel.TakeLocalEndpoint());

            base::ScopedAllowBaseSyncPrimitives allow_wait;
            int exit_code = -1;
            if (!process.WaitForExitWithTimeout(timeouts::kProcessWaitTimeout,
                                                &exit_code)) {
              SYSLOG(ERROR) << "Device trust key rotation timed out.";
              LogKeyRotationCommandError(KeyRotationCommandError::kTimeout);
              return KeyRotationCommand::Status::TIMED_OUT;
            }

            LogKeyRotationExitCode(exit_code);

            switch (exit_code) {
              case kSuccess:
                return KeyRotationCommand::Status::SUCCEEDED;
              case kFailedInsufficientPermissions:
                return KeyRotationCommand::Status::FAILED_INVALID_PERMISSIONS;
              case kFailedKeyConflict:
                return KeyRotationCommand::Status::FAILED_KEY_CONFLICT;
              case kFailure:
                return KeyRotationCommand::Status::FAILED;
              case kUnknownFailure:
              default:
                SYSLOG(ERROR)
                    << "Device trust key rotation failed with exit code: "
                    << exit_code;
                return KeyRotationCommand::Status::FAILED;
            }
          },
          command_line, launch_callback_, std::move(invitation)),
      std::move(callback));
}

}  // namespace enterprise_connectors
