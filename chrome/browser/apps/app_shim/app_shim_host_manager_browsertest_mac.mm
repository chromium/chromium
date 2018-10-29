// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_shim/app_shim_host_manager_mac.h"

#include <unistd.h>

#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/threading/thread_restrictions.h"
#include "chrome/browser/apps/app_shim/app_shim_handler_mac.h"
#include "chrome/browser/apps/app_shim/test/app_shim_host_manager_test_api_mac.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/mac/app_mode_common.h"
#include "chrome/common/mac/app_shim.mojom.h"
#include "chrome/common/mac/app_shim_param_traits.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/version_info/version_info.h"
#include "content/public/test/test_utils.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "mojo/public/cpp/platform/named_platform_channel.h"
#include "mojo/public/cpp/system/isolated_connection.h"

using LaunchAppCallback =
    chrome::mojom::AppShimHostBootstrap::LaunchAppCallback;

namespace {

const char kTestAppMode[] = "test_app";

// A test version of the AppShimController mojo client in chrome_main_app_mode.
class TestShimClient : public chrome::mojom::AppShim {
 public:
  TestShimClient();

  chrome::mojom::AppShimHostRequest GetHostRequest() {
    return std::move(host_request_);
  }
  LaunchAppCallback GetLaunchAppCallback() {
    return base::BindOnce(&TestShimClient::LaunchAppDone,
                          base::Unretained(this));
  }

  chrome::mojom::AppShimHostPtr& host() { return host_; }
  chrome::mojom::AppShimHostBootstrapPtr& host_bootstrap() {
    return host_bootstrap_;
  }

  // chrome::mojom::AppShim implementation (not used in testing, but can be).
  void CreateViewsBridgeFactory(
      views_bridge_mac::mojom::BridgeFactoryAssociatedRequest request)
      override {}
  void CreateContentNSViewBridgeFactory(
      content::mojom::NSViewBridgeFactoryAssociatedRequest request) override {}
  void Hide() override {}
  void UnhideWithoutActivation() override {}
  void SetUserAttention(apps::AppShimAttentionType attention_type) override {}

 private:
  void LaunchAppDone(apps::AppShimLaunchResult result,
                     chrome::mojom::AppShimRequest app_shim_request) {
    shim_binding_.Bind(std::move(app_shim_request));
  }

  mojo::IsolatedConnection mojo_connection_;
  mojo::Binding<chrome::mojom::AppShim> shim_binding_;
  chrome::mojom::AppShimHostPtr host_;
  chrome::mojom::AppShimHostRequest host_request_;
  chrome::mojom::AppShimHostBootstrapPtr host_bootstrap_;

