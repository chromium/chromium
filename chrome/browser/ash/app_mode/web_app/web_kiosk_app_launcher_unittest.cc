// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_mode/web_app/web_kiosk_app_launcher.h"

#include <memory>

#include "base/auto_reset.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/test_future.h"
#include "chrome/browser/ash/app_mode/kiosk_app_manager_observer.h"
#include "chrome/browser/ash/app_mode/web_app/web_kiosk_app_data.h"
#include "chrome/browser/ash/app_mode/web_app/web_kiosk_app_manager.h"
#include "chrome/browser/ash/crosapi/browser_util.h"
#include "chrome/browser/ash/crosapi/fake_browser_manager.h"
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/web_applications/test/fake_data_retriever.h"
#include "chrome/browser/web_applications/test/test_web_app_url_loader.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/ash/components/network/network_handler_test_helper.h"
#include "components/exo/shell_surface_util.h"
#include "components/exo/wm_helper_chromeos.h"
#include "components/user_manager/scoped_user_manager.h"
#include "components/webapps/browser/install_result_code.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/browser_test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace ash {

namespace {

using ::base::test::TestFuture;
using ::testing::_;
using ::testing::Invoke;
using ::testing::Return;

// TODO(crbug/1379290): Use `TestFuture<void>` in all these tests
#define EXEC_AND_WAIT_FOR_CALL(exec, mock, method)    \
  ({                                                  \
    TestFuture<bool> waiter;                          \
    EXPECT_CALL(mock, method).WillOnce(Invoke([&]() { \
      waiter.SetValue(true);                          \
    }));                                              \
    exec;                                             \
    EXPECT_TRUE(waiter.Wait());                       \
  })

}  // namespace

class MockAppLauncherDelegate : public WebKioskAppLauncher::Delegate {
 public:
  MockAppLauncherDelegate() = default;
  ~MockAppLauncherDelegate() override = default;

  MOCK_METHOD0(InitializeNetwork, void());
  MOCK_METHOD0(OnAppInstalling, void());
  MOCK_METHOD0(OnAppPrepared, void());
  MOCK_METHOD0(OnAppLaunched, void());
  MOCK_METHOD0(OnAppWindowCreated, void());
  MOCK_METHOD1(OnLaunchFailed, void(KioskAppLaunchError::Error));

  MOCK_CONST_METHOD0(IsNetworkReady, bool());
  MOCK_CONST_METHOD0(IsShowingNetworkConfigScreen, bool());
};

const char kAppEmail[] = "lala@example.com";
const char kAppInstallUrl[] = "https://example.com";
const char kAppLaunchUrl[] = "https://example.com/launch";
const char kAppLaunchBadUrl[] = "https://badexample.com";
const char kLacrosAppId[] = "org.chromium.lacros.12345";
const char kUserEmail[] = "user@example.com";
const char16_t kAppTitle[] = u"app";

std::unique_ptr<web_app::WebAppDataRetriever> CreateDataRetrieverWithData(
    const GURL& url) {
  auto data_retriever = std::make_unique<web_app::FakeDataRetriever>();
  auto info = std::make_unique<WebAppInstallInfo>();
  info->start_url = url;
  info->title = kAppTitle;
  data_retriever->SetRendererWebAppInstallInfo(std::move(info));
  return std::unique_ptr<web_app::WebAppDataRetriever>(
      std::move(data_retriever));
}

class AppWindowCloser : public BrowserListObserver {
 public:
  AppWindowCloser() { BrowserList::AddObserver(this); }

  ~AppWindowCloser() override { BrowserList::RemoveObserver(this); }

  void OnBrowserAdded(Browser* browser) override { app_browser_ = browser; }

  void OnBrowserRemoved(Browser* browser) override {
    closed_waiter_.SetValue(true);
  }

  void Close() {
    DCHECK(app_browser_);
    app_browser_->tab_strip_model()->CloseAllTabs();
    delete app_browser_;

    EXPECT_TRUE(closed_waiter_.Wait());
  }

 private:
  Browser* app_browser_ = nullptr;
  // TODO(crbug/1379290): Use `TestFuture<void>` in all these tests
  TestFuture<bool> closed_waiter_;
};

class WebKioskAppLauncherTest : public BrowserWithTestWindowTest {
 public:
  WebKioskAppLauncherTest() : BrowserWithTestWindowTest() {}
  ~WebKioskAppLauncherTest() override {}

