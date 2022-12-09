// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crosapi/test/ash_crosapi_tests_env.h"

#include <fcntl.h>

#include "ash/constants/ash_switches.h"
#include "base/base_paths.h"
#include "base/base_switches.h"
#include "base/check.h"
#include "base/command_line.h"
#include "base/files/file_path_watcher.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/path_service.h"
#include "base/process/launch.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/bind.h"
#include "chrome/common/chrome_switches.h"
#include "chromeos/crosapi/mojom/crosapi.mojom.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/platform/named_platform_channel.h"
#include "mojo/public/cpp/platform/platform_channel.h"
#include "mojo/public/cpp/platform/socket_utils_posix.h"
#include "mojo/public/cpp/system/invitation.h"
#include "ui/gl/gl_switches.h"

namespace crosapi {

namespace {

AshCrosapiTestEnv* g_instance = nullptr;

}  // namespace

AshCrosapiTestEnv::AshCrosapiTestEnv() {
  CHECK(!g_instance) << "AshCrosapiTestEnv is already created.";
  g_instance = this;

  // If the user has specified a path for the ash-chrome binary, use the path.
  // Note that both absolute and relative paths are accepted, and the last path
  // component should be 'chrome', 'test_ash_chrome' or equivalent.
  // The chrome passed should be built, and if you use some test-specific
  // features such as TestController crosapi, you need to pass
  // 'test_ash_chrome',
  base::FilePath ash_chrome_path =
      base::CommandLine::ForCurrentProcess()->GetSwitchValuePath(
          "ash-chrome-path");

  // By default, 'test_ash_chrome' is built and used under the directory that
  // contains application assets.
  if (ash_chrome_path.empty()) {
    CHECK(base::PathService::Get(base::DIR_ASSETS, &ash_chrome_path));
    ash_chrome_path = ash_chrome_path.Append("test_ash_chrome");
  }

  CHECK(base::PathExists(ash_chrome_path));
  // Sets chrome binary file to run ash process.
  base::CommandLine command_line(ash_chrome_path);

  // Sets a socket path.
  base::ScopedTempDir temp_dir;
  CHECK(temp_dir.CreateUniqueTempDir());
  const std::string socket_path =
      temp_dir.GetPath().AppendASCII("lacros.socket").MaybeAsASCII();
  command_line.AppendSwitchASCII(ash::switches::kLacrosMojoSocketForTesting,
                                 socket_path);

  // Sets a user data dir path.
  CHECK(user_data_dir_.CreateUniqueTempDir());
  command_line.AppendSwitchPath(switches::kUserDataDir,
                                user_data_dir_.GetPath());
  command_line.AppendSwitch(ash::switches::kUseMyFilesInUserDataDirForTesting);

  // This switch is for filemanger test to install an app.
  // TODO(crbug.com/1376891): Remove settings for a specific test.
  command_line.AppendSwitchASCII(switches::kEnableFeatures, "WebAppsCrosapi");

#if defined(MEMORY_SANITIZER)
  // MSAN is incompatible with GL acceleration.
  command_line.AppendSwitch(switches::kOverrideUseSoftwareGLForTests);
#endif

  // Waits for socket connection to establish.
  // TODO(crbug.com/1368029): Separate logs generated during setup from those
  // generated during test.
  base::FilePathWatcher watcher;
  base::RunLoop run_loop;
  // TODO(crbug.com/1371015): Terminate runloop if the passed chrome couldn't be
  // exec.
  CHECK(watcher.Watch(base::FilePath(socket_path),
                      base::FilePathWatcher::Type::kNonRecursive,
                      base::BindLambdaForTesting(
                          [&](const base::FilePath& filepath, bool error) {
                            CHECK(!error);
                            run_loop.Quit();
                          })));
  process_ = base::LaunchProcess(command_line, {});
  CHECK(process_.IsValid());
  run_loop.Run();

  // Sets up to use Mojo interface.
  auto channel = mojo::NamedPlatformChannel::ConnectToServer(socket_path);
  base::ScopedFD socket_fd = channel.TakePlatformHandle().TakeFD();
  int flags = fcntl(socket_fd.get(), F_GETFL);
  fcntl(socket_fd.get(), F_SETFL, flags & ~O_NONBLOCK);
  std::vector<base::ScopedFD> descriptors;
  uint8_t buf[32];
  auto size = mojo::SocketRecvmsg(socket_fd.get(), buf, sizeof(buf),
                                  &descriptors, true /*block*/);
  CHECK_EQ(size, 1);
  CHECK_EQ(buf[0], 1u);
  CHECK_EQ(descriptors.size(), 2u);

  auto endpoint = mojo::PlatformChannelEndpoint(
      mojo::PlatformHandle(std::move(descriptors[1])));

  mojo::IncomingInvitation invitation =
      mojo::IncomingInvitation::Accept(std::move(endpoint));

  // Binds crosapi.
  crosapi_remote_ =
      mojo::Remote<mojom::Crosapi>(mojo::PendingRemote<mojom::Crosapi>(
          invitation.ExtractMessagePipe(0), 0u));
}

AshCrosapiTestEnv::~AshCrosapiTestEnv() {
  CHECK_EQ(g_instance, this);
  if (process_.IsValid())
    process_.Terminate(/*exit_code=*/0, /*wait=*/true);
  g_instance = nullptr;
}

// static
AshCrosapiTestEnv* AshCrosapiTestEnv::GetInstance() {
  CHECK(g_instance) << "AshCrosapiTestEnv is not created.";
  return g_instance;
}

bool AshCrosapiTestEnv::IsValid() {
  return process_.IsValid() && crosapi_remote_.is_bound();
}

}  // namespace crosapi
