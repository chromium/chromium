// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/app_mode/kiosk_app_manager.h"
#include "chrome/browser/chromeos/app_mode/kiosk_app_types.h"
#include "chrome/browser/chromeos/app_mode/web_app/mock_web_kiosk_app_launcher.h"
#include "chrome/browser/chromeos/login/app_mode/kiosk_launch_controller.h"
#include "chrome/browser/chromeos/login/test/kiosk_test_helpers.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/webui/chromeos/login/fake_app_launch_splash_screen_handler.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/testing_profile.h"
#include "components/account_id/account_id.h"
#include "components/session_manager/core/session_manager.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;

namespace chromeos {

class KioskLaunchControllerTest
    : public InProcessBrowserTest,
      public ::testing::WithParamInterface<KioskAppType> {
 public:
  using AppState = KioskLaunchController::AppState;
  using NetworkUIState = KioskLaunchController::NetworkUIState;

  KioskLaunchControllerTest() = default;
  KioskLaunchControllerTest(const KioskLaunchControllerTest&) = delete;
  KioskLaunchControllerTest& operator=(const KioskLaunchControllerTest&) =
      delete;

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();

    auto app_launcher = std::make_unique<MockWebKioskAppLauncher>();
    view_ = std::make_unique<FakeAppLaunchSplashScreenHandler>();
    app_launcher_ = app_launcher.get();
    disable_wait_timer_and_login_operations_for_testing_ =
        KioskLaunchController::DisableWaitTimerAndLoginOperationsForTesting();
    controller_ = KioskLaunchController::CreateForTesting(
        view_.get(), std::move(app_launcher));

    switch (GetParam()) {
      case KioskAppType::ARC_APP:
        kiosk_app_id_ = KioskAppId::ForArcApp(EmptyAccountId());
        break;
      case KioskAppType::CHROME_APP:
        kiosk_app_id_ = KioskAppId::ForChromeApp(std::string());
        KioskAppManager::Get()->AddAppForTest(std::string(), EmptyAccountId(),
                                              GURL(), std::string());
        break;
      case KioskAppType::WEB_APP:
        kiosk_app_id_ = KioskAppId::ForWebApp(EmptyAccountId());
        break;
    }
  }

  KioskLaunchController* controller() { return controller_.get(); }

  KioskAppLauncher::Delegate* launch_controls() {
    return static_cast<KioskAppLauncher::Delegate*>(controller_.get());
  }

  KioskProfileLoader::Delegate* profile_controls() {
    return static_cast<KioskProfileLoader::Delegate*>(controller_.get());
  }

  AppLaunchSplashScreenView::Delegate* view_controls() {
    return static_cast<AppLaunchSplashScreenView::Delegate*>(controller_.get());
  }

  MockWebKioskAppLauncher* launcher() { return app_launcher_; }

  void ExpectState(AppState app_state, NetworkUIState network_state) {
    EXPECT_EQ(app_state, controller_->app_state_);
    EXPECT_EQ(network_state, controller_->network_ui_state_);
  }

  void FireSplashScreenTimer() { controller_->OnTimerFire(); }

  void SetOnline(bool online) {
    view_->SetNetworkReady(online);
    static_cast<AppLaunchSplashScreenView::Delegate*>(controller_.get())
        ->OnNetworkStateChanged(online);
  }

  Profile* profile() { return browser()->profile(); }

  FakeAppLaunchSplashScreenHandler* view() { return view_.get(); }

  KioskAppId kiosk_app_id() { return kiosk_app_id_; }

 private:
  ScopedCanConfigureNetwork can_configure_network_for_testing_{true, false};
  std::unique_ptr<base::AutoReset<bool>>
      disable_wait_timer_and_login_operations_for_testing_;
  std::unique_ptr<FakeAppLaunchSplashScreenHandler> view_;
  MockWebKioskAppLauncher* app_launcher_;  // owned by |controller_|.
  std::unique_ptr<KioskLaunchController> controller_;
  KioskAppId kiosk_app_id_;
};

IN_PROC_BROWSER_TEST_P(KioskLaunchControllerTest, RegularFlow) {
  controller()->Start(kiosk_app_id(), false);
  ExpectState(AppState::CREATING_PROFILE, NetworkUIState::NOT_SHOWING);

  EXPECT_CALL(*launcher(), Initialize()).Times(1);
  profile_controls()->OnProfileLoaded(profile());

  launch_controls()->InitializeNetwork();
  ExpectState(AppState::INIT_NETWORK, NetworkUIState::NOT_SHOWING);
  EXPECT_CALL(*launcher(), ContinueWithNetworkReady()).Times(1);
  SetOnline(true);

  launch_controls()->OnAppInstalling();

  launch_controls()->OnAppPrepared();
  ExpectState(AppState::INSTALLED, NetworkUIState::NOT_SHOWING);

  EXPECT_CALL(*launcher(), LaunchApp()).Times(1);
  FireSplashScreenTimer();

  launch_controls()->OnAppLaunched();
  ExpectState(AppState::LAUNCHED, NetworkUIState::NOT_SHOWING);
  EXPECT_TRUE(session_manager::SessionManager::Get()->IsSessionStarted());
}

IN_PROC_BROWSER_TEST_P(KioskLaunchControllerTest, AlreadyInstalled) {
  controller()->Start(kiosk_app_id(), false);
  ExpectState(AppState::CREATING_PROFILE, NetworkUIState::NOT_SHOWING);

  EXPECT_CALL(*launcher(), Initialize()).Times(1);
  profile_controls()->OnProfileLoaded(profile());

  launch_controls()->OnAppPrepared();
  ExpectState(AppState::INSTALLED, NetworkUIState::NOT_SHOWING);

  EXPECT_CALL(*launcher(), LaunchApp()).Times(1);
  FireSplashScreenTimer();

  launch_controls()->OnAppLaunched();
  ExpectState(AppState::LAUNCHED, NetworkUIState::NOT_SHOWING);
  EXPECT_TRUE(session_manager::SessionManager::Get()->IsSessionStarted());
}

