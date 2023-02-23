// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/app_mode/kiosk_launch_controller.h"

#include <memory>

#include "ash/constants/ash_switches.h"
#include "base/functional/bind.h"
#include "base/memory/scoped_refptr.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_command_line.h"
#include "base/test/task_environment.h"
#include "chrome/browser/ash/app_mode/fake_kiosk_app_launcher.h"
#include "chrome/browser/ash/app_mode/kiosk_app_launch_error.h"
#include "chrome/browser/ash/app_mode/kiosk_app_launcher.h"
#include "chrome/browser/ash/app_mode/kiosk_app_types.h"
#include "chrome/browser/ash/app_mode/web_app/web_kiosk_app_manager.h"
#include "chrome/browser/ash/login/test/kiosk_test_helpers.h"
#include "chrome/browser/ash/policy/core/device_local_account.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_service_test_base.h"
#include "chrome/browser/extensions/forced_extensions/install_stage_tracker.h"
#include "chrome/browser/ui/ash/keyboard/chrome_keyboard_controller_client_test_helper.h"
#include "chrome/browser/ui/webui/ash/login/app_launch_splash_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/fake_app_launch_splash_screen_handler.h"
#include "chrome/test/base/testing_profile.h"
#include "components/account_id/account_id.h"
#include "components/crash/core/common/crash_key.h"
#include "components/policy/core/browser/browser_policy_connector_base.h"
#include "components/policy/core/common/policy_service.h"
#include "components/policy/policy_constants.h"
#include "components/session_manager/core/session_manager.h"
#include "content/public/test/browser_task_environment.h"
#include "extensions/browser/extension_system.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_builder.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace ash {

namespace {

using ::testing::Eq;

const char kInstallUrl[] = "https://install.url";
const char kExtensionId[] = "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";
const char kInvalidExtensionId[] = "invalid-extension-id";
const char kExtensionName[] = "extension_name";

// URL of Chrome Web Store.
const char kWebStoreExtensionUpdateUrl[] =
    "https://clients2.google.com/service/update2/crx";

// URL of off store extensions.
const char kOffStoreExtensionUpdateUrl[] = "https://example.com/crx";

auto BuildExtension(std::string extension_name, std::string extension_id) {
  return extensions::ExtensionBuilder(extension_name)
      .SetID(extension_id)
      .Build();
}

}  // namespace

class MockKioskProfileLoadFailedObserver
    : public KioskLaunchController::KioskProfileLoadFailedObserver {
 public:
  MockKioskProfileLoadFailedObserver() = default;

  MockKioskProfileLoadFailedObserver(
      const MockKioskProfileLoadFailedObserver&) = delete;
  MockKioskProfileLoadFailedObserver& operator=(
      const MockKioskProfileLoadFailedObserver&) = delete;

  ~MockKioskProfileLoadFailedObserver() override = default;

  MOCK_METHOD(void, OnKioskProfileLoadFailed, (), (override));
};

class KioskLaunchControllerTest : public extensions::ExtensionServiceTestBase {
 public:
  using AppState = KioskLaunchController::AppState;
  using NetworkUIState = KioskLaunchController::NetworkUIState;

  KioskLaunchControllerTest()
      : extensions::ExtensionServiceTestBase(
            std::make_unique<content::BrowserTaskEnvironment>(
                base::test::TaskEnvironment::TimeSource::MOCK_TIME)) {}

  KioskLaunchControllerTest(const KioskLaunchControllerTest&) = delete;
  KioskLaunchControllerTest& operator=(const KioskLaunchControllerTest&) =
      delete;

  void SetUp() override {
    InitializeEmptyExtensionService();
    policy::BrowserPolicyConnectorBase::SetPolicyServiceForTesting(
        policy_service());

    keyboard_controller_client_ =
        ChromeKeyboardControllerClientTestHelper::InitializeWithFake();

    disable_wait_timer_and_login_operations_for_testing_ =
        KioskLaunchController::DisableLoginOperationsForTesting();

    view_ = std::make_unique<FakeAppLaunchSplashScreenHandler>();
    controller_ = std::make_unique<KioskLaunchController>(
        /*host=*/nullptr, view_.get(),
        base::BindRepeating(
            &KioskLaunchControllerTest::BuildFakeKioskAppLauncher,
            base::Unretained(this)));

    // We can't call `crash_reporter::ResetCrashKeysForTesting()` to reset crash
    // keys since it destroys the storage for static crash keys. Instead we set
    // the initial state to `KioskLaunchState::kStartLaunch` before testing.
    SetKioskLaunchStateCrashKey(KioskLaunchState::kStartLaunch);

    SetUpKioskAppInAppManager();

    extensions::ExtensionServiceTestBase::SetUp();
  }

