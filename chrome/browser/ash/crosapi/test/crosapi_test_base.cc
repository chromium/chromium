// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crosapi/test/crosapi_test_base.h"

#include <fcntl.h>

#include "ash/constants/ash_switches.h"
#include "base/command_line.h"
#include "base/files/file_path_watcher.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/path_service.h"
#include "base/process/launch.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/bind.h"
#include "chromeos/crosapi/cpp/crosapi_constants.h"
#include "chromeos/crosapi/mojom/crosapi.mojom.h"
#include "mojo/core/embedder/embedder.h"
#include "mojo/public/cpp/platform/named_platform_channel.h"
#include "mojo/public/cpp/platform/platform_channel.h"
#include "mojo/public/cpp/platform/socket_utils_posix.h"
#include "mojo/public/cpp/system/invitation.h"

namespace crosapi {

CrosapiTestBase::CrosapiTestBase() = default;
CrosapiTestBase::~CrosapiTestBase() = default;

void CrosapiTestBase::SetUp() {
  // Setup Mojo and its thread.
  // TODO(crbug.com/1361906): Move this initialization to main funstion.
  mojo::core::Init();

  io_thread_ = std::make_unique<base::Thread>("MojoThread");
  ASSERT_TRUE(io_thread_->StartWithOptions(
      base::Thread::Options(base::MessagePumpType::IO, 0)));

  ipc_support_ = std::make_unique<mojo::core::ScopedIPCSupport>(
      io_thread_->task_runner(),
      mojo::core::ScopedIPCSupport::ShutdownPolicy::CLEAN);

  // Set commands and launch Ash process.
  // TODO(crbug.com/1361906): Move to main functioon.
  base::FilePath path;
  ASSERT_TRUE(base::PathService::Get(base::DIR_ASSETS, &path));
  base::CommandLine command_line{path.Append("chrome")};

  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  const std::string socket_path =
      temp_dir.GetPath().AppendASCII("lacros.socket").MaybeAsASCII();
  command_line.AppendSwitchASCII(ash::switches::kLacrosMojoSocketForTesting,
                                 socket_path);

  // Wait until socket is available.
  base::FilePathWatcher watcher;
  base::RunLoop run_loop;

  ASSERT_TRUE(watcher.Watch(
      base::FilePath(socket_path), base::FilePathWatcher::Type::kNonRecursive,
      base::BindLambdaForTesting(
          [&](const base::FilePath& filepath, bool error) {
            ASSERT_TRUE(base::PathExists(base::FilePath(socket_path)));
            run_loop.Quit();
          })));

  process_ = base::LaunchProcess(command_line, {});
  ASSERT_TRUE(process_.IsValid());

  run_loop.Run();

  auto channel = mojo::NamedPlatformChannel::ConnectToServer(socket_path);
  base::ScopedFD socket_fd = channel.TakePlatformHandle().TakeFD();

  int flags = fcntl(socket_fd.get(), F_GETFL);
  fcntl(socket_fd.get(), F_SETFL, flags & ~O_NONBLOCK);

  uint8_t buf[32];
  std::vector<base::ScopedFD> descriptors;
  auto size = mojo::SocketRecvmsg(socket_fd.get(), buf, sizeof(buf),
                                  &descriptors, true /*block*/);
  ASSERT_EQ(1, size);
  ASSERT_EQ(2u, descriptors.size());
  auto endpoint = mojo::PlatformChannel::RecoverPassedEndpointFromString(
      base::NumberToString(descriptors[1].release()));

  // Bind Crosapi.
  mojo::IncomingInvitation invitation =
      mojo::IncomingInvitation::Accept(std::move(endpoint));

  remote_crosapi =
      mojo::Remote<mojom::Crosapi>(mojo::PendingRemote<mojom::Crosapi>(
          invitation.ExtractMessagePipe(0), 0u));
}

void CrosapiTestBase::TearDown() {
  if (process_.IsValid())
    process_.Terminate(/*exit_code=*/0, /*wait=*/true);
}

}  // namespace crosapi