  void SetUp() override {
    BrowserWithTestWindowTest::SetUp();
    network_handler_test_helper_ = std::make_unique<NetworkHandlerTestHelper>();

    app_manager_ = std::make_unique<WebKioskAppManager>();

    ConstructLauncher(/*should_skip_install=*/false);

    closer_ = std::make_unique<AppWindowCloser>();
  }

  void TearDown() override {
    closer_.reset();
    launcher_.reset();
    app_manager_.reset();
    network_handler_test_helper_.reset();
    BrowserWithTestWindowTest::TearDown();
  }

  void ConstructLauncher(bool should_skip_install) {
    launcher_ = std::make_unique<WebKioskAppLauncher>(
        profile(), AccountId::FromUserEmail(kAppEmail), should_skip_install,
        &delegate_);
    launcher_->SetBrowserWindowForTesting(window());
    auto url_loader = std::make_unique<web_app::TestWebAppUrlLoader>();
    url_loader_ = url_loader.get();
    launcher_->SetUrlLoaderForTesting(std::move(url_loader));
  }

  void SetupAppData(bool installed) {
    account_id_ = AccountId::FromUserEmail(kAppEmail);
    app_manager_->AddAppForTesting(account_id_, GURL(kAppInstallUrl));

    if (installed) {
      WebAppInstallInfo info;
      info.start_url = GURL(kAppLaunchUrl);
      info.title = kAppTitle;
      app_manager_->UpdateAppByAccountId(account_id_, info);
    }
  }

  void SetupInstallData() {
    url_loader_->SetNextLoadUrlResult(
        GURL(kAppInstallUrl), web_app::WebAppUrlLoader::Result::kUrlLoaded);
    launcher_->SetDataRetrieverFactoryForTesting(
        base::BindRepeating(&CreateDataRetrieverWithData, GURL(kAppLaunchUrl)));
  }

  void SetupBadInstallData() {
    url_loader_->SetNextLoadUrlResult(
        GURL(kAppInstallUrl), web_app::WebAppUrlLoader::Result::kUrlLoaded);
    launcher_->SetDataRetrieverFactoryForTesting(base::BindRepeating(
        &CreateDataRetrieverWithData, GURL(kAppLaunchBadUrl)));
  }

  void SetupNotLoadedAppData() {
    url_loader_->SetNextLoadUrlResult(
        GURL(kAppInstallUrl),
        web_app::WebAppUrlLoader::Result::kFailedPageTookTooLong);
  }

  const WebKioskAppData* app_data() {
    return app_manager_->GetAppByAccountId(account_id_);
  }

  void CloseAppWindow() {
    // Wait for it to be closed.
    closer_->Close();
  }

  MockAppLauncherDelegate& delegate() { return delegate_; }
  KioskAppLauncher* launcher() { return launcher_.get(); }

 protected:
  AccountId account_id_;
  web_app::TestWebAppUrlLoader* url_loader_;  // Owned by |launcher_|.

 private:
  std::unique_ptr<WebKioskAppManager> app_manager_;
  std::unique_ptr<NetworkHandlerTestHelper> network_handler_test_helper_;

  MockAppLauncherDelegate delegate_;
  std::unique_ptr<WebKioskAppLauncher> launcher_;
  std::unique_ptr<AppWindowCloser> closer_;
};

TEST_F(WebKioskAppLauncherTest, NormalFlowNotInstalled) {
  SetupAppData(/*installed*/ false);
  ConstructLauncher(/*should_skip_install=*/false);

  EXEC_AND_WAIT_FOR_CALL(launcher()->Initialize(), delegate(),
                         InitializeNetwork());

  SetupInstallData();

  EXPECT_CALL(delegate(), OnAppInstalling());
  EXEC_AND_WAIT_FOR_CALL(launcher()->ContinueWithNetworkReady(), delegate(),
                         OnAppPrepared());

  EXPECT_EQ(app_data()->status(), WebKioskAppData::Status::kInstalled);
  EXPECT_EQ(app_data()->launch_url(), kAppLaunchUrl);

  EXEC_AND_WAIT_FOR_CALL(launcher()->LaunchApp(), delegate(), OnAppLaunched());

  CloseAppWindow();
}