  void TearDown() override {
    extensions::ExtensionServiceTestBase::TearDown();

    kiosk_app_manager_.reset();

    policy::BrowserPolicyConnectorBase::SetPolicyServiceForTesting(nullptr);
  }

  KioskLaunchController& controller() { return *controller_; }

  KioskAppLauncher::NetworkDelegate& network_delegate() { return *controller_; }

  KioskProfileLoader::Delegate& profile_controls() { return *controller_; }

  AppLaunchSplashScreenView::Delegate& view_controls() { return *controller_; }

  FakeKioskAppLauncher& launcher() { return *app_launcher_; }

  int num_launchers_created() { return app_launchers_created_; }

  auto HasState(AppState app_state, NetworkUIState network_state) {
    return testing::AllOf(
        testing::Field("app_state", &KioskLaunchController::app_state_,
                       Eq(app_state)),
        testing::Field("network_ui_state",
                       &KioskLaunchController::network_ui_state_,
                       Eq(network_state)));
  }

  auto HasViewState(AppLaunchSplashScreenView::AppLaunchState launch_state) {
    return testing::Property(
        "GetAppLaunchState",
        &FakeAppLaunchSplashScreenHandler::GetAppLaunchState, Eq(launch_state));
  }

  auto HasErrorMessage(KioskAppLaunchError::Error error) {
    return testing::Property(
        "ErrorState", &FakeAppLaunchSplashScreenHandler::GetErrorMessageType,
        Eq(error));
  }

  void FireSplashScreenTimer() {
    task_environment()->FastForwardBy(kDefaultKioskSplashScreenMinTime);
  }

  void DeleteSplashScreen() { controller_->OnDeletingSplashScreenView(); }

  void SetOnline(bool online) {
    view_->SetNetworkReady(online);
    view_controls().OnNetworkStateChanged(online);
  }

  void OnNetworkConfigRequested() { controller().OnNetworkConfigRequested(); }

  FakeAppLaunchSplashScreenHandler& view() { return *view_; }

  KioskAppId kiosk_app_id() { return kiosk_app_id_; }

  void RunUntilAppPrepared() {
    controller().Start(kiosk_app_id(), /*auto_launch=*/false);
    profile_controls().OnProfileLoaded(profile());
    launcher().observers().NotifyAppInstalling();
    launcher().observers().NotifyAppPrepared();
  }

  void VerifyLaunchStateCrashKey(KioskLaunchState state) {
    EXPECT_EQ(crash_reporter::GetCrashKeyValue(kKioskLaunchStateCrashKey),
              KioskLaunchStateToString(state));
  }

 private:
  void SetUpKioskAppInAppManager() {
    std::string email = policy::GenerateDeviceLocalAccountUserId(
        kInstallUrl, policy::DeviceLocalAccount::Type::TYPE_WEB_KIOSK_APP);
    AccountId account_id(AccountId::FromUserEmail(email));
    kiosk_app_id_ = KioskAppId::ForWebApp(account_id);

    kiosk_app_manager_ = std::make_unique<WebKioskAppManager>();
    kiosk_app_manager_->AddAppForTesting(kiosk_app_id_.account_id.value(),
                                         GURL(kInstallUrl));
  }

  std::unique_ptr<KioskAppLauncher> BuildFakeKioskAppLauncher(
      Profile*,
      const KioskAppId& kiosk_app_id,
      KioskAppLauncher::NetworkDelegate*) {
    app_launchers_created_++;
    auto app_launcher = std::make_unique<FakeKioskAppLauncher>();
    app_launcher_ = app_launcher.get();
    return std::move(app_launcher);
  }

  TestingProfile profile_;
  session_manager::SessionManager session_manager_;
  std::unique_ptr<ChromeKeyboardControllerClientTestHelper>
      keyboard_controller_client_;
  std::unique_ptr<WebKioskAppManager> kiosk_app_manager_;