  DISALLOW_COPY_AND_ASSIGN(TestShimClient);
};

TestShimClient::TestShimClient()
    : shim_binding_(this), host_request_(mojo::MakeRequest(&host_)) {
  base::FilePath user_data_dir;
  CHECK(base::PathService::Get(chrome::DIR_USER_DATA, &user_data_dir));
  base::FilePath symlink_path =
      user_data_dir.Append(app_mode::kAppShimSocketSymlinkName);

  base::FilePath socket_path;
  base::ScopedAllowBlockingForTesting allow_blocking;
  CHECK(base::ReadSymbolicLink(symlink_path, &socket_path));
  app_mode::VerifySocketPermissions(socket_path);

  mojo::ScopedMessagePipeHandle message_pipe = mojo_connection_.Connect(
      mojo::NamedPlatformChannel::ConnectToServer(socket_path.value()));
  host_bootstrap_ = chrome::mojom::AppShimHostBootstrapPtr(
      chrome::mojom::AppShimHostBootstrapPtrInfo(std::move(message_pipe), 0));
}

// Browser Test for AppShimHostManager to test IPC interactions across the
// UNIX domain socket.
class AppShimHostManagerBrowserTest : public InProcessBrowserTest,
                                      public apps::AppShimHandler {
 public:
  AppShimHostManagerBrowserTest() {}

 protected:
  // Wait for OnShimLaunch, then send a quit, and wait for the response. Used to
  // test launch behavior.
  void RunAndExitGracefully();

  // InProcessBrowserTest overrides:
  void SetUpOnMainThread() override;
  void TearDownOnMainThread() override;

  // AppShimHandler overrides:
  void OnShimLaunch(apps::AppShimHandler::Host* host,
                    apps::AppShimLaunchType launch_type,
                    const std::vector<base::FilePath>& files) override;
  void OnShimClose(apps::AppShimHandler::Host* host) override {}
  void OnShimFocus(apps::AppShimHandler::Host* host,
                   apps::AppShimFocusType focus_type,
                   const std::vector<base::FilePath>& files) override {}
  void OnShimSetHidden(apps::AppShimHandler::Host* host, bool hidden) override {
  }
  void OnShimQuit(apps::AppShimHandler::Host* host) override;

  std::unique_ptr<TestShimClient> test_client_;
  std::vector<base::FilePath> last_launch_files_;
  apps::AppShimLaunchType last_launch_type_ = apps::APP_SHIM_LAUNCH_NUM_TYPES;

 private:
  std::unique_ptr<base::RunLoop> runner_;

  int launch_count_ = 0;
  int quit_count_ = 0;

  DISALLOW_COPY_AND_ASSIGN(AppShimHostManagerBrowserTest);
};

void AppShimHostManagerBrowserTest::RunAndExitGracefully() {
  runner_ = std::make_unique<base::RunLoop>();
  EXPECT_EQ(0, launch_count_);
  runner_->Run();  // Will stop in OnShimLaunch().
  EXPECT_EQ(1, launch_count_);

  runner_ = std::make_unique<base::RunLoop>();
  test_client_->host()->QuitApp();
  EXPECT_EQ(0, quit_count_);
  runner_->Run();  // Will stop in OnShimQuit().
  EXPECT_EQ(1, quit_count_);

  test_client_.reset();
}

void AppShimHostManagerBrowserTest::SetUpOnMainThread() {
  // Can't do this in the constructor, it needs a BrowserProcess.
  apps::AppShimHandler::RegisterHandler(kTestAppMode, this);
}

void AppShimHostManagerBrowserTest::TearDownOnMainThread() {
  apps::AppShimHandler::RemoveHandler(kTestAppMode);
}

void AppShimHostManagerBrowserTest::OnShimLaunch(
    apps::AppShimHandler::Host* host,
    apps::AppShimLaunchType launch_type,
    const std::vector<base::FilePath>& files) {
  host->OnAppLaunchComplete(apps::APP_SHIM_LAUNCH_SUCCESS);
  ++launch_count_;
  last_launch_type_ = launch_type;
  last_launch_files_ = files;
  runner_->Quit();
}

void AppShimHostManagerBrowserTest::OnShimQuit(
    apps::AppShimHandler::Host* host) {
  ++quit_count_;
  runner_->Quit();
}

// Test regular launch, which would ask Chrome to launch the app.
IN_PROC_BROWSER_TEST_F(AppShimHostManagerBrowserTest, LaunchNormal) {
  test_client_.reset(new TestShimClient());
  test_client_->host_bootstrap()->LaunchApp(
      test_client_->GetHostRequest(), browser()->profile()->GetPath(),
      kTestAppMode, apps::APP_SHIM_LAUNCH_NORMAL, std::vector<base::FilePath>(),
      test_client_->GetLaunchAppCallback());

  RunAndExitGracefully();
  EXPECT_EQ(apps::APP_SHIM_LAUNCH_NORMAL, last_launch_type_);
  EXPECT_TRUE(last_launch_files_.empty());
}

// Test register-only launch, used when Chrome has already launched the app.
IN_PROC_BROWSER_TEST_F(AppShimHostManagerBrowserTest, LaunchRegisterOnly) {
  test_client_.reset(new TestShimClient());
  test_client_->host_bootstrap()->LaunchApp(
      test_client_->GetHostRequest(), browser()->profile()->GetPath(),
      kTestAppMode, apps::APP_SHIM_LAUNCH_REGISTER_ONLY,
      std::vector<base::FilePath>(), test_client_->GetLaunchAppCallback());

  RunAndExitGracefully();
  EXPECT_EQ(apps::APP_SHIM_LAUNCH_REGISTER_ONLY, last_launch_type_);
  EXPECT_TRUE(last_launch_files_.empty());
}

// Ensure the domain socket can be created in a fresh user data dir.
IN_PROC_BROWSER_TEST_F(AppShimHostManagerBrowserTest,
                       PRE_ReCreate) {
  test::AppShimHostManagerTestApi test_api(
      g_browser_process->platform_part()->app_shim_host_manager());
  EXPECT_TRUE(test_api.acceptor());
}

// Ensure the domain socket can be re-created after a prior browser process has
// quit.
IN_PROC_BROWSER_TEST_F(AppShimHostManagerBrowserTest,
                       ReCreate) {
  test::AppShimHostManagerTestApi test_api(
      g_browser_process->platform_part()->app_shim_host_manager());
  EXPECT_TRUE(test_api.acceptor());
}

// Tests for the files created by AppShimHostManager.
class AppShimHostManagerBrowserTestSocketFiles
    : public AppShimHostManagerBrowserTest {
 public:
  AppShimHostManagerBrowserTestSocketFiles() {}

 protected:
  base::FilePath directory_in_tmp_;
  base::FilePath symlink_path_;
  base::FilePath version_path_;

 private:
  bool SetUpUserDataDirectory() override;
  void TearDownInProcessBrowserTestFixture() override;

  DISALLOW_COPY_AND_ASSIGN(AppShimHostManagerBrowserTestSocketFiles);
};

bool AppShimHostManagerBrowserTestSocketFiles::SetUpUserDataDirectory() {
  // Create an existing symlink. It should be replaced by AppShimHostManager.
  base::FilePath user_data_dir;
  EXPECT_TRUE(base::PathService::Get(chrome::DIR_USER_DATA, &user_data_dir));
  symlink_path_ = user_data_dir.Append(app_mode::kAppShimSocketSymlinkName);
  base::FilePath temp_dir;
  base::PathService::Get(base::DIR_TEMP, &temp_dir);
  EXPECT_TRUE(base::CreateSymbolicLink(temp_dir.Append("chrome-XXXXXX"),
                                       symlink_path_));

  // Create an invalid RunningChromeVersion file.
  version_path_ =
      user_data_dir.Append(app_mode::kRunningChromeVersionSymlinkName);
  EXPECT_TRUE(base::CreateSymbolicLink(base::FilePath("invalid_version"),
                                       version_path_));
  return AppShimHostManagerBrowserTest::SetUpUserDataDirectory();
}

void AppShimHostManagerBrowserTestSocketFiles::
    TearDownInProcessBrowserTestFixture() {
  // Check that created files have been deleted.
  EXPECT_FALSE(base::PathExists(directory_in_tmp_));
  EXPECT_FALSE(base::PathExists(symlink_path_));
  EXPECT_FALSE(base::PathExists(version_path_));
}

IN_PROC_BROWSER_TEST_F(AppShimHostManagerBrowserTestSocketFiles,
                       ReplacesSymlinkAndCleansUpFiles) {
  // Get the directory created by AppShimHostManager.
  test::AppShimHostManagerTestApi test_api(
      g_browser_process->platform_part()->app_shim_host_manager());
  directory_in_tmp_ = test_api.directory_in_tmp();

  // Check that socket files have been created.
  base::ScopedAllowBlockingForTesting allow_blocking;
  EXPECT_TRUE(base::PathExists(directory_in_tmp_));
  EXPECT_TRUE(base::PathExists(symlink_path_));

  // Check that the symlink has been replaced.
  base::FilePath socket_path;
  ASSERT_TRUE(base::ReadSymbolicLink(symlink_path_, &socket_path));
  EXPECT_EQ(app_mode::kAppShimSocketShortName, socket_path.BaseName().value());

  // Check that the RunningChromeVersion file is correctly written.
  base::FilePath version;
  EXPECT_TRUE(base::ReadSymbolicLink(version_path_, &version));
  EXPECT_EQ(version_info::GetVersionNumber(), version.value());
}

}  // namespace
