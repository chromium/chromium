// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/app_mode/kiosk_launch_controller.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "ash/constants/ash_switches.h"
#include "ash/public/cpp/login_accelerators.h"
#include "base/auto_reset.h"
#include "base/check_deref.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_command_line.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "base/types/expected.h"
#include "chrome/browser/ash/app_mode/cancellable_job.h"
#include "chrome/browser/ash/app_mode/fake_kiosk_app_launcher.h"
#include "chrome/browser/ash/app_mode/kiosk_app.h"
#include "chrome/browser/ash/app_mode/kiosk_app_launch_error.h"
#include "chrome/browser/ash/app_mode/kiosk_app_launcher.h"
#include "chrome/browser/ash/app_mode/kiosk_app_types.h"
#include "chrome/browser/ash/app_mode/kiosk_chrome_app_manager.h"
#include "chrome/browser/ash/app_mode/kiosk_controller.h"
#include "chrome/browser/ash/app_mode/kiosk_controller_impl.h"
#include "chrome/browser/ash/app_mode/kiosk_launch_state.h"
#include "chrome/browser/ash/app_mode/kiosk_profile_load_failed_observer.h"
#include "chrome/browser/ash/app_mode/load_profile.h"
#include "chrome/browser/ash/app_mode/web_app/web_kiosk_app_manager.h"
#include "chrome/browser/ash/login/app_mode/network_ui_controller.h"
#include "chrome/browser/ash/login/screens/fake_app_launch_splash_screen.h"
#include "chrome/browser/ash/login/screens/network_error.h"
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/ash/settings/scoped_cros_settings_test_helper.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_service_test_base.h"
#include "chrome/browser/extensions/forced_extensions/force_installed_tracker.h"
#include "chrome/browser/extensions/forced_extensions/install_stage_tracker.h"
#include "chrome/browser/ui/ash/keyboard/chrome_keyboard_controller_client_test_helper.h"
#include "chrome/browser/ui/webui/ash/login/app_launch_splash_screen_handler.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "chromeos/ash/components/install_attributes/stub_install_attributes.h"
#include "chromeos/ash/components/network/network_handler.h"
#include "chromeos/ash/components/standalone_browser/feature_refs.h"
#include "chromeos/ash/components/standalone_browser/standalone_browser_features.h"
#include "components/account_id/account_id.h"
#include "components/crash/core/common/crash_key.h"
#include "components/policy/core/browser/browser_policy_connector_base.h"
#include "components/policy/core/common/device_local_account_type.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_types.h"
#include "components/policy/policy_constants.h"
#include "components/session_manager/core/session_manager.h"
#include "components/user_manager/scoped_user_manager.h"
#include "content/public/test/browser_task_environment.h"
#include "extensions/browser/extension_system.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_builder.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/image/image_skia.h"
#include "url/gurl.h"

namespace ash {

using kiosk::LoadProfileCallback;
using kiosk::LoadProfileResultCallback;

namespace {

using ::testing::Eq;

const char kInstallUrl[] = "https://install.url";
const char kExtensionId[] = "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";
const char kInvalidExtensionId[] = "invalid-extension-id";
const char kExtensionName[] = "extension_name";
const char kTestDomain[] = "test.com";
const char kDeviceId[] = "123";
const char kDefaultNetwork[] = "default-network";

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

class FakeNetworkMonitor : public ash::NetworkUiController::NetworkMonitor {
 public:
  using Observer = ash::NetworkUiController::NetworkMonitor::Observer;
  using State = ash::NetworkUiController::NetworkMonitor::State;

  FakeNetworkMonitor() = default;
  ~FakeNetworkMonitor() override = default;

  void AddObserver(Observer* observer) override { observer_ = observer; }

  void RemoveObserver(Observer* observer) override { observer_ = nullptr; }

  State GetState() const override {
    return online_ ? State::ONLINE : State::OFFLINE;
  }

  std::string GetNetworkName() const override { return kDefaultNetwork; }

  void SetOnline(bool online) {
    online_ = online;
    if (observer_) {
      observer_->UpdateState(
          NetworkError::ErrorReason::ERROR_REASON_NETWORK_STATE_CHANGED);
    }
  }

  base::WeakPtr<FakeNetworkMonitor> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