  ScopedCanConfigureNetwork can_configure_network_for_testing_{true, false};
  std::unique_ptr<base::AutoReset<bool>>
      disable_wait_timer_and_login_operations_for_testing_;
  std::unique_ptr<FakeAppLaunchSplashScreenHandler> view_;
  FakeKioskAppLauncher* app_launcher_ = nullptr;  // owned by `controller_`.
  int app_launchers_created_ = 0;
  std::unique_ptr<KioskLaunchController> controller_;
  KioskAppId kiosk_app_id_;
};

TEST_F(KioskLaunchControllerTest, StartShouldShowAppDataOnSplashScreen) {
  controller().Start(kiosk_app_id(), /*auto_launch=*/false);

  EXPECT_EQ(view().last_app_data().url, GURL(kInstallUrl));
}

TEST_F(KioskLaunchControllerTest, ProfileLoadedShouldInitializeLauncher) {
  controller().Start(kiosk_app_id(), /*auto_launch=*/false);
  VerifyLaunchStateCrashKey(KioskLaunchState::kLauncherStarted);
  EXPECT_THAT(controller(), HasState(AppState::kCreatingProfile,
                                     NetworkUIState::kNotShowing));

  profile_controls().OnProfileLoaded(profile());
  EXPECT_TRUE(launcher().IsInitialized());
}

TEST_F(KioskLaunchControllerTest, AppInstallingShouldUpdateSplashScreen) {
  controller().Start(kiosk_app_id(), /*auto_launch=*/false);
  VerifyLaunchStateCrashKey(KioskLaunchState::kLauncherStarted);
  profile_controls().OnProfileLoaded(profile());

  launcher().observers().NotifyAppInstalling();

  EXPECT_THAT(
      view(),
      HasViewState(
          AppLaunchSplashScreenView::AppLaunchState::kInstallingApplication));
}

TEST_F(KioskLaunchControllerTest, AppPreparedShouldUpdateInternalState) {
  RunUntilAppPrepared();

  EXPECT_THAT(controller(),
              HasState(AppState::kInstalled, NetworkUIState::kNotShowing));
  EXPECT_THAT(
      view(),
      HasViewState(
          AppLaunchSplashScreenView::AppLaunchState::kWaitingAppWindow));
}

TEST_F(KioskLaunchControllerTest, SplashScreenTimerShouldLaunchPreparedApp) {
  RunUntilAppPrepared();
  EXPECT_FALSE(launcher().HasAppLaunched());

  FireSplashScreenTimer();
  EXPECT_TRUE(launcher().HasAppLaunched());
}

TEST_F(KioskLaunchControllerTest, SplashScreenTimeoutShouldBeConfigurable) {
  const int kTimeStep = 15;

  base::test::ScopedCommandLine command_line;
  command_line.GetProcessCommandLine()->AppendSwitchASCII(
      ash::switches::kKioskSplashScreenMinTimeSeconds,
      base::NumberToString(2 * kTimeStep));

  RunUntilAppPrepared();
  EXPECT_FALSE(launcher().HasAppLaunched());

  task_environment()->FastForwardBy(base::Seconds(kTimeStep));
  EXPECT_FALSE(launcher().HasAppLaunched());

  task_environment()->FastForwardBy(base::Seconds(kTimeStep));
  EXPECT_TRUE(launcher().HasAppLaunched());
}

TEST_F(KioskLaunchControllerTest,
       SplashScreenTimerShouldNotLaunchUnpreparedApp) {
  controller().Start(kiosk_app_id(), /*auto_launch=*/false);
  profile_controls().OnProfileLoaded(profile());
  launcher().observers().NotifyAppInstalling();

  FireSplashScreenTimer();
  EXPECT_FALSE(launcher().HasAppLaunched());

  launcher().observers().NotifyAppPrepared();
  EXPECT_TRUE(launcher().HasAppLaunched());
}

TEST_F(KioskLaunchControllerTest, AppLaunchedShouldStartSession) {
  RunUntilAppPrepared();
  FireSplashScreenTimer();

  launcher().observers().NotifyAppLaunched();

  EXPECT_THAT(controller(),
              HasState(AppState::kLaunched, NetworkUIState::kNotShowing));
  EXPECT_THAT(
      view(),
      HasViewState(
          AppLaunchSplashScreenView::AppLaunchState::kWaitingAppWindow));
  EXPECT_TRUE(session_manager::SessionManager::Get()->IsSessionStarted());
}

