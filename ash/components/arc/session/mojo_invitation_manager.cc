// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/components/arc/session/mojo_invitation_manager.h"

#include <string_view>

#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/process/process.h"
#include "base/rand_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "mojo/core/embedder/embedder.h"
#include "mojo/proxy/switches.h"
#include "mojo/public/cpp/platform/platform_channel.h"
#include "mojo/public/cpp/system/invitation.h"
#include "mojo/public/cpp/system/message_pipe.h"

namespace arc {

namespace {

base::FilePath GetMojoProxyPath() {
  base::FilePath mojo_proxy_path;
  CHECK(base::PathService::Get(base::DIR_EXE, &mojo_proxy_path));
  return mojo_proxy_path.Append("mojo_proxy");
}

}  // namespace

MojoInvitationManager::MojoInvitationManager()
    : MojoInvitationManager(GetMojoProxyPath()) {}

MojoInvitationManager::MojoInvitationManager(const base::FilePath& proxy_path)
    : proxy_path_(proxy_path) {}

MojoInvitationManager::~MojoInvitationManager() {
  if (proxy_process_.IsValid()) {
    // Ensure proxy process is terminated.
    base::ThreadPool::PostTask(
        FROM_HERE,
        {base::WithBaseSyncPrimitives(),
         base::TaskShutdownBehavior::BLOCK_SHUTDOWN},
        base::BindOnce(&CollectMojoProxyProcess, std::move(proxy_process_)));
  }
}

base::Process MojoInvitationManager::LaunchMojoProxy(
    mojo::PlatformChannel& channel,
    mojo::PlatformChannel& proxy_channel,
    std::string_view token) {
  base::ScopedFD target_fd =
      channel.TakeLocalEndpoint().TakePlatformHandle().TakeFD();
  auto proxy_remote_endpoint = proxy_channel.TakeRemoteEndpoint();
  base::ScopedFD proxy_fd = proxy_remote_endpoint.TakePlatformHandle().TakeFD();

  constexpr int kLegacyClientFdValue = STDERR_FILENO + 1;
  constexpr int kHostIpczTransportFdValue = kLegacyClientFdValue + 1;

  base::LaunchOptions proxy_launch_options;
  proxy_launch_options.fds_to_remap.emplace_back(target_fd.get(),
                                                 kLegacyClientFdValue);
  proxy_launch_options.fds_to_remap.emplace_back(proxy_fd.get(),
                                                 kHostIpczTransportFdValue);

  base::CommandLine proxy_command_line(proxy_path_);
  proxy_command_line.AppendSwitchASCII(
      switches::kLegacyClientFd, base::NumberToString(kLegacyClientFdValue));
  proxy_command_line.AppendSwitchASCII(
      switches::kHostIpczTransportFd,
      base::NumberToString(kHostIpczTransportFdValue));
  proxy_command_line.AppendSwitchASCII(switches::kAttachmentName, token);
  proxy_command_line.AppendSwitch(switches::kInheritIpczBroker);

  base::Process proxy_process =
      base::LaunchProcess(proxy_command_line, proxy_launch_options);
  proxy_remote_endpoint.ProcessLaunchAttempted();

  return proxy_process;
}

void MojoInvitationManager::SendInvitation(mojo::PlatformChannel& channel,
                                           std::string_view token) {
  mojo::OutgoingInvitation invitation;
  pipe_ = invitation.AttachMessagePipe(token);

  if (mojo::core::IsMojoIpczEnabled()) {
    // ARCVM containers still use legacy Mojo Core. If IPCZ is enabled, we
    // spawn an instance of Mojo Proxy which acts as a IPCZ <=> Mojo Core
    // translation layer between Ash Chrome and ARCVM.
    mojo::PlatformChannel proxy_channel;
    proxy_process_ = LaunchMojoProxy(channel, proxy_channel, token);
    DCHECK(proxy_process_.IsValid());
    invitation.set_extra_flags(MOJO_SEND_INVITATION_FLAG_SHARE_BROKER);
    mojo::OutgoingInvitation::Send(std::move(invitation),
                                   proxy_process_.Handle(),
                                   proxy_channel.TakeLocalEndpoint());
  } else {
    mojo::OutgoingInvitation::Send(std::move(invitation),
                                   base::kNullProcessHandle,
                                   channel.TakeLocalEndpoint());
  }
}

// static
void MojoInvitationManager::CollectMojoProxyProcess(
    base::Process proxy_process) {
  // Wait for some time until the proxy process terminates.
  // In the common case, Mojo Proxy will exit cleanly once
  // all portals are closed.
  constexpr base::TimeDelta timeout = base::Milliseconds(2500);
  if (proxy_process.WaitForExitWithTimeout(timeout, nullptr)) {
    return;
  }

  // Otherwise, terminate the process forcefully.
  bool success = proxy_process.Terminate(/*exit_code=*/0, /*wait=*/true);
  LOG_IF(ERROR, !success) << "Failed to terminate Mojo Proxy process";
}

}  // namespace arc
