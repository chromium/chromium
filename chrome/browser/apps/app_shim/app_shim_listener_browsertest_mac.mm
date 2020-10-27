// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_shim/app_shim_listener.h"

#include <unistd.h>

#include "base/bind.h"
#include "base/check.h"
#include "base/files/file_path.h"
#include "base/mac/foundation_util.h"
#include "base/macros.h"
#include "base/optional.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/threading/thread_restrictions.h"
#include "chrome/app_shim/app_shim_controller.h"
#include "chrome/browser/apps/app_shim/app_shim_host_bootstrap_mac.h"
#include "chrome/browser/apps/app_shim/test/app_shim_listener_test_api_mac.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/mac/app_mode_common.h"
#include "chrome/common/mac/app_shim.mojom.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/version_info/version_info.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_utils.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/platform/named_platform_channel.h"
#include "mojo/public/cpp/platform/platform_channel.h"
#include "mojo/public/cpp/system/isolated_connection.h"

using OnShimConnectedCallback =
    chrome::mojom::AppShimHostBootstrap::OnShimConnectedCallback;

const char kTestAppMode[] = "test_app";

// TODO(https://crbug.com/1042727): Fix test GURL scoping and remove this getter
// function.
GURL TestAppUrl() {
  return GURL("https://example.com");
}

// A test version of the AppShimController mojo client in chrome_main_app_mode.
class TestShimClient : public chrome::mojom::AppShim {
 public:
  TestShimClient();

  // Friend accessor.
  mojo::PlatformChannelEndpoint ConnectToBrowser(
      const mojo::NamedPlatformChannel::ServerName& server_name) {
    return AppShimController::ConnectToBrowser(server_name);
  }

  mojo::PendingReceiver<chrome::mojom::AppShimHost> GetHostReceiver() {
    return std::move(host_receiver_);
  }
  OnShimConnectedCallback GetOnShimConnectedCallback() {
    return base::BindOnce(&TestShimClient::OnShimConnectedDone,
                          base::Unretained(this));
  }

  mojo::Remote<chrome::mojom::AppShimHostBootstrap>& host_bootstrap() {
    return host_bootstrap_;
  }

  // chrome::mojom::AppShim implementation (not used in testing, but can be).
  void CreateRemoteCocoaApplication(
      mojo::PendingAssociatedReceiver<remote_cocoa::mojom::Application>
          receiver) override {}
  void CreateCommandDispatcherForWidget(uint64_t widget_id) override {}
  void SetUserAttention(
      chrome::mojom::AppShimAttentionType attention_type) override {}
  void SetBadgeLabel(const std::string& badge_label) override {}
  void UpdateProfileMenu(std::vector<chrome::mojom::ProfileMenuItemPtr>
                             profile_menu_items) override {}

 private:
  void OnShimConnectedDone(
      chrome::mojom::AppShimLaunchResult result,
      mojo::PendingReceiver<chrome::mojom::AppShim> app_shim_receiver) {
    shim_receiver_.Bind(std::move(app_shim_receiver));
  }

  mojo::IsolatedConnection mojo_connection_;
  mojo::Receiver<chrome::mojom::AppShim> shim_receiver_{this};
  mojo::Remote<chrome::mojom::AppShimHost> host_;
  mojo::PendingReceiver<chrome::mojom::AppShimHost> host_receiver_;
  mojo::Remote<chrome::mojom::AppShimHostBootstrap> host_bootstrap_;

  DISALLOW_COPY_AND_ASSIGN(TestShimClient);
};

TestShimClient::TestShimClient()
    : host_receiver_(host_.BindNewPipeAndPassReceiver()) {
  base::FilePath user_data_dir;
  CHECK(base::PathService::Get(chrome::DIR_USER_DATA, &user_data_dir));

  std::string name_fragment =
      base::StringPrintf("%s.%s.%s", base::mac::BaseBundleID(),
                         app_mode::kAppShimBootstrapNameFragment,
                         base::MD5String(user_data_dir.value()).c_str());
  mojo::PlatformChannelEndpoint endpoint = ConnectToBrowser(name_fragment);

  mojo::ScopedMessagePipeHandle message_pipe =
      mojo_connection_.Connect(std::move(endpoint));
  host_bootstrap_ = mojo::Remote<chrome::mojom::AppShimHostBootstrap>(
      mojo::PendingRemote<chrome::mojom::AppShimHostBootstrap>(
          std::move(message_pipe), 0));
}

// Browser Test for AppShimListener to test IPC interactions.
class AppShimListenerBrowserTest : public InProcessBrowserTest,
                                   public AppShimHostBootstrap::Client,
                                   public chrome::mojom::AppShimHost {
 public:
  AppShimListenerBrowserTest() = default;

 protected:
  // Wait for OnShimProcessConnected, then send a quit, and wait for the
  // response. Used to test launch behavior.
  void RunAndExitGracefully();

  // InProcessBrowserTest overrides:
  void SetUpOnMainThread() override;
  void TearDownOnMainThread() override;

  // AppShimHostBootstrap::Client:
  void OnShimProcessConnected(
      std::unique_ptr<AppShimHostBootstrap> bootstrap) override;

  std::unique_ptr<TestShimClient> test_client_;
  std::vector<base::FilePath> last_launch_files_;
  base::Optional<chrome::mojom::AppShimLaunchType> last_launch_type_;

 private:
  // chrome::mojom::AppShimHost.
  void FocusApp() override {}
  void ReopenApp() override {}
  void FilesOpened(const std::vector<base::FilePath>& files) override {}
  void ProfileSelectedFromMenu(const base::FilePath& profile_path) override {}

  std::unique_ptr<base::RunLoop> runner_;
  mojo::Receiver<chrome::mojom::AppShimHost> receiver_{this};
  mojo::Remote<chrome::mojom::AppShim> app_shim_;

  int launch_count_ = 0;

  DISALLOW_COPY_AND_ASSIGN(AppShimListenerBrowserTest);
};