TEST_F(KioskLaunchControllerTest,
       InitializeLauncherShouldSignalNetworkRequired) {
  controller().Start(kiosk_app_id(), /*auto_launch=*/false);
  profile_controls().OnProfileLoaded(profile());

  network_delegate().InitializeNetwork();
  EXPECT_TRUE(view().IsNetworkRequired());
}

TEST_F(KioskLaunchControllerTest,
       NetworkPresentShouldInvokeContinueWithNetworkReady) {
  controller().Start(kiosk_app_id(), /*auto_launch=*/false);
  profile_controls().OnProfileLoaded(profile());

  network_delegate().InitializeNetwork();
  EXPECT_THAT(controller(),
              HasState(AppState::kInitNetwork, NetworkUIState::kNotShowing));
  EXPECT_THAT(
      view(),
      HasViewState(
          AppLaunchSplashScreenView::AppLaunchState::kPreparingNetwork));
  EXPECT_FALSE(launcher().HasContinueWithNetworkReadyBeenCalled());

  SetOnline(true);
  EXPECT_TRUE(launcher().HasContinueWithNetworkReadyBeenCalled());
}

TEST_F(KioskLaunchControllerTest,
       NetworkInitTimeoutShouldShowNetworkConfigureUI) {
  controller().Start(kiosk_app_id(), /*auto_launch=*/false);
  profile_controls().OnProfileLoaded(profile());

  network_delegate().InitializeNetwork();
  EXPECT_THAT(controller(),
              HasState(AppState::kInitNetwork, NetworkUIState::kNotShowing));
  EXPECT_THAT(
      view(),
      HasViewState(
          AppLaunchSplashScreenView::AppLaunchState::kPreparingNetwork));

  task_environment()->FastForwardBy(base::Seconds(10));

  EXPECT_THAT(controller(),
              HasState(AppState::kInitNetwork, NetworkUIState::kShowing));
}

TEST_F(KioskLaunchControllerTest,
       UserRequestedNetworkConfigShouldWaitForProfileLoad) {
  controller().Start(kiosk_app_id(), /*auto_launch=*/false);
  VerifyLaunchStateCrashKey(KioskLaunchState::kLauncherStarted);
  EXPECT_THAT(controller(), HasState(AppState::kCreatingProfile,
                                     NetworkUIState::kNotShowing));

  // User presses the hotkey.
  OnNetworkConfigRequested();
  EXPECT_THAT(controller(), HasState(AppState::kCreatingProfile,
                                     NetworkUIState::kNeedToShow));
  VerifyLaunchStateCrashKey(KioskLaunchState::kLauncherStarted);

  profile_controls().OnProfileLoaded(profile());

  EXPECT_THAT(controller(),
              HasState(AppState::kCreatingProfile, NetworkUIState::kShowing));
  EXPECT_THAT(view(), HasViewState(AppLaunchSplashScreenView::AppLaunchState::
                                       kShowingNetworkConfigureUI));
}

TEST_F(KioskLaunchControllerTest, ConfigureNetworkDuringInstallation) {
  SetOnline(false);
  controller().Start(kiosk_app_id(), /*auto_launch=*/false);
  VerifyLaunchStateCrashKey(KioskLaunchState::kLauncherStarted);
  EXPECT_THAT(controller(), HasState(AppState::kCreatingProfile,
                                     NetworkUIState::kNotShowing));
  profile_controls().OnProfileLoaded(profile());

  launcher().observers().NotifyAppInstalling();

  // User presses the hotkey, current installation is canceled.
  OnNetworkConfigRequested();

  EXPECT_THAT(controller(),
              HasState(AppState::kInitNetwork, NetworkUIState::kShowing));
  EXPECT_THAT(
      view(),
      HasViewState(
          AppLaunchSplashScreenView::AppLaunchState::kInstallingApplication));

  view_controls().OnNetworkConfigFinished();
  EXPECT_THAT(
      view(),
      HasViewState(
          AppLaunchSplashScreenView::AppLaunchState::kPreparingProfile));
  EXPECT_TRUE(launcher().IsInitialized());
}

