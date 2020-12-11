// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/strcat.h"
#include "chrome/browser/chromeos/app_mode/kiosk_app_manager.h"
#include "chrome/browser/chromeos/app_mode/kiosk_app_types.h"
#include "chrome/browser/chromeos/app_mode/web_app/mock_web_kiosk_app_launcher.h"
#include "chrome/browser/chromeos/login/app_mode/kiosk_launch_controller.h"
#include "chrome/browser/chromeos/login/test/kiosk_test_helpers.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/external_provider_impl.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/webui/chromeos/login/fake_app_launch_splash_screen_handler.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/testing_profile.h"
#include "components/account_id/account_id.h"
#include "components/policy/core/common/mock_configuration_policy_provider.h"
#include "components/session_manager/core/session_manager.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/browser_test.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/pref_names.h"
#include "extensions/browser/test_extension_registry_observer.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_builder.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;

namespace chromeos {

const char kExtensionId1[] = "extensionId1";
const char kExtensionId2[] = "extensionId2";
const char kExtensionName1[] = "extensionName1";
const char kExtensionName2[] = "extensionName2";

// URL of Chrome Web.
const char kExtensionUpdateUrl[] =
    "https://clients2.google.com/service/update2/crx";

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
  MockWebKioskAppLauncher* app_launcher_;  // owned by `controller_`.
  std::unique_ptr<KioskLaunchController> controller_;
  KioskAppId kiosk_app_id_;
};

IN_PROC_BROWSER_TEST_P(KioskLaunchControllerTest, RegularFlow) {
  controller()->Start(kiosk_app_id(), false);
  ExpectState(AppState::kCreatingProfile, NetworkUIState::kNotShowing);

  EXPECT_CALL(*launcher(), Initialize()).Times(1);
  profile_controls()->OnProfileLoaded(profile());

  launch_controls()->InitializeNetwork();
  ExpectState(AppState::kInitNetwork, NetworkUIState::kNotShowing);
  EXPECT_CALL(*launcher(), ContinueWithNetworkReady()).Times(1);
  SetOnline(true);

  launch_controls()->OnAppInstalling();

  launch_controls()->OnAppPrepared();
  ExpectState(AppState::kInstalled, NetworkUIState::kNotShowing);

  EXPECT_CALL(*launcher(), LaunchApp()).Times(1);
  FireSplashScreenTimer();

  launch_controls()->OnAppLaunched();
  ExpectState(AppState::kLaunched, NetworkUIState::kNotShowing);
  EXPECT_TRUE(session_manager::SessionManager::Get()->IsSessionStarted());
}

IN_PROC_BROWSER_TEST_P(KioskLaunchControllerTest, AlreadyInstalled) {
  controller()->Start(kiosk_app_id(), false);
  ExpectState(AppState::kCreatingProfile, NetworkUIState::kNotShowing);

  EXPECT_CALL(*launcher(), Initialize()).Times(1);
  profile_controls()->OnProfileLoaded(profile());

  launch_controls()->OnAppPrepared();
  ExpectState(AppState::kInstalled, NetworkUIState::kNotShowing);

  EXPECT_CALL(*launcher(), LaunchApp()).Times(1);
  FireSplashScreenTimer();

  launch_controls()->OnAppLaunched();
  ExpectState(AppState::kLaunched, NetworkUIState::kNotShowing);
  EXPECT_TRUE(session_manager::SessionManager::Get()->IsSessionStarted());
}

IN_PROC_BROWSER_TEST_P(KioskLaunchControllerTest,
                       ConfigureNetworkBeforeProfile) {
  controller()->Start(kiosk_app_id(), false);
  ExpectState(AppState::kCreatingProfile, NetworkUIState::kNotShowing);

  // User presses the hotkey.
  view_controls()->OnNetworkConfigRequested();
  ExpectState(AppState::kCreatingProfile, NetworkUIState::kNeedToShow);

  EXPECT_CALL(*launcher(), Initialize()).Times(1);
  profile_controls()->OnProfileLoaded(profile());
  // WebKioskAppLauncher::Initialize call is synchronous, we have to call the
  // response now.
  launch_controls()->InitializeNetwork();

  ExpectState(AppState::kInitNetwork, NetworkUIState::kShowing);
  EXPECT_CALL(*launcher(), RestartLauncher()).Times(1);
  view_controls()->OnNetworkConfigFinished();

  EXPECT_CALL(*launcher(), LaunchApp()).Times(1);
  launch_controls()->OnAppPrepared();

  // Skipping INSTALLED state since there splash screen timer is stopped when
  // network configure ui was shown.

  launch_controls()->OnAppLaunched();
  ExpectState(AppState::kLaunched, NetworkUIState::kNotShowing);
  EXPECT_TRUE(session_manager::SessionManager::Get()->IsSessionStarted());
}