void AppShimListenerBrowserTest::RunAndExitGracefully() {
  runner_ = std::make_unique<base::RunLoop>();
  EXPECT_EQ(0, launch_count_);
  runner_->Run();  // Will stop in OnShimProcessConnected().
  EXPECT_EQ(1, launch_count_);
  test_client_.reset();
}

void AppShimListenerBrowserTest::SetUpOnMainThread() {
  // Can't do this in the constructor, it needs a BrowserProcess.
  AppShimHostBootstrap::SetClient(this);
}

void AppShimListenerBrowserTest::TearDownOnMainThread() {
  AppShimHostBootstrap::SetClient(nullptr);
}

void AppShimListenerBrowserTest::OnShimProcessConnected(
    std::unique_ptr<AppShimHostBootstrap> bootstrap) {
  ++launch_count_;
  receiver_.Bind(bootstrap->GetAppShimHostReceiver());
  last_launch_type_ = bootstrap->GetLaunchType();
  last_launch_files_ = bootstrap->GetLaunchFiles();

  bootstrap->OnConnectedToHost(app_shim_.BindNewPipeAndPassReceiver());
  runner_->Quit();
}

// Test regular launch, which would ask Chrome to launch the app.
IN_PROC_BROWSER_TEST_F(AppShimListenerBrowserTest, LaunchNormal) {
  test_client_.reset(new TestShimClient());
  auto app_shim_info = chrome::mojom::AppShimInfo::New();
  app_shim_info->profile_path = browser()->profile()->GetPath();
  app_shim_info->app_id = kTestAppMode;
  app_shim_info->app_url = TestAppUrl();
  app_shim_info->launch_type = chrome::mojom::AppShimLaunchType::kNormal;
  test_client_->host_bootstrap()->OnShimConnected(
      test_client_->GetHostReceiver(), std::move(app_shim_info),
      test_client_->GetOnShimConnectedCallback());
  RunAndExitGracefully();
  EXPECT_EQ(chrome::mojom::AppShimLaunchType::kNormal, last_launch_type_);
  EXPECT_TRUE(last_launch_files_.empty());
}

// Test register-only launch, used when Chrome has already launched the app.
IN_PROC_BROWSER_TEST_F(AppShimListenerBrowserTest, LaunchRegisterOnly) {
  test_client_.reset(new TestShimClient());
  auto app_shim_info = chrome::mojom::AppShimInfo::New();
  app_shim_info->profile_path = browser()->profile()->GetPath();
  app_shim_info->app_id = kTestAppMode;
  app_shim_info->app_url = TestAppUrl();
  app_shim_info->launch_type = chrome::mojom::AppShimLaunchType::kRegisterOnly;
  test_client_->host_bootstrap()->OnShimConnected(
      test_client_->GetHostReceiver(), std::move(app_shim_info),
      test_client_->GetOnShimConnectedCallback());

  RunAndExitGracefully();
  EXPECT_EQ(chrome::mojom::AppShimLaunchType::kRegisterOnly,
            *last_launch_type_);
  EXPECT_TRUE(last_launch_files_.empty());
}

// Ensure bootstrap name registers.
IN_PROC_BROWSER_TEST_F(AppShimListenerBrowserTest, PRE_ReCreate) {
  test::AppShimListenerTestApi test_api(
      g_browser_process->platform_part()->app_shim_listener());
  EXPECT_TRUE(test_api.mach_acceptor());
}

// Ensure the bootstrap name can be re-created after a prior browser process has
// quit.
IN_PROC_BROWSER_TEST_F(AppShimListenerBrowserTest, ReCreate) {
  test::AppShimListenerTestApi test_api(
      g_browser_process->platform_part()->app_shim_listener());
  EXPECT_TRUE(test_api.mach_acceptor());
}

// Tests for the files created by AppShimListener.
class AppShimListenerBrowserTestSymlink : public AppShimListenerBrowserTest {
 public:
  AppShimListenerBrowserTestSymlink() {}

 protected:
  base::FilePath version_path_;

 private:
  bool SetUpUserDataDirectory() override;
  void TearDownInProcessBrowserTestFixture() override;

  DISALLOW_COPY_AND_ASSIGN(AppShimListenerBrowserTestSymlink);
};

bool AppShimListenerBrowserTestSymlink::SetUpUserDataDirectory() {
  // Create an existing symlink. It should be replaced by AppShimListener.
  base::FilePath user_data_dir;
  EXPECT_TRUE(base::PathService::Get(chrome::DIR_USER_DATA, &user_data_dir));

  // Create an invalid RunningChromeVersion file.
  version_path_ =
      user_data_dir.Append(app_mode::kRunningChromeVersionSymlinkName);
  EXPECT_TRUE(base::CreateSymbolicLink(base::FilePath("invalid_version"),
                                       version_path_));
  return AppShimListenerBrowserTest::SetUpUserDataDirectory();
}

void AppShimListenerBrowserTestSymlink::TearDownInProcessBrowserTestFixture() {
  // Check that created files have been deleted.
  EXPECT_FALSE(base::PathExists(version_path_));
}

IN_PROC_BROWSER_TEST_F(AppShimListenerBrowserTestSymlink,
                       RunningChromeVersionCorrectlyWritten) {
  // Check that the RunningChromeVersion file is correctly written.
  base::FilePath version;
  EXPECT_TRUE(base::ReadSymbolicLink(version_path_, &version));
  EXPECT_EQ(version_info::GetVersionNumber(), version.value());
}