TEST_F(KioskLaunchControllerTest, KioskProfileLoadFailedObserverShouldBeFired) {
  MockKioskProfileLoadFailedObserver profile_load_failed_observer;
  controller().AddKioskProfileLoadFailedObserver(&profile_load_failed_observer);

  controller().Start(kiosk_app_id(), /*auto_launch=*/false);
  EXPECT_THAT(controller(), HasState(AppState::kCreatingProfile,
                                     NetworkUIState::kNotShowing));

  EXPECT_CALL(profile_load_failed_observer, OnKioskProfileLoadFailed())
      .Times(1);
  profile_controls().OnProfileLoadFailed(
      KioskAppLaunchError::Error::kUnableToMount);
  VerifyLaunchStateCrashKey(KioskLaunchState::kLaunchFailed);

  controller().RemoveKioskProfileLoadFailedObserver(
      &profile_load_failed_observer);
  EXPECT_EQ(num_launchers_created(), 0);
}

TEST_F(KioskLaunchControllerTest, KioskProfileLoadErrorShouldBeStored) {
  controller().Start(kiosk_app_id(), /*auto_launch=*/false);

  profile_controls().OnProfileLoadFailed(
      KioskAppLaunchError::Error::kUnableToMount);
  VerifyLaunchStateCrashKey(KioskLaunchState::kLaunchFailed);

  const base::Value::Dict& dict =
      g_browser_process->local_state()->GetDict("kiosk");
  EXPECT_THAT(dict.FindInt("launch_error"),
              Eq(static_cast<int>(KioskAppLaunchError::Error::kUnableToMount)));
}

TEST_F(KioskLaunchControllerTest,
       LaunchShouldCompleteAfterNetworkRequiredDuringAppLaunch) {
  SetOnline(false);
  RunUntilAppPrepared();
  FireSplashScreenTimer();
  EXPECT_EQ(launcher().launch_app_called(), 1);

  // Network required during app launch
  network_delegate().InitializeNetwork();
  EXPECT_THAT(controller(),
              HasState(AppState::kInitNetwork, NetworkUIState::kNotShowing));
  EXPECT_FALSE(launcher().HasContinueWithNetworkReadyBeenCalled());

  SetOnline(true);
  EXPECT_TRUE(launcher().HasContinueWithNetworkReadyBeenCalled());

  launcher().observers().NotifyAppPrepared();
  EXPECT_EQ(launcher().launch_app_called(), 2);
}

class KioskLaunchControllerWithExtensionTest
    : public KioskLaunchControllerTest {
 public:
  void SetForceInstallPolicy(const std::string& extension_id,
                             const std::string& update_url) {
    base::Value::List list;
    list.Append(extension_id + ";" + update_url);
    policy::PolicyMap map;
    map.Set(policy::key::kExtensionInstallForcelist,
            policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
            policy::POLICY_SOURCE_CLOUD, base::Value(std::move(list)), nullptr);

    policy_provider()->UpdateChromePolicy(map);
    base::RunLoop().RunUntilIdle();
  }

  extensions::ForceInstalledTracker* force_installed_tracker() {
    return extensions::ExtensionSystem::Get(profile())
        ->extension_service()
        ->force_installed_tracker();
  }

  void SetExtensionReady(const std::string& extension_id,
                         const std::string& extension_name) {
    force_installed_tracker()->OnExtensionReady(
        profile(), BuildExtension(extension_name, extension_id).get());
  }

  void SetExtensionFailed(
      const std::string& extension_id,
      const std::string& extension_name,
      extensions::InstallStageTracker::FailureReason reason) {
    force_installed_tracker()->OnExtensionInstallationFailed(
        BuildExtension(extension_name, extension_id)->id(), reason);
  }
};

