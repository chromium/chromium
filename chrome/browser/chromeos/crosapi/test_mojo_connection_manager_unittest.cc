// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/crosapi/test_mojo_connection_manager.h"

#include <fcntl.h>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_path_watcher.h"
#include "base/files/scoped_file.h"
#include "base/files/scoped_temp_dir.h"
#include "base/process/launch.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/thread_pool.h"
#include "base/test/bind_test_util.h"
#include "base/test/multiprocess_test.h"
#include "base/test/task_environment.h"
#include "base/test/test_timeouts.h"
#include "chrome/test/base/scoped_testing_local_state.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chromeos/crosapi/mojom/crosapi.mojom.h"
#include "mojo/public/cpp/platform/named_platform_channel.h"
#include "mojo/public/cpp/platform/platform_channel.h"
#include "mojo/public/cpp/platform/socket_utils_posix.h"
#include "mojo/public/cpp/system/invitation.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/multiprocess_func_list.h"

namespace crosapi {

class TestLacrosChromeService : public crosapi::mojom::LacrosChromeService {
 public:
  TestLacrosChromeService(
      mojo::PendingReceiver<mojom::LacrosChromeService> receiver,
      base::RunLoop& run_loop)
      : receiver_(this, std::move(receiver)), run_loop_(run_loop) {}

  ~TestLacrosChromeService() override = default;

  void Init(crosapi::mojom::LacrosInitParamsPtr params) override {
    init_is_called_ = true;
  }

  void RequestAshChromeServiceReceiver(
      RequestAshChromeServiceReceiverCallback callback) override {
    EXPECT_TRUE(init_is_called_);
    std::move(callback).Run(ash_chrome_service_.BindNewPipeAndPassReceiver());
    request_ash_chrome_service_is_called_ = true;
    run_loop_.Quit();
  }

  void NewWindow(NewWindowCallback callback) override {}

  bool init_is_called() { return init_is_called_; }

  bool request_ash_chrome_service_is_called() {
    return request_ash_chrome_service_is_called_;
  }

 private:
  mojo::Receiver<mojom::LacrosChromeService> receiver_;

  bool init_is_called_ = false;
  bool request_ash_chrome_service_is_called_ = false;

  base::RunLoop& run_loop_;

  mojo::Remote<crosapi::mojom::AshChromeService> ash_chrome_service_;
};

using TestMojoConnectionManagerTest = testing::Test;

TEST_F(TestMojoConnectionManagerTest, ConnectWithLacrosChrome) {
  // Constructing LacrosInitParams requires local state prefs.
  ScopedTestingLocalState local_state(TestingBrowserProcess::GetGlobal());

  // Create temp dir before task environment, just in case lingering tasks need
  // to access it.
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  // Use IO type to support the FileDescriptorWatcher API on POSIX.
  base::test::TaskEnvironment task_environment{
      base::test::TaskEnvironment::MainThreadType::IO};

  // Ash-chrome queues an invitation, drop a socket and wait for connection.
  std::string socket_path =
      temp_dir.GetPath().MaybeAsASCII() + "/lacros.socket";

  // Sets up watcher to Wait for the socket to be created by
  // |TestMojoConnectionManager| before attempting to connect. There is no
  // garanteen that |OnTestingSocketCreated| has run after the run loop is done,
  // so this test should NOT depend on the assumption.
  base::FilePathWatcher watcher;
  base::RunLoop run_loop1;
  watcher.Watch(base::FilePath(socket_path), false,
                base::BindRepeating(base::BindLambdaForTesting(
                    [&run_loop1](const base::FilePath& path, bool error) {
                      EXPECT_FALSE(error);
                      run_loop1.Quit();
                    })));
  TestMojoConnectionManager test_mojo_connection_manager{
      base::FilePath(socket_path)};
  run_loop1.Run();

  // Test connects with ash-chrome via the socket.
  auto channel = mojo::NamedPlatformChannel::ConnectToServer(socket_path);
  ASSERT_TRUE(channel.is_valid());
  base::ScopedFD socket_fd = channel.TakePlatformHandle().TakeFD();

  uint8_t buf[32];
  std::vector<base::ScopedFD> descriptors;
  ssize_t size;
  base::RunLoop run_loop2;
  base::ThreadPool::PostTaskAndReply(
      FROM_HERE, {base::MayBlock()},
      base::BindOnce(base::BindLambdaForTesting([&]() {
        // Mark the channel as blocking.
        int flags = fcntl(socket_fd.get(), F_GETFL);
        PCHECK(flags != -1);
        fcntl(socket_fd.get(), F_SETFL, flags & ~O_NONBLOCK);
        size = mojo::SocketRecvmsg(socket_fd.get(), buf, sizeof(buf),
                                   &descriptors, true /*block*/);
      })),
      run_loop2.QuitClosure());
  run_loop2.Run();
  EXPECT_EQ(1, size);
  EXPECT_EQ(0u, buf[0]);
  ASSERT_EQ(1u, descriptors.size());

  // Test launches lacros-chrome as child process.
  base::LaunchOptions options;
  options.fds_to_remap.emplace_back(descriptors[0].get(), descriptors[0].get());
  base::CommandLine lacros_cmd(base::GetMultiProcessTestChildBaseCommandLine());
  lacros_cmd.AppendSwitchASCII(mojo::PlatformChannel::kHandleSwitch,
                               base::NumberToString(descriptors[0].release()));
  base::Process lacros_process =
      base::SpawnMultiProcessTestChild("LacrosMain", lacros_cmd, options);

  // lacros-chrome accepts the invitation to establish mojo connection with
  // ash-chrome.
  int rv = -1;
  ASSERT_TRUE(base::WaitForMultiprocessTestChildExit(
      lacros_process, TestTimeouts::action_timeout(), &rv));
  lacros_process.Close();
  EXPECT_EQ(0, rv);
}

// Another process that emulates the behavior of lacros-chrome.
MULTIPROCESS_TEST_MAIN(LacrosMain) {
  base::test::SingleThreadTaskEnvironment task_environment;
  mojo::IncomingInvitation invitation = mojo::IncomingInvitation::Accept(
      mojo::PlatformChannel::RecoverPassedEndpointFromCommandLine(
          *base::CommandLine::ForCurrentProcess()));
  base::RunLoop run_loop;
  TestLacrosChromeService test_lacros_chrome_service(
      mojo::PendingReceiver<crosapi::mojom::LacrosChromeService>(
          invitation.ExtractMessagePipe(0)),
      run_loop);
  run_loop.Run();
  DCHECK(test_lacros_chrome_service.init_is_called());
  DCHECK(test_lacros_chrome_service.request_ash_chrome_service_is_called());
  return 0;
}

}  // namespace crosapi