TEST_F(WebKioskAppLauncherTest, NormalFlowAlreadyInstalled) {
  SetupAppData(/*installed*/ true);

  EXEC_AND_WAIT_FOR_CALL(launcher()->Initialize(), delegate(), OnAppPrepared());

  EXEC_AND_WAIT_FOR_CALL(launcher()->LaunchApp(), delegate(), OnAppLaunched());

  CloseAppWindow();
}

TEST_F(WebKioskAppLauncherTest, NormalFlowBadLaunchUrl) {
  SetupAppData(/*installed*/ false);

  EXEC_AND_WAIT_FOR_CALL(launcher()->Initialize(), delegate(),
                         InitializeNetwork());

  SetupBadInstallData();

  EXPECT_CALL(delegate(), OnAppInstalling());
  EXEC_AND_WAIT_FOR_CALL(
      launcher()->ContinueWithNetworkReady(), delegate(),
      OnLaunchFailed((KioskAppLaunchError::Error::kUnableToLaunch)));

  EXPECT_NE(app_data()->status(), WebKioskAppData::Status::kInstalled);
}

TEST_F(WebKioskAppLauncherTest, InstallationRestarted) {
  SetupAppData(/*installed*/ false);
  // Freezes url requests until they are manually processed.
  url_loader_->SaveLoadUrlRequests();

  EXEC_AND_WAIT_FOR_CALL(launcher()->Initialize(), delegate(),
                         InitializeNetwork());

  SetupInstallData();

  EXPECT_CALL(delegate(), OnAppInstalling());
  launcher()->ContinueWithNetworkReady();

  EXPECT_CALL(delegate(), InitializeNetwork()).Times(1);
  launcher()->RestartLauncher();

  // App should not be installed yet.
  EXPECT_NE(app_data()->status(), WebKioskAppData::Status::kInstalled);

  // We should not receive any status updates now.
  url_loader_->ProcessLoadUrlRequests();

  SetupInstallData();

  EXPECT_CALL(delegate(), OnAppInstalling()).Times(1);
  EXEC_AND_WAIT_FOR_CALL(
      {
        launcher()->ContinueWithNetworkReady();
        url_loader_->ProcessLoadUrlRequests();
      },
      delegate(), OnAppPrepared());

  EXPECT_EQ(app_data()->status(), WebKioskAppData::Status::kInstalled);

  EXEC_AND_WAIT_FOR_CALL(launcher()->LaunchApp(), delegate(), OnAppLaunched());

  CloseAppWindow();
}

TEST_F(WebKioskAppLauncherTest, UrlNotLoaded) {
  base::HistogramTester histogram;

  SetupAppData(/*installed*/ false);

  EXEC_AND_WAIT_FOR_CALL(launcher()->Initialize(), delegate(),
                         InitializeNetwork());

  SetupNotLoadedAppData();

  EXPECT_CALL(delegate(), OnAppInstalling());
  EXEC_AND_WAIT_FOR_CALL(
      launcher()->ContinueWithNetworkReady(), delegate(),
      OnLaunchFailed(KioskAppLaunchError::Error::kUnableToInstall));

  EXPECT_NE(app_data()->status(), WebKioskAppData::Status::kInstalled);

  content::FetchHistogramsFromChildProcesses();
  histogram.ExpectUniqueSample(
      "Kiosk.WebApp.InstallError",
      webapps::InstallResultCode::kInstallURLLoadTimeOut, 1);
}

TEST_F(WebKioskAppLauncherTest, SkipInstallation) {
  SetupAppData(/*installed*/ false);

  ConstructLauncher(/*should_skip_install=*/true);

  EXEC_AND_WAIT_FOR_CALL(launcher()->Initialize(), delegate(), OnAppPrepared());

  EXPECT_EQ(app_data()->status(), WebKioskAppData::Status::kInit);
  EXPECT_EQ(app_data()->launch_url(), GURL());

  EXEC_AND_WAIT_FOR_CALL(launcher()->LaunchApp(), delegate(), OnAppLaunched());

  CloseAppWindow();
}

class WebKioskAppLauncherUsingLacrosTest : public WebKioskAppLauncherTest {
 public:
  WebKioskAppLauncherUsingLacrosTest()
      : browser_manager_(std::make_unique<crosapi::FakeBrowserManager>()),
        fake_user_manager_(new FakeChromeUserManager()),
        scoped_user_manager_(base::WrapUnique(fake_user_manager_)),
        wm_helper_(std::make_unique<exo::WMHelperChromeOS>()) {
    scoped_feature_list_.InitAndEnableFeature(features::kWebKioskEnableLacros);
  }

