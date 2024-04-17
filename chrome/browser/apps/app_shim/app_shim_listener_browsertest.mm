// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_shim/app_shim_listener.h"

#include <unistd.h>

#include <memory>
#include <optional>

#include "base/apple/foundation_util.h"
#include "base/check.h"
#include "base/check_op.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/strings/strcat.h"
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
#include "mojo/core/embedder/embedder.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/platform/named_platform_channel.h"
#include "mojo/public/cpp/platform/platform_channel.h"
#include "mojo/public/cpp/system/handle.h"
#include "mojo/public/cpp/system/isolated_connection.h"
#include "third_party/ipcz/include/ipcz/ipcz.h"

// A test version of the AppShimController mojo client in chrome_main_app_mode.
class TestShimClient : public chrome::mojom::AppShim {
 public:
  TestShimClient();
  TestShimClient(const TestShimClient&) = delete;
  TestShimClient& operator=(const TestShimClient&) = delete;

  // Friend accessor.
  mojo::PlatformChannelEndpoint ConnectToBrowser(
      const mojo::NamedPlatformChannel::ServerName& server_name) {
    return AppShimController::ConnectToBrowser(server_name);
  }

  mojo::PendingReceiver<chrome::mojom::AppShimHost> GetHostReceiver() {
    return std::move(host_receiver_);
  }