TEST_F(KioskLaunchControllerWithExtensionTest,
       ExtensionLoadedBeforeAppPreparedShouldMoveIntoInstalledState) {
  base::HistogramTester histogram;

  SetForceInstallPolicy(kExtensionId, kWebStoreExtensionUpdateUrl);
  SetExtensionReady(kExtensionId, kExtensionName);

  RunUntilAppPrepared();

  EXPECT_THAT(controller(),
              HasState(AppState::kInstalled, NetworkUIState::kNotShowing));
  EXPECT_THAT(
      view(),
      HasViewState(
          AppLaunchSplashScreenView::AppLaunchState::kWaitingAppWindow));

  FireSplashScreenTimer();
  EXPECT_TRUE(launcher().HasAppLaunched());

  launcher().observers().NotifyAppLaunched();
  EXPECT_THAT(controller(),
              HasState(AppState::kLaunched, NetworkUIState::kNotShowing));
  EXPECT_THAT(
      view(),
      HasViewState(
          AppLaunchSplashScreenView::AppLaunchState::kWaitingAppWindow));
  EXPECT_TRUE(session_manager::SessionManager::Get()->IsSessionStarted());

  histogram.ExpectTotalCount("Kiosk.Extensions.InstallTimedOut", 0);
}

TEST_F(KioskLaunchControllerWithExtensionTest,
       ExtensionLoadedBeforeSplashScreenTimerShouldNotLaunchApp) {
  base::HistogramTester histogram;

  SetForceInstallPolicy(kExtensionId, kWebStoreExtensionUpdateUrl);
  RunUntilAppPrepared();
  EXPECT_THAT(controller(), HasState(AppState::kInstallingExtensions,
                                     NetworkUIState::kNotShowing));
  EXPECT_THAT(
      view(),
      HasViewState(
          AppLaunchSplashScreenView::AppLaunchState::kInstallingExtension));

  SetExtensionReady(kExtensionId, kExtensionName);
  EXPECT_THAT(controller(),
              HasState(AppState::kInstalled, NetworkUIState::kNotShowing));
  EXPECT_THAT(
      view(),
      HasViewState(
          AppLaunchSplashScreenView::AppLaunchState::kWaitingAppWindow));
  EXPECT_FALSE(launcher().HasAppLaunched());

  histogram.ExpectBucketCount("Kiosk.Extensions.InstallTimedOut", false, 1);
}

TEST_F(KioskLaunchControllerWithExtensionTest,
       ExtensionLoadedAfterSplashScreenTimerShouldLaunchApp) {
  SetForceInstallPolicy(kExtensionId, kWebStoreExtensionUpdateUrl);
  RunUntilAppPrepared();
  FireSplashScreenTimer();

  EXPECT_THAT(controller(), HasState(AppState::kInstallingExtensions,
                                     NetworkUIState::kNotShowing));
  EXPECT_THAT(
      view(),
      HasViewState(
          AppLaunchSplashScreenView::AppLaunchState::kInstallingExtension));

  SetExtensionReady(kExtensionId, kExtensionName);
  EXPECT_THAT(controller(),
              HasState(AppState::kInstalled, NetworkUIState::kNotShowing));
  EXPECT_THAT(
      view(),
      HasViewState(
          AppLaunchSplashScreenView::AppLaunchState::kWaitingAppWindow));
  EXPECT_TRUE(launcher().HasAppLaunched());
}

TEST_F(KioskLaunchControllerWithExtensionTest,
       AppLaunchShouldContinueDespiteExtensionInstallTimeout) {
  base::HistogramTester histogram;

  SetForceInstallPolicy(kExtensionId, kWebStoreExtensionUpdateUrl);
  RunUntilAppPrepared();
  EXPECT_THAT(controller(), HasState(AppState::kInstallingExtensions,
                                     NetworkUIState::kNotShowing));
  EXPECT_THAT(
      view(),
      HasViewState(
          AppLaunchSplashScreenView::AppLaunchState::kInstallingExtension));

  FireSplashScreenTimer();

  task_environment()->FastForwardBy(base::Minutes(2));

  EXPECT_TRUE(launcher().HasAppLaunched());
  EXPECT_THAT(controller(),
              HasState(AppState::kInstalled, NetworkUIState::kNotShowing));
  EXPECT_THAT(
      view(),
      HasViewState(
          AppLaunchSplashScreenView::AppLaunchState::kWaitingAppWindow));
  EXPECT_THAT(view(), HasErrorMessage(
                          KioskAppLaunchError::Error::kExtensionsLoadTimeout));

  histogram.ExpectBucketCount("Kiosk.Extensions.InstallTimedOut", true, 1);
}

