// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_mode/web_app/web_kiosk_app_launcher.h"

#include <memory>

#include "base/strings/utf_string_conversions.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/ash/app_mode/kiosk_app_manager_observer.h"
#include "chrome/browser/ash/app_mode/web_app/web_kiosk_app_data.h"
#include "chrome/browser/ash/app_mode/web_app/web_kiosk_app_manager.h"
#include "chrome/browser/ash/crosapi/fake_browser_manager.h"
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/web_applications/test/fake_data_retriever.h"
#include "chrome/browser/web_applications/test/test_web_app_url_loader.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "chrome/test/base/testing_profile.h"
#include "components/exo/shell_surface_util.h"
#include "components/exo/wm_helper_chromeos.h"
#include "components/user_manager/scoped_user_manager.h"
#include "components/webapps/browser/install_result_code.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/browser_test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using ::base::test::RunClosure;
using ::testing::_;
using ::testing::Return;

namespace ash {

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
  MOCK_CONST_METHOD0(ShouldSkipAppInstallation, bool());
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
    closed_ = true;
    waiter.Quit();
  }

  void Close() {
    DCHECK(app_browser_);
    app_browser_->tab_strip_model()->CloseAllTabs();
    delete app_browser_;
    if (!closed_)
      waiter.Run();
  }

 private:
  bool closed_ = false;
  Browser* app_browser_ = nullptr;
  base::RunLoop waiter;
};

class WebKioskAppLauncherTest : public BrowserWithTestWindowTest {
 public:
  WebKioskAppLauncherTest() : BrowserWithTestWindowTest() {}
  ~WebKioskAppLauncherTest() override {}

  void SetUp() override {
    BrowserWithTestWindowTest::SetUp();
    app_manager_ = std::make_unique<WebKioskAppManager>();
    delegate_ = std::make_unique<MockAppLauncherDelegate>();
    launcher_ = std::make_unique<WebKioskAppLauncher>(
        profile(), delegate_.get(), AccountId::FromUserEmail(kAppEmail));

    launcher_->SetBrowserWindowForTesting(window());
    url_loader_ = new web_app::TestWebAppUrlLoader();
    launcher_->SetUrlLoaderForTesting(
        std::unique_ptr<web_app::TestWebAppUrlLoader>(url_loader_));

    closer_ = std::make_unique<AppWindowCloser>();
  }