IN_PROC_BROWSER_TEST_P(KioskLaunchControllerTest,
                       ConfigureNetworkBeforeProfile) {
  controller()->Start(kiosk_app_id(), false);
  ExpectState(AppState::CREATING_PROFILE, NetworkUIState::NOT_SHOWING);

  // User presses the hotkey.
  view_controls()->OnNetworkConfigRequested();
  ExpectState(AppState::CREATING_PROFILE, NetworkUIState::NEED_TO_SHOW);

  EXPECT_CALL(*launcher(), Initialize()).Times(1);
  profile_controls()->OnProfileLoaded(profile());
  // WebKioskAppLauncher::Initialize call is synchronous, we have to call the
  // response now.
  launch_controls()->InitializeNetwork();

  ExpectState(AppState::INIT_NETWORK, NetworkUIState::SHOWING);
  EXPECT_CALL(*launcher(), RestartLauncher()).Times(1);
  view_controls()->OnNetworkConfigFinished();

  EXPECT_CALL(*launcher(), LaunchApp()).Times(1);
  launch_controls()->OnAppPrepared();

  // Skipping INSTALLED state since there splash screen timer is stopped when
  // network configure ui was shown.

  launch_controls()->OnAppLaunched();
  ExpectState(AppState::LAUNCHED, NetworkUIState::NOT_SHOWING);
  EXPECT_TRUE(session_manager::SessionManager::Get()->IsSessionStarted());
}

IN_PROC_BROWSER_TEST_P(KioskLaunchControllerTest,
                       ConfigureNetworkDuringInstallation) {
  SetOnline(false);
  controller()->Start(kiosk_app_id(), false);
  ExpectState(AppState::CREATING_PROFILE, NetworkUIState::NOT_SHOWING);

  EXPECT_CALL(*launcher(), Initialize()).Times(1);
  profile_controls()->OnProfileLoaded(profile());

  launch_controls()->InitializeNetwork();
  ExpectState(AppState::INIT_NETWORK, NetworkUIState::NOT_SHOWING);
  EXPECT_CALL(*launcher(), ContinueWithNetworkReady()).Times(1);
  SetOnline(true);

  launch_controls()->OnAppInstalling();

  // User presses the hotkey, current installation is canceled.
  EXPECT_CALL(*launcher(), RestartLauncher()).Times(1);
  view_controls()->OnNetworkConfigRequested();
  // Launcher restart causes network to be requested again.
  launch_controls()->InitializeNetwork();
  ExpectState(AppState::INIT_NETWORK, NetworkUIState::SHOWING);

  EXPECT_CALL(*launcher(), RestartLauncher()).Times(1);
  view_controls()->OnNetworkConfigFinished();

  launch_controls()->OnAppInstalling();
  ExpectState(AppState::INSTALLING, NetworkUIState::NOT_SHOWING);

  launch_controls()->OnAppPrepared();
  ExpectState(AppState::INSTALLED, NetworkUIState::NOT_SHOWING);

  EXPECT_CALL(*launcher(), LaunchApp()).Times(1);
  FireSplashScreenTimer();

  launch_controls()->OnAppLaunched();
  ExpectState(AppState::LAUNCHED, NetworkUIState::NOT_SHOWING);
  EXPECT_TRUE(session_manager::SessionManager::Get()->IsSessionStarted());
}

IN_PROC_BROWSER_TEST_P(KioskLaunchControllerTest,
                       ConnectionLostDuringInstallation) {
  controller()->Start(kiosk_app_id(), false);
  ExpectState(AppState::CREATING_PROFILE, NetworkUIState::NOT_SHOWING);

  EXPECT_CALL(*launcher(), Initialize()).Times(1);
  profile_controls()->OnProfileLoaded(profile());

  launch_controls()->InitializeNetwork();
  ExpectState(AppState::INIT_NETWORK, NetworkUIState::NOT_SHOWING);
  EXPECT_CALL(*launcher(), ContinueWithNetworkReady()).Times(1);
  SetOnline(true);

  launch_controls()->OnAppInstalling();
  ExpectState(AppState::INSTALLING, NetworkUIState::NOT_SHOWING);

  SetOnline(false);
  launch_controls()->InitializeNetwork();
  ExpectState(AppState::INIT_NETWORK, NetworkUIState::SHOWING);

  EXPECT_CALL(*launcher(), RestartLauncher()).Times(1);
  view_controls()->OnNetworkConfigFinished();

  launch_controls()->OnAppInstalling();
  ExpectState(AppState::INSTALLING, NetworkUIState::NOT_SHOWING);

  launch_controls()->OnAppPrepared();
  ExpectState(AppState::INSTALLED, NetworkUIState::NOT_SHOWING);

  EXPECT_CALL(*launcher(), LaunchApp()).Times(1);
  FireSplashScreenTimer();

  launch_controls()->OnAppLaunched();
  ExpectState(AppState::LAUNCHED, NetworkUIState::NOT_SHOWING);
  EXPECT_TRUE(session_manager::SessionManager::Get()->IsSessionStarted());
}

INSTANTIATE_TEST_SUITE_P(All,
                         KioskLaunchControllerTest,
                         testing::Values(KioskAppType::ARC_APP,
                                         KioskAppType::CHROME_APP,
                                         KioskAppType::WEB_APP));
}  // namespace chromeos