TEST_F(KioskLaunchControllerWithExtensionTest,
       AppLaunchShouldContinueDespiteExtensionInstallFailure) {
  base::HistogramTester histogram;

  SetForceInstallPolicy(kExtensionId, kWebStoreExtensionUpdateUrl);
  RunUntilAppPrepared();
  EXPECT_THAT(controller(), HasState(AppState::kInstallingExtensions,
                                     NetworkUIState::kNotShowing));
  EXPECT_THAT(
      view(),
      HasViewState(
          AppLaunchSplashScreenView::AppLaunchState::kInstallingExtension));

  SetExtensionFailed(
      kExtensionId, kExtensionName,
      extensions::InstallStageTracker::FailureReason::INVALID_ID);

  FireSplashScreenTimer();
  EXPECT_TRUE(launcher().HasAppLaunched());
}

TEST_F(KioskLaunchControllerWithExtensionTest,
       AppLaunchShouldContinueDespiteInvalidExtensionPolicy) {
  base::HistogramTester histogram;

  SetForceInstallPolicy(kInvalidExtensionId, kWebStoreExtensionUpdateUrl);
  RunUntilAppPrepared();

  EXPECT_THAT(
      view(),
      HasErrorMessage(KioskAppLaunchError::Error::kExtensionsPolicyInvalid));

  FireSplashScreenTimer();
  EXPECT_TRUE(launcher().HasAppLaunched());

  histogram.ExpectTotalCount("Kiosk.Extensions.InstallTimedOut", 0);
}

TEST_F(KioskLaunchControllerWithExtensionTest,
       WebStoreExtensionFailureShouldBeLogged) {
  base::HistogramTester histogram;

  SetForceInstallPolicy(kExtensionId, kWebStoreExtensionUpdateUrl);
  RunUntilAppPrepared();
  EXPECT_THAT(controller(), HasState(AppState::kInstallingExtensions,
                                     NetworkUIState::kNotShowing));
  EXPECT_THAT(
      view(),
      HasViewState(
          AppLaunchSplashScreenView::AppLaunchState::kInstallingExtension));

  SetExtensionFailed(
      kExtensionId, kExtensionName,
      extensions::InstallStageTracker::FailureReason::INVALID_ID);

  histogram.ExpectUniqueSample(
      "Kiosk.Extensions.InstallError.WebStore",
      extensions::InstallStageTracker::FailureReason::INVALID_ID, 1);
}

TEST_F(KioskLaunchControllerWithExtensionTest,
       OffStoreExtensionFailureShouldBeLogged) {
  base::HistogramTester histogram;

  SetForceInstallPolicy(kExtensionId, kOffStoreExtensionUpdateUrl);
  RunUntilAppPrepared();

  SetExtensionFailed(
      kExtensionId, kExtensionName,
      extensions::InstallStageTracker::FailureReason::INVALID_ID);

  histogram.ExpectUniqueSample(
      "Kiosk.Extensions.InstallError.OffStore",
      extensions::InstallStageTracker::FailureReason::INVALID_ID, 1);
}

TEST_F(KioskLaunchControllerTest, TestFullFlow) {
  SetOnline(true);

  EXPECT_EQ(num_launchers_created(), 0);

  controller().Start(kiosk_app_id(), /*auto_launch=*/false);

  EXPECT_EQ(num_launchers_created(), 0);

  profile_controls().OnProfileLoaded(profile());

  EXPECT_EQ(launcher().initialize_called(), 1);
  EXPECT_FALSE(launcher().HasAppLaunched());
  EXPECT_FALSE(launcher().HasContinueWithNetworkReadyBeenCalled());

  launcher().observers().NotifyAppInstalling();

  network_delegate().InitializeNetwork();

  EXPECT_EQ(launcher().initialize_called(), 1);
  EXPECT_EQ(launcher().continue_with_network_ready_called(), 1);
  EXPECT_FALSE(launcher().HasAppLaunched());

  launcher().observers().NotifyAppPrepared();

  FireSplashScreenTimer();

  EXPECT_EQ(launcher().initialize_called(), 1);
  EXPECT_EQ(launcher().continue_with_network_ready_called(), 1);
  EXPECT_EQ(launcher().launch_app_called(), 1);
  EXPECT_EQ(num_launchers_created(), 1);
}
}  // namespace ash
