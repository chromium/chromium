// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_mode/web_app/web_kiosk_app_launcher.h"

#include <memory>

#include "base/strings/utf_string_conversions.h"
#include "base/test/gmock_callback_support.h"
#include "chrome/browser/ash/app_mode/kiosk_app_manager_observer.h"
#include "chrome/browser/ash/app_mode/web_app/web_kiosk_app_data.h"
#include "chrome/browser/ash/app_mode/web_app/web_kiosk_app_manager.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/web_applications/components/web_application_info.h"
#include "chrome/browser/web_applications/test/test_data_retriever.h"
#include "chrome/browser/web_applications/test/test_web_app_url_loader.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/scoped_testing_local_state.h"
#include "chrome/test/base/test_browser_window.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using ::base::test::RunClosure;
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
  MOCK_METHOD1(OnLaunchFailed, void(KioskAppLaunchError::Error));

  MOCK_CONST_METHOD0(IsNetworkReady, bool());
  MOCK_CONST_METHOD0(IsShowingNetworkConfigScreen, bool());
  MOCK_CONST_METHOD0(ShouldSkipAppInstallation, bool());
};

const char kAppEmail[] = "lala@example.com";
const char kAppInstallUrl[] = "https://example.com";
const char kAppLaunchUrl[] = "https://example.com/launch";
const char kAppLaunchBadUrl[] = "https://badexample.com";
const char kAppTitle[] = "app";

std::unique_ptr<web_app::WebAppDataRetriever> CreateDataRetrieverWithData(
    const GURL& url) {
  auto data_retriever = std::make_unique<web_app::TestDataRetriever>();
  auto info = std::make_unique<WebApplicationInfo>();
  info->start_url = url;
  info->title = base::UTF8ToUTF16(kAppTitle);
  data_retriever->SetRendererWebApplicationInfo(std::move(info));
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

class WebKioskAppLauncherTest : public ChromeRenderViewHostTestHarness {
 public:
  WebKioskAppLauncherTest()
      : ChromeRenderViewHostTestHarness(),
        local_state_(TestingBrowserProcess::GetGlobal()) {}
  ~WebKioskAppLauncherTest() override {}

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    app_manager_ = std::make_unique<WebKioskAppManager>();
    delegate_ = std::make_unique<MockAppLauncherDelegate>();
    launcher_ = std::make_unique<WebKioskAppLauncher>(
        profile(), delegate_.get(), AccountId::FromUserEmail(kAppEmail));

    browser_window_ = new TestBrowserWindow();
    new TestBrowserWindowOwner(browser_window_);
    browser_window_->SetNativeWindow(new aura::Window(nullptr));

    launcher_->SetBrowserWindowForTesting(browser_window_);
    url_loader_ = new web_app::TestWebAppUrlLoader();
    launcher_->SetUrlLoaderForTesting(
        std::unique_ptr<web_app::TestWebAppUrlLoader>(url_loader_));

    closer_.reset(new AppWindowCloser());
  }

  void TearDown() override {
    closer_.reset();
    launcher_.reset();
    delegate_.reset();
    app_manager_.reset();
    ChromeRenderViewHostTestHarness::TearDown();
  }

  void SetupAppData(bool installed) {
    account_id_ = AccountId::FromUserEmail(kAppEmail);
    app_manager_->AddAppForTesting(account_id_, GURL(kAppInstallUrl));

    if (installed) {
      auto info = std::make_unique<WebApplicationInfo>();
      info->start_url = GURL(kAppLaunchUrl);
      info->title = base::UTF8ToUTF16(kAppTitle);
      app_manager_->UpdateAppByAccountId(account_id_, std::move(info));
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
  ScopedTestingLocalState local_state_;

  TestBrowserWindow* browser_window_;
  std::unique_ptr<MockAppLauncherDelegate> delegate_;
  std::unique_ptr<WebKioskAppLauncher> launcher_;
  std::unique_ptr<AppWindowCloser> closer_;
};

// TODO(crbug.com/1097708): these tests flakily fail on MSAN Builds.
#if !defined(MEMORY_SANITIZER)
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
#endif

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

// TODO(crbug.com/1097708): these tests flakily fail on MSAN Builds.
#if !defined(MEMORY_SANITIZER)
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
#endif

TEST_F(WebKioskAppLauncherTest, UrlNotLoaded) {
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

}  // namespace ash