  void LoginWebKioskUser() {
    const AccountId account_id(AccountId::FromUserEmail(kUserEmail));
    fake_user_manager()->AddWebKioskAppUser(account_id);
    fake_user_manager()->LoginUser(account_id);
  }

  void CreateLacrosWindowAndNotify() {
    auto window = std::make_unique<aura::Window>(nullptr);
    window->Init(ui::LAYER_SOLID_COLOR);
    exo::SetShellApplicationId(window.get(), kLacrosAppId);
    wm_helper()->NotifyExoWindowCreated(window.get());
  }

  crosapi::FakeBrowserManager* browser_manager() const {
    return browser_manager_.get();
  }

  FakeChromeUserManager* fake_user_manager() const {
    return fake_user_manager_;
  }

  exo::WMHelper* wm_helper() const { return wm_helper_.get(); }

 private:
  base::AutoReset<bool> set_lacros_enabled_ =
      crosapi::browser_util::SetLacrosEnabledForTest(true);
  base::AutoReset<absl::optional<bool>> set_lacros_primary_ =
      crosapi::browser_util::SetLacrosPrimaryBrowserForTest(true);
  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<crosapi::FakeBrowserManager> browser_manager_;
  FakeChromeUserManager* fake_user_manager_;
  user_manager::ScopedUserManager scoped_user_manager_;
  std::unique_ptr<exo::WMHelper> wm_helper_;
};

TEST_F(WebKioskAppLauncherUsingLacrosTest, NormalFlow) {
  LoginWebKioskUser();
  SetupAppData(/*installed*/ true);
  browser_manager()->set_new_fullscreen_window_creation_result(
      crosapi::mojom::CreationResult::kSuccess);

  EXEC_AND_WAIT_FOR_CALL(launcher()->Initialize(), delegate(), OnAppPrepared());

  // The browser manager is running before launching app. The
  // `OnAppWindowCreated` method will be called after the lacros-chrome window
  // is created successfully.
  EXPECT_CALL(delegate(), OnAppLaunched()).Times(1);
  browser_manager()->set_is_running(true);
  launcher()->LaunchApp();

  EXEC_AND_WAIT_FOR_CALL(CreateLacrosWindowAndNotify(), delegate(),
                         OnAppWindowCreated());
  EXPECT_CALL(delegate(), OnLaunchFailed(_)).Times(0);
}

TEST_F(WebKioskAppLauncherUsingLacrosTest, WaitBrowserManagerToRun) {
  LoginWebKioskUser();
  SetupAppData(/*installed*/ true);
  browser_manager()->set_new_fullscreen_window_creation_result(
      crosapi::mojom::CreationResult::kSuccess);

  EXEC_AND_WAIT_FOR_CALL(launcher()->Initialize(), delegate(), OnAppPrepared());

  // The browser manager is not running before launching app. The crosapi call
  // will pend until it is ready. The `OnAppWindowCreated` method will be called
  // after the lacros-chrome window is created successfully.
  EXPECT_CALL(delegate(), OnAppLaunched()).Times(1);
  browser_manager()->set_is_running(false);
  launcher()->LaunchApp();
  browser_manager()->set_is_running(true);
  browser_manager()->StartRunning();

  EXEC_AND_WAIT_FOR_CALL(CreateLacrosWindowAndNotify(), delegate(),
                         OnAppWindowCreated());
  EXPECT_CALL(delegate(), OnLaunchFailed(_)).Times(0);
}

TEST_F(WebKioskAppLauncherUsingLacrosTest, FailToLaunchApp) {
  LoginWebKioskUser();
  SetupAppData(/*installed*/ true);
  browser_manager()->set_new_fullscreen_window_creation_result(
      crosapi::mojom::CreationResult::kBrowserNotRunning);

  EXEC_AND_WAIT_FOR_CALL(launcher()->Initialize(), delegate(), OnAppPrepared());

  // If the lacros-chrome window fails to be created, the `OnLaunchFailed`
  // method will be called instead.

  EXPECT_CALL(delegate(), OnAppLaunched()).Times(1);
  EXPECT_CALL(delegate(), OnAppWindowCreated()).Times(0);
  browser_manager()->set_is_running(true);

  EXEC_AND_WAIT_FOR_CALL(launcher()->LaunchApp(), delegate(),
                         OnLaunchFailed(_));
}

}  // namespace ash
