// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/components/arc/session/mojo_invitation_manager.h"

#include "ash/components/arc/session/mojo_init_data.h"
#include "base/command_line.h"
#include "base/process/launch.h"
#include "base/test/multiprocess_test.h"
#include "base/test/test_timeouts.h"
#include "base/threading/thread.h"
#include "mojo/core/embedder/embedder.h"
#include "mojo/core/embedder/scoped_ipc_support.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/multiprocess_func_list.h"

namespace arc {
namespace {

TEST(ArcMojoInvitationTest, MojoProxyIsLaunchedWhenIpczIsEnabled) {
  base::CommandLine command_line =
      base::GetMultiProcessTestChildBaseCommandLine();
  base::LaunchOptions launch_options;
  launch_options.environment["MOJO_IPCZ"] = "1";
  base::Process process = base::SpawnMultiProcessTestChild(
      "InvitationSenderMain", command_line, launch_options);

  int rv = -1;
  process.WaitForExitWithTimeout(TestTimeouts::action_timeout(), &rv);
  EXPECT_EQ(0, rv);
}

TEST(ArcMojoInvitationTest, MojoProxyIsNotLaunchedWhenIpczIsDisabled) {
  base::CommandLine command_line =
      base::GetMultiProcessTestChildBaseCommandLine();
  base::LaunchOptions launch_options;
  base::Process process = base::SpawnMultiProcessTestChild(
      "InvitationSenderMain", command_line, launch_options);

  int rv = -1;
  process.WaitForExitWithTimeout(TestTimeouts::action_timeout(), &rv);
  EXPECT_EQ(0, rv);
}

MULTIPROCESS_TEST_MAIN(InvitationSenderMain) {
  base::Thread io_thread{"IO thread"};
  io_thread.StartWithOptions(
      base::Thread::Options{base::MessagePumpType::IO, 0});

  auto ipc_support = std::make_unique<mojo::core::ScopedIPCSupport>(
      io_thread.task_runner(),
      mojo::core::ScopedIPCSupport::ShutdownPolicy::CLEAN);

  {
    MojoInvitationManager invitation_manager(base::FilePath("/bin/true"));
    mojo::PlatformChannel channel;
    MojoInitData mojo_init_data;
    invitation_manager.SendInvitation(channel, mojo_init_data.token());

    base::Process proxy_process = std::move(invitation_manager.proxy_process());
    if (mojo::core::IsMojoIpczEnabled()) {
      // Verify that (fake) proxy process was launched successfully.
      CHECK(proxy_process.IsValid());
      int rv = -1;
      proxy_process.WaitForExit(&rv);
      CHECK_EQ(0, rv);
    } else {
      // Verify that (fake) proxy process was not launched.
      CHECK(!proxy_process.IsValid());
    }
  }

  ipc_support.reset();
  io_thread.Stop();
  mojo::core::ShutDown();

  return 0;
}

}  // namespace
}  // namespace arc