IN_PROC_BROWSER_TEST_P(KioskLaunchControllerTest,
                       ConfigureNetworkDuringInstallation) {
  SetOnline(false);
  controller()->Start(kiosk_app_id(), false);
  ExpectState(AppState::kCreatingProfile, NetworkUIState::kNotShowing);

  EXPECT_CALL(*launcher(), Initialize()).Times(1);
  profile_controls()->OnProfileLoaded(profile());

  launch_controls()->InitializeNetwork();
  ExpectState(AppState::kInitNetwork, NetworkUIState::kNotShowing);
  EXPECT_CALL(*launcher(), ContinueWithNetworkReady()).Times(1);
  SetOnline(true);

  launch_controls()->OnAppInstalling();

  // User presses the hotkey, current installation is canceled.
  EXPECT_CALL(*launcher(), RestartLauncher()).Times(1);
  view_controls()->OnNetworkConfigRequested();
  // Launcher restart causes network to be requested again.
  launch_controls()->InitializeNetwork();
  ExpectState(AppState::kInitNetwork, NetworkUIState::kShowing);

  EXPECT_CALL(*launcher(), RestartLauncher()).Times(1);
  view_controls()->OnNetworkConfigFinished();

  launch_controls()->OnAppInstalling();
  ExpectState(AppState::kInstallingApp, NetworkUIState::kNotShowing);

  launch_controls()->OnAppPrepared();
  ExpectState(AppState::kInstalled, NetworkUIState::kNotShowing);

  EXPECT_CALL(*launcher(), LaunchApp()).Times(1);
  FireSplashScreenTimer();

  launch_controls()->OnAppLaunched();
  ExpectState(AppState::kLaunched, NetworkUIState::kNotShowing);
  EXPECT_TRUE(session_manager::SessionManager::Get()->IsSessionStarted());
}

IN_PROC_BROWSER_TEST_P(KioskLaunchControllerTest,
                       ConnectionLostDuringInstallation) {
  controller()->Start(kiosk_app_id(), false);
  ExpectState(AppState::kCreatingProfile, NetworkUIState::kNotShowing);

  EXPECT_CALL(*launcher(), Initialize()).Times(1);
  profile_controls()->OnProfileLoaded(profile());

  launch_controls()->InitializeNetwork();
  ExpectState(AppState::kInitNetwork, NetworkUIState::kNotShowing);
  EXPECT_CALL(*launcher(), ContinueWithNetworkReady()).Times(1);
  SetOnline(true);

  launch_controls()->OnAppInstalling();
  ExpectState(AppState::kInstallingApp, NetworkUIState::kNotShowing);

  SetOnline(false);
  launch_controls()->InitializeNetwork();
  ExpectState(AppState::kInitNetwork, NetworkUIState::kShowing);

  EXPECT_CALL(*launcher(), RestartLauncher()).Times(1);
  view_controls()->OnNetworkConfigFinished();

  launch_controls()->OnAppInstalling();
  ExpectState(AppState::kInstallingApp, NetworkUIState::kNotShowing);

  launch_controls()->OnAppPrepared();
  ExpectState(AppState::kInstalled, NetworkUIState::kNotShowing);

  EXPECT_CALL(*launcher(), LaunchApp()).Times(1);
  FireSplashScreenTimer();

  launch_controls()->OnAppLaunched();
  ExpectState(AppState::kLaunched, NetworkUIState::kNotShowing);
  EXPECT_TRUE(session_manager::SessionManager::Get()->IsSessionStarted());
}

INSTANTIATE_TEST_SUITE_P(All,
                         KioskLaunchControllerTest,
                         testing::Values(KioskAppType::ARC_APP,
                                         KioskAppType::CHROME_APP,
                                         KioskAppType::WEB_APP));