  void TearDown() override {
    closer_.reset();
    launcher_.reset();
    delegate_.reset();
    app_manager_.reset();
    BrowserWithTestWindowTest::TearDown();
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

  MockAppLauncherDelegate* delegate() { return delegate_.get(); }
  KioskAppLauncher* launcher() { return launcher_.get(); }

 protected:
  AccountId account_id_;
  web_app::TestWebAppUrlLoader* url_loader_;  // Owned by |launcher_|.

 private:
  std::unique_ptr<WebKioskAppManager> app_manager_;

  std::unique_ptr<MockAppLauncherDelegate> delegate_;
  std::unique_ptr<WebKioskAppLauncher> launcher_;
  std::unique_ptr<AppWindowCloser> closer_;
};

TEST_F(WebKioskAppLauncherTest, NormalFlowNotInstalled) {
  SetupAppData(/*installed*/ false);

  base::RunLoop loop1;
  EXPECT_CALL(*delegate(), ShouldSkipAppInstallation()).WillOnce(Return(false));
  EXPECT_CALL(*delegate(), InitializeNetwork())
      .WillOnce(RunClosure(loop1.QuitClosure()));
  launcher()->Initialize();
  loop1.Run();

  SetupInstallData();

  base::RunLoop loop2;
  EXPECT_CALL(*delegate(), OnAppInstalling());
  EXPECT_CALL(*delegate(), OnAppPrepared())
      .WillOnce(RunClosure(loop2.QuitClosure()));
  launcher()->ContinueWithNetworkReady();
  loop2.Run();

  EXPECT_EQ(app_data()->status(), WebKioskAppData::Status::kInstalled);
  EXPECT_EQ(app_data()->launch_url(), kAppLaunchUrl);

  base::RunLoop loop3;
  EXPECT_CALL(*delegate(), OnAppLaunched())
      .WillOnce(RunClosure(loop3.QuitClosure()));
  launcher()->LaunchApp();
  loop3.Run();

  CloseAppWindow();
}

TEST_F(WebKioskAppLauncherTest, NormalFlowAlreadyInstalled) {
  SetupAppData(/*installed*/ true);
  base::RunLoop loop1;
  EXPECT_CALL(*delegate(), OnAppPrepared())
      .WillOnce(RunClosure(loop1.QuitClosure()));
  launcher()->Initialize();
  loop1.Run();

  base::RunLoop loop2;
  EXPECT_CALL(*delegate(), OnAppLaunched())
      .WillOnce(RunClosure(loop2.QuitClosure()));
  launcher()->LaunchApp();
  loop2.Run();

  CloseAppWindow();
}

TEST_F(WebKioskAppLauncherTest, NormalFlowBadLaunchUrl) {
  SetupAppData(/*installed*/ false);

  base::RunLoop loop1;
  EXPECT_CALL(*delegate(), ShouldSkipAppInstallation()).WillOnce(Return(false));
  EXPECT_CALL(*delegate(), InitializeNetwork())
      .WillOnce(RunClosure(loop1.QuitClosure()));
  launcher()->Initialize();
  loop1.Run();

  SetupBadInstallData();

  base::RunLoop loop2;
  EXPECT_CALL(*delegate(), OnAppInstalling());
  EXPECT_CALL(*delegate(),
              OnLaunchFailed((KioskAppLaunchError::Error::kUnableToLaunch)))
      .WillOnce(RunClosure(loop2.QuitClosure()));
  launcher()->ContinueWithNetworkReady();
  loop2.Run();

  EXPECT_NE(app_data()->status(), WebKioskAppData::Status::kInstalled);
}

TEST_F(WebKioskAppLauncherTest, InstallationRestarted) {
  SetupAppData(/*installed*/ false);
  // Freezes url requests until they are manually processed.
  url_loader_->SaveLoadUrlRequests();

  base::RunLoop loop1;
  EXPECT_CALL(*delegate(), ShouldSkipAppInstallation()).WillOnce(Return(false));
  EXPECT_CALL(*delegate(), InitializeNetwork())
      .WillOnce(RunClosure(loop1.QuitClosure()));
  launcher()->Initialize();
  loop1.Run();

  SetupInstallData();

  EXPECT_CALL(*delegate(), OnAppInstalling());
  launcher()->ContinueWithNetworkReady();

  EXPECT_CALL(*delegate(), ShouldSkipAppInstallation()).WillOnce(Return(false));
  EXPECT_CALL(*delegate(), InitializeNetwork()).Times(1);
  launcher()->RestartLauncher();

  // App should not be installed yet.
  EXPECT_NE(app_data()->status(), WebKioskAppData::Status::kInstalled);

  // We should not receive any status updates now.
  url_loader_->ProcessLoadUrlRequests();

  SetupInstallData();

  base::RunLoop loop2;
  EXPECT_CALL(*delegate(), OnAppInstalling()).Times(1);
  EXPECT_CALL(*delegate(), OnAppPrepared())
      .Times(1)
      .WillOnce(RunClosure(loop2.QuitClosure()));
  launcher()->ContinueWithNetworkReady();
  url_loader_->ProcessLoadUrlRequests();
  loop2.Run();

  EXPECT_EQ(app_data()->status(), WebKioskAppData::Status::kInstalled);

  base::RunLoop loop3;
  EXPECT_CALL(*delegate(), OnAppLaunched())
      .WillOnce(RunClosure(loop3.QuitClosure()));
  launcher()->LaunchApp();
  loop3.Run();

  CloseAppWindow();
}

TEST_F(WebKioskAppLauncherTest, UrlNotLoaded) {
  base::HistogramTester histogram;

  SetupAppData(/*installed*/ false);

  base::RunLoop loop1;
  EXPECT_CALL(*delegate(), ShouldSkipAppInstallation()).WillOnce(Return(false));
  EXPECT_CALL(*delegate(), InitializeNetwork())
      .WillOnce(RunClosure(loop1.QuitClosure()));
  launcher()->Initialize();
  loop1.Run();

  SetupNotLoadedAppData();

  base::RunLoop loop2;
  EXPECT_CALL(*delegate(), OnAppInstalling());
  EXPECT_CALL(*delegate(),
              OnLaunchFailed(KioskAppLaunchError::Error::kUnableToInstall))
      .WillOnce(RunClosure(loop2.QuitClosure()));
  launcher()->ContinueWithNetworkReady();
  loop2.Run();

  EXPECT_NE(app_data()->status(), WebKioskAppData::Status::kInstalled);

  content::FetchHistogramsFromChildProcesses();
  histogram.ExpectUniqueSample(
      "Kiosk.WebApp.InstallError",
      webapps::InstallResultCode::kInstallURLLoadTimeOut, 1);
}

TEST_F(WebKioskAppLauncherTest, SkipInstallation) {
  SetupAppData(/*installed*/ false);

  base::RunLoop loop1;
  EXPECT_CALL(*delegate(), ShouldSkipAppInstallation()).WillOnce(Return(true));
  EXPECT_CALL(*delegate(), OnAppPrepared())
      .WillOnce(RunClosure(loop1.QuitClosure()));
  launcher()->Initialize();
  loop1.Run();

  EXPECT_EQ(app_data()->status(), WebKioskAppData::Status::kInit);
  EXPECT_EQ(app_data()->launch_url(), GURL());

  base::RunLoop loop2;
  EXPECT_CALL(*delegate(), OnAppLaunched())
      .WillOnce(RunClosure(loop2.QuitClosure()));
  launcher()->LaunchApp();
  loop2.Run();

  CloseAppWindow();
}

class WebKioskAppLauncherUsingLacrosTest : public WebKioskAppLauncherTest {
 public:
  WebKioskAppLauncherUsingLacrosTest()
      : browser_manager_(std::make_unique<crosapi::FakeBrowserManager>()),
        fake_user_manager_(new ash::FakeChromeUserManager()),
        scoped_user_manager_(base::WrapUnique(fake_user_manager_)),
        wm_helper_(std::make_unique<exo::WMHelperChromeOS>()) {
    scoped_feature_list_.InitAndEnableFeature(features::kWebKioskEnableLacros);
    crosapi::browser_util::SetLacrosEnabledForTest(true);
    crosapi::browser_util::SetLacrosPrimaryBrowserForTest(true);
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

  ash::FakeChromeUserManager* fake_user_manager() const {
    return fake_user_manager_;
  }

  exo::WMHelper* wm_helper() const { return wm_helper_.get(); }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<crosapi::FakeBrowserManager> browser_manager_;
  ash::FakeChromeUserManager* fake_user_manager_;
  user_manager::ScopedUserManager scoped_user_manager_;
  std::unique_ptr<exo::WMHelper> wm_helper_;
};

TEST_F(WebKioskAppLauncherUsingLacrosTest, NormalFlow) {
  LoginWebKioskUser();
  SetupAppData(/*installed*/ true);
  browser_manager()->set_new_fullscreen_window_creation_result(
      crosapi::mojom::CreationResult::kSuccess);

  base::RunLoop loop1;
  EXPECT_CALL(*delegate(), OnAppPrepared())
      .WillOnce(RunClosure(loop1.QuitClosure()));
  launcher()->Initialize();
  loop1.Run();

  // The browser manager is running before launching app. The
  // `OnAppWindowCreated` method will be called after the lacros-chrome window
  // is created successfully.
  base::RunLoop loop2;
  EXPECT_CALL(*delegate(), OnAppLaunched()).Times(1);
  EXPECT_CALL(*delegate(), OnAppWindowCreated())
      .Times(1)
      .WillOnce(RunClosure(loop2.QuitClosure()));
  EXPECT_CALL(*delegate(), OnLaunchFailed(_)).Times(0);
  browser_manager()->set_is_running(true);
  launcher()->LaunchApp();
  CreateLacrosWindowAndNotify();
  loop2.Run();
}

TEST_F(WebKioskAppLauncherUsingLacrosTest, WaitBrowserManagerToRun) {
  LoginWebKioskUser();
  SetupAppData(/*installed*/ true);
  browser_manager()->set_new_fullscreen_window_creation_result(
      crosapi::mojom::CreationResult::kSuccess);

  base::RunLoop loop1;
  EXPECT_CALL(*delegate(), OnAppPrepared())
      .WillOnce(RunClosure(loop1.QuitClosure()));
  launcher()->Initialize();
  loop1.Run();

  // The browser manager is not running before launching app. The crosapi call
  // will pend until it is ready. The `OnAppWindowCreated` method will be called
  // after the lacros-chrome window is created successfully.
  base::RunLoop loop2;
  EXPECT_CALL(*delegate(), OnAppLaunched()).Times(1);
  EXPECT_CALL(*delegate(), OnAppWindowCreated())
      .Times(1)
      .WillOnce(RunClosure(loop2.QuitClosure()));
  EXPECT_CALL(*delegate(), OnLaunchFailed(_)).Times(0);
  browser_manager()->set_is_running(false);
  launcher()->LaunchApp();
  browser_manager()->set_is_running(true);
  browser_manager()->StartRunning();
  CreateLacrosWindowAndNotify();
  loop2.Run();
}

TEST_F(WebKioskAppLauncherUsingLacrosTest, FailToLaunchApp) {
  LoginWebKioskUser();
  SetupAppData(/*installed*/ true);
  browser_manager()->set_new_fullscreen_window_creation_result(
      crosapi::mojom::CreationResult::kBrowserNotRunning);

  base::RunLoop loop1;
  EXPECT_CALL(*delegate(), OnAppPrepared())
      .WillOnce(RunClosure(loop1.QuitClosure()));
  launcher()->Initialize();
  loop1.Run();

  // If the lacros-chrome window fails to be created, the `OnLaunchFailed`
  // method will be called instead.
  base::RunLoop loop2;
  EXPECT_CALL(*delegate(), OnAppLaunched()).Times(1);
  EXPECT_CALL(*delegate(), OnAppWindowCreated()).Times(0);
  EXPECT_CALL(*delegate(), OnLaunchFailed(_))
      .Times(1)
      .WillOnce(RunClosure(loop2.QuitClosure()));

  browser_manager()->set_is_running(true);
  launcher()->LaunchApp();
  loop2.Run();
}

}  // namespace ash