 private:
  bool online_ = false;
  raw_ptr<ash::NetworkUiController::NetworkMonitor::Observer> observer_;
  base::WeakPtrFactory<FakeNetworkMonitor> weak_ptr_factory_{this};
};

class FakeAcceleratorController
    : public KioskLaunchController::AcceleratorController {
 public:
  FakeAcceleratorController() = default;
  ~FakeAcceleratorController() override = default;

  void EnableAccelerators() override { enabled_ = true; }

  void DisableAccelerators() override { enabled_ = false; }

  bool enabled() { return enabled_; }

 private:
  bool enabled_ = false;
};

}  // namespace

using NetworkUIState = NetworkUiController::NetworkUIState;

class MockKioskProfileLoadFailedObserver
    : public KioskProfileLoadFailedObserver {
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

  KioskLaunchControllerTest()
      : extensions::ExtensionServiceTestBase(
            std::make_unique<content::BrowserTaskEnvironment>(
                base::test::TaskEnvironment::TimeSource::MOCK_TIME)) {}

  KioskLaunchControllerTest(const KioskLaunchControllerTest&) = delete;
  KioskLaunchControllerTest& operator=(const KioskLaunchControllerTest&) =
      delete;

  void SetUp() override {
    SetDeviceEnterpriseManaged();
    InitializeEmptyExtensionService();
    policy::BrowserPolicyConnectorBase::SetPolicyServiceForTesting(
        policy_service());

    keyboard_controller_client_ =
        ChromeKeyboardControllerClientTestHelper::InitializeWithFake();

    can_configure_network_for_testing_ =
        NetworkUiController::SetCanConfigureNetworkForTesting(true);

    auto network_monitor_unique = std::make_unique<FakeNetworkMonitor>();
    network_monitor_ = network_monitor_unique->GetWeakPtr();
    auto fake_accelerator_controller =
        std::make_unique<FakeAcceleratorController>();
    accelerator_controller_ = fake_accelerator_controller.get();
    controller_ = std::make_unique<KioskLaunchController>(
        /*host=*/nullptr, &screen_, FakeLoadProfileCallback(),
        /*app_launched_callback=*/app_launched_future_.GetCallback(),
        /*done_callback=*/launch_done_future_.GetCallback(),
        /*attempt_restart=*/base::DoNothing(),
        /*attempt_logout=*/base::DoNothing(),
        base::BindRepeating(
            &KioskLaunchControllerTest::BuildFakeKioskAppLauncher,
            base::Unretained(this)),
        std::move(network_monitor_unique),
        std::move(fake_accelerator_controller));

    // We can't call `crash_reporter::ResetCrashKeysForTesting()` to reset crash
    // keys since it destroys the storage for static crash keys. Instead we set
    // the initial state to `KioskLaunchState::kStartLaunch` before testing.
    SetKioskLaunchStateCrashKey(KioskLaunchState::kStartLaunch);

    SetUpKioskAppId();

    extensions::ExtensionServiceTestBase::SetUp();

    LoginFakeUser();
  }

  void TearDown() override {
    extensions::ExtensionServiceTestBase::TearDown();

    policy::BrowserPolicyConnectorBase::SetPolicyServiceForTesting(nullptr);
  }

  KioskLaunchController& controller() { return *controller_; }

  KioskAppLauncher::NetworkDelegate& network_delegate() {
    return *controller_->GetNetworkUiControllerForTesting();
  }

  FakeKioskAppLauncher& launcher() { return *app_launcher_; }

  FakeAcceleratorController& accelerator_controller() {
    return *accelerator_controller_;
  }

  int num_launchers_created() { return app_launchers_created_; }

  auto HasState(AppState app_state, NetworkUIState network_state) {
    return testing::AllOf(
        testing::Field("app_state", &KioskLaunchController::app_state_,
                       Eq(app_state)),
        testing::Property("network_ui_state",
                          &KioskLaunchController::GetNetworkUiStateForTesting,
                          Eq(network_state)));
  }

  auto HasViewState(AppLaunchSplashScreenView::AppLaunchState launch_state) {
    return testing::Property("GetAppLaunchState",
                             &FakeAppLaunchSplashScreen::GetAppLaunchState,
                             Eq(launch_state));
  }

  auto HasErrorMessage(KioskAppLaunchError::Error error) {
    return testing::Property(
        "ErrorState", &FakeAppLaunchSplashScreen::GetLaunchError, Eq(error));
  }

  void FireSplashScreenTimer() {
    task_environment()->FastForwardBy(kDefaultKioskSplashScreenMinTime);
  }

  void SetOnline(bool online) { network_monitor_->SetOnline(online); }

  void OnNetworkConfigRequested() { controller().OnNetworkConfigRequested(); }

  FakeAppLaunchSplashScreen& screen() { return screen_; }

  KioskAppId kiosk_app_id() { return kiosk_app_id_; }

  KioskApp kiosk_app() {
    return KioskApp{kiosk_app_id_,
                    /*name=*/"test-app-name",
                    /*icon=*/gfx::ImageSkia(),
                    /*url=*/GURL(kInstallUrl)};
  }

  void FinishLoadingProfile() {
    std::move(on_profile_loaded_callback_).Run(profile());
  }
  void FinishLoadingProfileWithError(KioskAppLaunchError::Error error) {
    std::move(on_profile_loaded_callback_).Run(base::unexpected(error));
  }

  void RunUntilAppPrepared() {
    controller().Start(kiosk_app(), /*auto_launch=*/false);
    FinishLoadingProfile();
    launcher().observers().NotifyAppInstalling();
    launcher().observers().NotifyAppPrepared();
  }

  void VerifyLaunchStateCrashKey(KioskLaunchState state) {
    EXPECT_EQ(crash_reporter::GetCrashKeyValue(kKioskLaunchStateCrashKey),
              KioskLaunchStateToString(state));
  }

  void CancelAppLaunch() { controller().HandleAccelerator(kAppLaunchBailout); }

  void CleanUpController() { controller().CleanUp(); }

  void LoginFakeUser() {
    fake_user_manager_->AddWebKioskAppUser(kiosk_app_id().account_id);
    fake_user_manager_->LoginUser(kiosk_app_id().account_id);
  }

  auto& app_launched_future() { return app_launched_future_; }

  auto& launch_done_future() { return launch_done_future_; }

 private:
  void SetDeviceEnterpriseManaged() {
    cros_settings_test_helper().InstallAttributes()->SetCloudManaged(
        kTestDomain, kDeviceId);
  }

  void SetUpKioskAppId() {
    std::string email = policy::GenerateDeviceLocalAccountUserId(
        kInstallUrl, policy::DeviceLocalAccountType::kWebKioskApp);
    AccountId account_id(AccountId::FromUserEmail(email));
    kiosk_app_id_ = KioskAppId::ForWebApp(account_id);
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

  LoadProfileCallback FakeLoadProfileCallback() {
    return base::BindLambdaForTesting([&](const AccountId& app_account_id,
                                          KioskAppType app_type,
                                          LoadProfileResultCallback on_done) {
      on_profile_loaded_callback_ = std::move(on_done);
      return std::unique_ptr<CancellableJob>{};
    });
  }

  TestingProfile profile_;
  session_manager::SessionManager session_manager_;
  std::unique_ptr<ChromeKeyboardControllerClientTestHelper>
      keyboard_controller_client_;

  base::test::
      TestFuture<const KioskAppId&, Profile*, const std::optional<std::string>&>
          app_launched_future_;

  base::test::TestFuture<KioskAppLaunchError::Error> launch_done_future_;

  base::AutoReset<std::optional<bool>> can_configure_network_for_testing_ =
      NetworkUiController::SetCanConfigureNetworkForTesting(true);

  user_manager::TypedScopedUserManager<ash::FakeChromeUserManager>
      fake_user_manager_{std::make_unique<ash::FakeChromeUserManager>()};

  LoadProfileResultCallback on_profile_loaded_callback_;
  FakeAppLaunchSplashScreen screen_;

  int app_launchers_created_ = 0;
  std::unique_ptr<KioskLaunchController> controller_;

  // owned by `controller_`.
  raw_ptr<FakeKioskAppLauncher, DanglingUntriaged> app_launcher_ = nullptr;
  // owned by `controller_`.
  base::WeakPtr<FakeNetworkMonitor> network_monitor_;
  // owned by `controller_`.
  raw_ptr<FakeAcceleratorController> accelerator_controller_ = nullptr;

  KioskAppId kiosk_app_id_;
};

TEST_F(KioskLaunchControllerTest, StartShouldShowAppDataOnSplashScreen) {
  controller().Start(kiosk_app(), /*auto_launch=*/false);
  EXPECT_EQ(screen().GetAppData().url, GURL(kInstallUrl));
}

TEST_F(KioskLaunchControllerTest, ControllerShouldDisableAccelerators) {
  controller().Start(kiosk_app(), /*auto_launch=*/false);
  EXPECT_FALSE(accelerator_controller().enabled());
}

TEST_F(KioskLaunchControllerTest, CleanUpShouldReenableAccelerators) {
  controller().Start(kiosk_app(), /*auto_launch=*/false);
  CleanUpController();
  EXPECT_TRUE(accelerator_controller().enabled());
}

TEST_F(KioskLaunchControllerTest, ProfileLoadedShouldInitializeLauncher) {
  controller().Start(kiosk_app(), /*auto_launch=*/false);
  VerifyLaunchStateCrashKey(KioskLaunchState::kLauncherStarted);
  EXPECT_THAT(controller(), HasState(AppState::kCreatingProfile,
                                     NetworkUIState::kNotShowing));
  FinishLoadingProfile();
  EXPECT_TRUE(launcher().IsInitialized());
}

TEST_F(KioskLaunchControllerTest, ProfileLoadDoesNotLaunchAppAfterCleanUp) {
  controller().Start(kiosk_app(), /*auto_launch=*/false);
  EXPECT_THAT(controller(), HasState(AppState::kCreatingProfile,
                                     NetworkUIState::kNotShowing));

  CleanUpController();
  FinishLoadingProfile();

  EXPECT_EQ(num_launchers_created(), 0);
}

TEST_F(KioskLaunchControllerTest, AppInstallingShouldUpdateSplashScreen) {
  controller().Start(kiosk_app(), /*auto_launch=*/false);
  VerifyLaunchStateCrashKey(KioskLaunchState::kLauncherStarted);
  FinishLoadingProfile();

  launcher().observers().NotifyAppInstalling();

  EXPECT_THAT(
      screen(),
      HasViewState(
          AppLaunchSplashScreenView::AppLaunchState::kInstallingApplication));
}

TEST_F(KioskLaunchControllerTest, AppPreparedShouldUpdateInternalState) {
  RunUntilAppPrepared();

  EXPECT_THAT(controller(),
              HasState(AppState::kInstalled, NetworkUIState::kNotShowing));
  EXPECT_THAT(
      screen(),
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
  controller().Start(kiosk_app(), /*auto_launch=*/false);
  FinishLoadingProfile();
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
      screen(),
      HasViewState(
          AppLaunchSplashScreenView::AppLaunchState::kWaitingAppWindow));
  EXPECT_TRUE(session_manager::SessionManager::Get()->IsSessionStarted());
}

TEST_F(KioskLaunchControllerTest, AppWindowCreatedShouldInvokeOnDoneCallback) {
  RunUntilAppPrepared();
  FireSplashScreenTimer();
  launcher().observers().NotifyAppLaunched();
  ASSERT_FALSE(app_launched_future().IsReady());
  ASSERT_FALSE(launch_done_future().IsReady());

  launcher().observers().NotifyAppWindowCreated("app-name");

  ASSERT_TRUE(app_launched_future().IsReady());
  ASSERT_TRUE(launch_done_future().IsReady());

  const auto [app_id, profile, app_name] = app_launched_future().Take();
  auto error_maybe = launch_done_future().Take();
  EXPECT_EQ(app_id, kiosk_app_id());
  EXPECT_EQ(error_maybe, KioskAppLaunchError::Error::kNone);
  EXPECT_NE(profile, nullptr);
  EXPECT_EQ(app_name, "app-name");
}

TEST_F(KioskLaunchControllerTest, SplashScreenTimerShouldInvokeOnDoneCallback) {
  RunUntilAppPrepared();
  ASSERT_FALSE(app_launched_future().IsReady());
  launcher().observers().NotifyAppLaunched();
  launcher().observers().NotifyAppWindowCreated("app-name");

  // App launched but launch is not done yet. The splash screen remains up until
  // the timer is fired.
  ASSERT_TRUE(app_launched_future().IsReady());
  ASSERT_FALSE(launch_done_future().IsReady());

  FireSplashScreenTimer();

  ASSERT_TRUE(launch_done_future().IsReady());

  const auto [app_id, profile, app_name] = app_launched_future().Take();
  auto error_maybe = launch_done_future().Take();
  EXPECT_EQ(app_id, kiosk_app_id());
  EXPECT_EQ(error_maybe, KioskAppLaunchError::Error::kNone);
  EXPECT_NE(profile, nullptr);
  EXPECT_EQ(app_name, "app-name");
}

TEST_F(KioskLaunchControllerTest, ShouldInvokeOnDoneCallbackOnError) {
  controller().Start(kiosk_app(), /*auto_launch=*/false);

  ASSERT_FALSE(app_launched_future().IsReady());
  ASSERT_FALSE(launch_done_future().IsReady());
  FinishLoadingProfileWithError(KioskAppLaunchError::Error::kUnableToMount);
  ASSERT_FALSE(app_launched_future().IsReady());
  ASSERT_TRUE(launch_done_future().IsReady());

  auto error_maybe = launch_done_future().Take();
  EXPECT_EQ(error_maybe, KioskAppLaunchError::Error::kUnableToMount);
}

TEST_F(KioskLaunchControllerTest,
       ShouldInvokeOnDoneCallbackOnAlreadyMountedError) {
  // The Already Mounted error has it's own `return` statement so this test
  // ensures the callback is also invoked in this code path.
  controller().Start(kiosk_app(), /*auto_launch=*/false);

  ASSERT_FALSE(app_launched_future().IsReady());
  ASSERT_FALSE(launch_done_future().IsReady());
  FinishLoadingProfileWithError(KioskAppLaunchError::Error::kAlreadyMounted);
  ASSERT_FALSE(app_launched_future().IsReady());
  ASSERT_TRUE(launch_done_future().IsReady());

  auto error_maybe = launch_done_future().Take();
  EXPECT_EQ(error_maybe, KioskAppLaunchError::Error::kAlreadyMounted);
}

TEST_F(KioskLaunchControllerTest,
       NetworkPresentShouldInvokeContinueWithNetworkReady) {
  controller().Start(kiosk_app(), /*auto_launch=*/false);
  FinishLoadingProfile();

  network_delegate().InitializeNetwork();
  EXPECT_THAT(controller(), HasState(AppState::kInitLauncher,
                                     NetworkUIState::kWaitingForNetwork));
  EXPECT_THAT(
      screen(),
      HasViewState(
          AppLaunchSplashScreenView::AppLaunchState::kPreparingNetwork));
  EXPECT_FALSE(launcher().HasContinueWithNetworkReadyBeenCalled());

  SetOnline(true);
  EXPECT_TRUE(launcher().HasContinueWithNetworkReadyBeenCalled());
}

TEST_F(KioskLaunchControllerTest,
       NetworkInitTimeoutShouldShowNetworkConfigureUI) {
  controller().Start(kiosk_app(), /*auto_launch=*/false);
  FinishLoadingProfile();

  network_delegate().InitializeNetwork();
  EXPECT_THAT(controller(), HasState(AppState::kInitLauncher,
                                     NetworkUIState::kWaitingForNetwork));
  EXPECT_THAT(
      screen(),
      HasViewState(
          AppLaunchSplashScreenView::AppLaunchState::kPreparingNetwork));

  task_environment()->FastForwardBy(base::Seconds(10));

  EXPECT_THAT(controller(),
              HasState(AppState::kInitNetwork, NetworkUIState::kShowing));
}

TEST_F(KioskLaunchControllerTest,
       UserRequestedNetworkConfigShouldWaitForProfileLoad) {
  controller().Start(kiosk_app(), /*auto_launch=*/false);
  VerifyLaunchStateCrashKey(KioskLaunchState::kLauncherStarted);
  EXPECT_THAT(controller(), HasState(AppState::kCreatingProfile,
                                     NetworkUIState::kNotShowing));

  // User presses the hotkey.
  OnNetworkConfigRequested();
  EXPECT_THAT(controller(), HasState(AppState::kCreatingProfile,
                                     NetworkUIState::kNeedToShow));
  VerifyLaunchStateCrashKey(KioskLaunchState::kLauncherStarted);

  FinishLoadingProfile();

  EXPECT_THAT(controller(),
              HasState(AppState::kInitNetwork, NetworkUIState::kShowing));
  EXPECT_THAT(screen(), HasViewState(AppLaunchSplashScreenView::AppLaunchState::
                                         kShowingNetworkConfigureUI));
}

TEST_F(KioskLaunchControllerTest, ConfigureNetworkDuringInstallation) {
  SetOnline(false);
  controller().Start(kiosk_app(), /*auto_launch=*/false);
  VerifyLaunchStateCrashKey(KioskLaunchState::kLauncherStarted);
  EXPECT_THAT(controller(), HasState(AppState::kCreatingProfile,
                                     NetworkUIState::kNotShowing));
  FinishLoadingProfile();

  launcher().observers().NotifyAppInstalling();

  // User presses the hotkey, current installation is canceled.
  OnNetworkConfigRequested();

  EXPECT_THAT(controller(),
              HasState(AppState::kInitNetwork, NetworkUIState::kShowing));
  EXPECT_THAT(
      screen(),
      HasViewState(
          AppLaunchSplashScreenView::AppLaunchState::kInstallingApplication));

  screen().ContinueAppLaunch();
  EXPECT_THAT(
      screen(),
      HasViewState(
          AppLaunchSplashScreenView::AppLaunchState::kPreparingProfile));
  EXPECT_TRUE(launcher().IsInitialized());
  EXPECT_EQ(num_launchers_created(), 2);
}

TEST_F(KioskLaunchControllerTest, KioskProfileLoadFailedObserverShouldBeFired) {
  MockKioskProfileLoadFailedObserver profile_load_failed_observer;
  controller().AddKioskProfileLoadFailedObserver(&profile_load_failed_observer);

  controller().Start(kiosk_app(), /*auto_launch=*/false);
  EXPECT_THAT(controller(), HasState(AppState::kCreatingProfile,
                                     NetworkUIState::kNotShowing));

  EXPECT_CALL(profile_load_failed_observer, OnKioskProfileLoadFailed())
      .Times(1);
  FinishLoadingProfileWithError(KioskAppLaunchError::Error::kUnableToMount);
  VerifyLaunchStateCrashKey(KioskLaunchState::kLaunchFailed);
  EXPECT_EQ(num_launchers_created(), 0);

  controller().RemoveKioskProfileLoadFailedObserver(
      &profile_load_failed_observer);
  EXPECT_EQ(num_launchers_created(), 0);
}

TEST_F(KioskLaunchControllerTest, LoadProfileErrorShouldBeStored) {
  controller().Start(kiosk_app(), /*auto_launch=*/false);

  FinishLoadingProfileWithError(KioskAppLaunchError::Error::kUnableToMount);
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
  EXPECT_THAT(controller(), HasState(AppState::kInstalled,
                                     NetworkUIState::kWaitingForNetwork));
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
      screen(),
      HasViewState(
          AppLaunchSplashScreenView::AppLaunchState::kWaitingAppWindow));

  FireSplashScreenTimer();
  EXPECT_TRUE(launcher().HasAppLaunched());

  launcher().observers().NotifyAppLaunched();
  EXPECT_THAT(controller(),
              HasState(AppState::kLaunched, NetworkUIState::kNotShowing));
  EXPECT_THAT(
      screen(),
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
      screen(),
      HasViewState(
          AppLaunchSplashScreenView::AppLaunchState::kInstallingExtension));

  SetExtensionReady(kExtensionId, kExtensionName);
  EXPECT_THAT(controller(),
              HasState(AppState::kInstalled, NetworkUIState::kNotShowing));
  EXPECT_THAT(
      screen(),
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
      screen(),
      HasViewState(
          AppLaunchSplashScreenView::AppLaunchState::kInstallingExtension));

  SetExtensionReady(kExtensionId, kExtensionName);
  EXPECT_THAT(controller(),
              HasState(AppState::kInstalled, NetworkUIState::kNotShowing));
  EXPECT_THAT(
      screen(),
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
      screen(),
      HasViewState(
          AppLaunchSplashScreenView::AppLaunchState::kInstallingExtension));

  FireSplashScreenTimer();

  task_environment()->FastForwardBy(base::Minutes(2));

  EXPECT_TRUE(launcher().HasAppLaunched());
  EXPECT_THAT(controller(),
              HasState(AppState::kInstalled, NetworkUIState::kNotShowing));
  EXPECT_THAT(
      screen(),
      HasViewState(
          AppLaunchSplashScreenView::AppLaunchState::kWaitingAppWindow));
  EXPECT_THAT(
      screen(),
      HasErrorMessage(KioskAppLaunchError::Error::kExtensionsLoadTimeout));

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
      screen(),
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
      screen(),
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
      screen(),
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

  controller().Start(kiosk_app(), /*auto_launch=*/false);

  EXPECT_EQ(num_launchers_created(), 0);

  FinishLoadingProfile();

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
