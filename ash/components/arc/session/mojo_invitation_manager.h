// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMPONENTS_ARC_SESSION_MOJO_INVITATION_MANAGER_H_
#define ASH_COMPONENTS_ARC_SESSION_MOJO_INVITATION_MANAGER_H_

#include <string_view>

#include "base/files/file_path.h"
#include "base/process/process.h"
#include "mojo/public/cpp/platform/platform_channel.h"
#include "mojo/public/cpp/system/message_pipe.h"

namespace arc {

// Handles sending a Mojo invitation to ARCVM.
//
// If required (i.e. if IPCZ is enabled in Ash Chrome), it also launches
// an instance of Mojo Proxy to act as a translation layer between IPCZ
// and Mojo Core. On destruction, it posts a task to collect the process.
//
// This class is not thread-safe. The caller must guarantee method calls
// are not being made simultaneously in a multithreaded context.
class MojoInvitationManager {
 public:
  MojoInvitationManager();
  explicit MojoInvitationManager(const base::FilePath& proxy_path);
  ~MojoInvitationManager();

  void SendInvitation(mojo::PlatformChannel& channel, std::string_view token);

  mojo::ScopedMessagePipeHandle TakePipe() { return std::move(pipe_); }
  base::Process& proxy_process() { return proxy_process_; }

 private:
  base::Process LaunchMojoProxy(mojo::PlatformChannel& channel,
                                mojo::PlatformChannel& proxy_channel,
                                std::string_view token);
  static void CollectMojoProxyProcess(base::Process proxy_process);

  mojo::ScopedMessagePipeHandle pipe_;
  base::FilePath proxy_path_;
  base::Process proxy_process_;
};

}  // namespace arc

#endif  // ASH_COMPONENTS_ARC_SESSION_MOJO_INVITATION_MANAGER_H_