class KioskLaunchControllerWithExtensionTest
    : public KioskLaunchControllerTest {
 public:
  void SetupForceList() {
    std::unique_ptr<base::Value> dict =
        extensions::DictionaryBuilder()
            .Set(kExtensionId1,
                 extensions::DictionaryBuilder()
                     .Set(extensions::ExternalProviderImpl::kExternalUpdateUrl,
                          kExtensionUpdateUrl)
                     .Build())
            .Set(kExtensionId2,
                 extensions::DictionaryBuilder()
                     .Set(extensions::ExternalProviderImpl::kExternalUpdateUrl,
                          kExtensionUpdateUrl)
                     .Build())
            .Build();
    ProfileManager::GetPrimaryUserProfile()->GetPrefs()->Set(
        extensions::pref_names::kInstallForceList, std::move(*dict));

    base::Value list(base::Value::Type::LIST);
    list.Append(base::StrCat({kExtensionId1, ";", kExtensionUpdateUrl}));
    list.Append(base::StrCat({kExtensionId2, ";", kExtensionUpdateUrl}));

    policy::PolicyMap map;
    map.Set(extensions::pref_names::kInstallForceList,
            policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
            policy::POLICY_SOURCE_CLOUD, std::move(list), nullptr);

    extensions::ExtensionRegistry* registry =
        extensions::ExtensionRegistry::Get(
            ProfileManager::GetPrimaryUserProfile());
    extensions::TestExtensionRegistryObserver install_observer(registry);
    policy_provider_.UpdateChromePolicy(map);
    base::RunLoop().RunUntilIdle();
  }

  void PreRunTestOnMainThread() override {
    SetupForceList();
    InProcessBrowserTest::PreRunTestOnMainThread();
  }

  extensions::ForceInstalledTracker* force_installed_tracker() {
    return extensions::ExtensionSystem::Get(profile())
        ->extension_service()
        ->force_installed_tracker();
  }

  void RunUntilAppPrepared() {
    controller()->Start(kiosk_app_id(), false);
    ExpectState(AppState::kCreatingProfile, NetworkUIState::kNotShowing);

    EXPECT_CALL(*launcher(), Initialize()).Times(1);
    profile_controls()->OnProfileLoaded(profile());

    launch_controls()->InitializeNetwork();
    ExpectState(AppState::kInitNetwork, NetworkUIState::kNotShowing);
    EXPECT_CALL(*launcher(), ContinueWithNetworkReady()).Times(1);
    SetOnline(true);

    launch_controls()->OnAppInstalling();

    launch_controls()->OnAppPrepared();
  }

  void SetExtensionLoaded() {
    auto ext1 = extensions::ExtensionBuilder(kExtensionName1)
                    .SetID(kExtensionId1)
                    .Build();
    auto ext2 = extensions::ExtensionBuilder(kExtensionName2)
                    .SetID(kExtensionId2)
                    .Build();
    force_installed_tracker()->OnExtensionReady(profile(), ext1.get());
    force_installed_tracker()->OnExtensionReady(profile(), ext2.get());
  }

  policy::MockConfigurationPolicyProvider policy_provider_;
};

IN_PROC_BROWSER_TEST_P(KioskLaunchControllerWithExtensionTest,
                       ExtensionLoadedBeforeAppPrepared) {
  SetExtensionLoaded();
  RunUntilAppPrepared();
  ExpectState(AppState::kInstalled, NetworkUIState::kNotShowing);

  EXPECT_CALL(*launcher(), LaunchApp()).Times(1);
  FireSplashScreenTimer();

  launch_controls()->OnAppLaunched();
  ExpectState(AppState::kLaunched, NetworkUIState::kNotShowing);
  EXPECT_TRUE(session_manager::SessionManager::Get()->IsSessionStarted());
}

IN_PROC_BROWSER_TEST_P(KioskLaunchControllerWithExtensionTest,
                       ExtensionLoadedAfterAppPrepared) {
  RunUntilAppPrepared();
  ExpectState(AppState::kInstallingExtensions, NetworkUIState::kNotShowing);

  SetExtensionLoaded();
  ExpectState(AppState::kInstalled, NetworkUIState::kNotShowing);

  EXPECT_CALL(*launcher(), LaunchApp()).Times(1);
  FireSplashScreenTimer();

  launch_controls()->OnAppLaunched();
  ExpectState(AppState::kLaunched, NetworkUIState::kNotShowing);
  EXPECT_TRUE(session_manager::SessionManager::Get()->IsSessionStarted());
}

INSTANTIATE_TEST_SUITE_P(All,
                         KioskLaunchControllerWithExtensionTest,
                         testing::Values(KioskAppType::ARC_APP,
                                         KioskAppType::CHROME_APP,
                                         KioskAppType::WEB_APP));
}  // namespace chromeos