  chrome::mojom::AppShimHostBootstrap::OnShimConnectedCallback
  GetOnShimConnectedCallback() {
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
  void UpdateApplicationDockMenu(
      std::vector<chrome::mojom::ApplicationDockMenuItemPtr> dock_menu_items)
      override {}
  void BindNotificationProvider(
      mojo::PendingReceiver<mac_notifications::mojom::MacNotificationProvider>
          provider) override {}
  void RequestNotificationPermission(
      RequestNotificationPermissionCallback callback) override {}
  void BindChildHistogramFetcherFactory(
      mojo::PendingReceiver<metrics::mojom::ChildHistogramFetcherFactory>
          receiver) override {}

 private:
  void OnShimConnectedDone(
      chrome::mojom::AppShimLaunchResult result,
      variations::VariationsCommandLine feature_state,
      mojo::PendingReceiver<chrome::mojom::AppShim> app_shim_receiver) {
    shim_receiver_.Bind(std::move(app_shim_receiver));
  }

  mojo::ScopedMessagePipeHandle ConnectIcpzToShim(
      mojo::PlatformChannelEndpoint endpoint) {
    // ipcz does not support nodes connecting to themselves, as these tests
    // normally do. Instead for ipcz we set up a secondary broker node and use
    // that to connect back to Mojo's global ipcz node in this process,
    // effectively simulating an external shim process.
    //
    // Note that ipcz handles and Mojo handles are interchangeable types with
    // ipcz enabled, so we can use scoped Mojo handles to manage ipcz object
    // lifetime here.
    const IpczAPI& ipcz = mojo::core::GetIpczAPIForMojo();
    IpczHandle node;
    IpczResult result =
        ipcz.CreateNode(&mojo::core::GetIpczDriverForMojo(),
                        IPCZ_CREATE_NODE_AS_BROKER, nullptr, &node);
    CHECK_EQ(IPCZ_RESULT_OK, result);
    secondary_ipcz_broker_.reset(mojo::Handle{node});

    // MojoIpcz reserves the first portal on each invitation connection for
    // internal services. We discard it here since it's not needed.
    IpczHandle portals[2];
    result = ipcz.ConnectNode(
        secondary_ipcz_broker_->value(),
        mojo::core::CreateIpczTransportFromEndpoint(
            std::move(endpoint),
            {.local_is_broker = true, .remote_is_broker = true}),
        /*num_initial_portals=*/2, IPCZ_CONNECT_NODE_TO_BROKER,
        /*options=*/nullptr, portals);
    CHECK_EQ(IPCZ_RESULT_OK, result);
    ipcz.Close(portals[0], IPCZ_NO_FLAGS, nullptr);
    return mojo::ScopedMessagePipeHandle(mojo::MessagePipeHandle(portals[1]));
  }

  mojo::IsolatedConnection mojo_connection_;
  mojo::ScopedHandle secondary_ipcz_broker_;
  mojo::Receiver<chrome::mojom::AppShim> shim_receiver_{this};
  mojo::Remote<chrome::mojom::AppShimHost> host_;
  mojo::PendingReceiver<chrome::mojom::AppShimHost> host_receiver_;
  mojo::Remote<chrome::mojom::AppShimHostBootstrap> host_bootstrap_;
};

TestShimClient::TestShimClient() {
  base::FilePath user_data_dir;
  CHECK(base::PathService::Get(chrome::DIR_USER_DATA, &user_data_dir));

  std::string name_fragment =
      base::StrCat({base::apple::BaseBundleID(), ".",
                    app_mode::kAppShimBootstrapNameFragment, ".",
                    base::MD5String(user_data_dir.value())});
  mojo::PlatformChannelEndpoint endpoint = ConnectToBrowser(name_fragment);

  mojo::ScopedMessagePipeHandle message_pipe;
  if (mojo::core::IsMojoIpczEnabled()) {
    // With MojoIpcz, we need to set up a secondary node in order to simulate an
    // external shim connection.
    message_pipe = ConnectIcpzToShim(std::move(endpoint));

    // It's important for the AppShimHost interface portals to be created on the
    // secondary node too, since the fake shim passes the receiver endpoint back
    // to the host in a reply over the primordial AppShim interface connecting
    // the two nodes.
    const IpczAPI& ipcz = mojo::core::GetIpczAPIForMojo();
    IpczHandle remote, receiver;
    const IpczResult result =
        ipcz.OpenPortals(secondary_ipcz_broker_->value(), IPCZ_NO_FLAGS,
                         nullptr, &remote, &receiver);
    CHECK_EQ(IPCZ_RESULT_OK, result);
    host_.Bind(mojo::PendingRemote<chrome::mojom::AppShimHost>(
        mojo::ScopedMessagePipeHandle(mojo::MessagePipeHandle(remote)), 0));
    host_receiver_ = mojo::PendingReceiver<chrome::mojom::AppShimHost>(
        mojo::ScopedMessagePipeHandle(mojo::MessagePipeHandle(receiver)));
  } else {
    // Non-ipcz Mojo supports processes establishing IsolatedConnections to
    // themselves.
    message_pipe = mojo_connection_.Connect(std::move(endpoint));
    host_receiver_ = host_.BindNewPipeAndPassReceiver();
  }
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
  AppShimListenerBrowserTest(const AppShimListenerBrowserTest&) = delete;
  AppShimListenerBrowserTest& operator=(const AppShimListenerBrowserTest&) =
      delete;

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
  std::optional<chrome::mojom::AppShimLaunchType> last_launch_type_;

 private:
  // chrome::mojom::AppShimHost.
  void FocusApp() override {}
  void ReopenApp() override {}
  void FilesOpened(const std::vector<base::FilePath>& files) override {}
  void ProfileSelectedFromMenu(const base::FilePath& profile_path) override {}
  void OpenAppSettings() override {}
  void UrlsOpened(const std::vector<GURL>& urls) override {}
  void OpenAppWithOverrideUrl(const GURL& override_url) override {}
  void EnableAccessibilitySupport(
      chrome::mojom::AppShimScreenReaderSupportMode mode) override {}
  void ApplicationWillTerminate() override {}
  void NotificationPermissionStatusChanged(
      mac_notifications::mojom::PermissionStatus status) override {}

  std::unique_ptr<base::RunLoop> runner_;
  mojo::Receiver<chrome::mojom::AppShimHost> receiver_{this};
  mojo::Remote<chrome::mojom::AppShim> app_shim_;

  int launch_count_ = 0;
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
  test_client_ = std::make_unique<TestShimClient>();
  auto app_shim_info = chrome::mojom::AppShimInfo::New();
  app_shim_info->profile_path = browser()->profile()->GetPath();
  app_shim_info->app_id = "test_app";
  app_shim_info->app_url = GURL("https://example.com");
  app_shim_info->launch_type = chrome::mojom::AppShimLaunchType::kNormal;
  app_shim_info->notification_action_handler =
      mojo::PendingRemote<
          mac_notifications::mojom::MacNotificationActionHandler>()
          .InitWithNewPipeAndPassReceiver();
  test_client_->host_bootstrap()->OnShimConnected(
      test_client_->GetHostReceiver(), std::move(app_shim_info),
      test_client_->GetOnShimConnectedCallback());
  RunAndExitGracefully();
  EXPECT_EQ(chrome::mojom::AppShimLaunchType::kNormal, last_launch_type_);
  EXPECT_TRUE(last_launch_files_.empty());
}

// Test register-only launch, used when Chrome has already launched the app.
IN_PROC_BROWSER_TEST_F(AppShimListenerBrowserTest, LaunchRegisterOnly) {
  test_client_ = std::make_unique<TestShimClient>();
  auto app_shim_info = chrome::mojom::AppShimInfo::New();
  app_shim_info->profile_path = browser()->profile()->GetPath();
  app_shim_info->app_id = "test_app";
  app_shim_info->app_url = GURL("https://example.com");
  app_shim_info->launch_type = chrome::mojom::AppShimLaunchType::kRegisterOnly;
  app_shim_info->notification_action_handler =
      mojo::PendingRemote<
          mac_notifications::mojom::MacNotificationActionHandler>()
          .InitWithNewPipeAndPassReceiver();
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
  AppShimListenerBrowserTestSymlink() = default;
  AppShimListenerBrowserTestSymlink(const AppShimListenerBrowserTestSymlink&) =
      delete;
  AppShimListenerBrowserTestSymlink& operator=(
      const AppShimListenerBrowserTestSymlink&) = delete;

 protected:
  base::FilePath version_path_;

 private:
  bool SetUpUserDataDirectory() override;
  void TearDownInProcessBrowserTestFixture() override;
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
  base::FilePath encoded_config;
  EXPECT_TRUE(base::ReadSymbolicLink(version_path_, &encoded_config));
  auto config =
      app_mode::ChromeConnectionConfig::DecodeFromPath(encoded_config);
  EXPECT_EQ(version_info::GetVersionNumber(), config.framework_version);
  EXPECT_EQ(mojo::core::IsMojoIpczEnabled(), config.is_mojo_ipcz_enabled);
}
