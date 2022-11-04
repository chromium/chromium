// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/app_mode/test/kiosk_base_test.h"

#include <string>
#include <utility>

#include "apps/test/app_window_waiter.h"
#include "ash/constants/ash_switches.h"
#include "ash/public/cpp/login_screen_test_api.h"
#include "base/callback_forward.h"
#include "base/command_line.h"
#include "base/json/json_reader.h"
#include "base/logging.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/values.h"
#include "chrome/browser/ash/app_mode/kiosk_app_data.h"
#include "chrome/browser/ash/app_mode/kiosk_app_manager.h"
#include "chrome/browser/ash/login/app_mode/kiosk_launch_controller.h"
#include "chrome/browser/ash/login/startup_utils.h"
#include "chrome/browser/ash/login/test/js_checker.h"
#include "chrome/browser/ash/login/test/kiosk_apps_mixin.h"
#include "chrome/browser/ash/login/test/kiosk_test_helpers.h"
#include "chrome/browser/ash/login/test/oobe_screen_waiter.h"
#include "chrome/browser/ash/login/test/oobe_window_visibility_waiter.h"
#include "chrome/browser/ash/login/test/test_condition_waiter.h"
#include "chrome/browser/ash/login/ui/login_display_host.h"
#include "chrome/browser/extensions/browsertest_util.h"
#include "chrome/browser/profiles/profile_impl.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/settings_window_manager_chromeos.h"
#include "chrome/browser/ui/webui/ash/login/error_screen_handler.h"
#include "chrome/browser/ui/webui/settings/chromeos/constants/routes.mojom-forward.h"
#include "extensions/browser/app_window/app_window.h"
#include "extensions/common/extension.h"
#include "extensions/components/native_app_window/native_app_window_views.h"
#include "extensions/test/extension_test_message_listener.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace ash {

namespace {

// Timeout while waiting for network connectivity during tests.
const int kTestNetworkTimeoutSeconds = 1;

// Helper function for GetConsumerKioskAutoLaunchStatusCallback.
void ConsumerKioskAutoLaunchStatusCheck(
    KioskAppManager::ConsumerKioskAutoLaunchStatus* out_status,
    base::OnceClosure runner_quit_task,
    KioskAppManager::ConsumerKioskAutoLaunchStatus in_status) {
  LOG(INFO) << "KioskAppManager::ConsumerKioskModeStatus = "
            << static_cast<int>(in_status);
  *out_status = in_status;
  std::move(runner_quit_task).Run();
}

// Helper function for WaitForNetworkTimeOut.
void OnNetworkWaitTimedOut(base::OnceClosure runner_quit_task) {
  std::move(runner_quit_task).Run();
}

}  // namespace

const char kTestEnterpriseKioskApp[] = "gcpjojfkologpegommokeppihdbcnahn";
const char kTestEnterpriseAccountId[] = "enterprise-kiosk-app@localhost";

const test::UIPath kConfigNetwork = {"app-launch-splash", "configNetwork"};
const char kSizeChangedMessage[] = "size_changed";

bool ShouldBrowserBeClosedByAppSessionBrowserHander(
    AppSessionAsh* app_session) {
  base::RunLoop waiter;
  bool result = false;
  app_session->SetOnHandleBrowserCallbackForTesting(
      base::BindLambdaForTesting([&waiter, &result](bool is_closing) {
        result = is_closing;
        waiter.Quit();
      }));
  waiter.Run();
  return result;
}

Browser* OpenA11ySettingsBrowser(AppSessionAsh* app_session) {
  auto* settings_manager = chrome::SettingsWindowManager::GetInstance();
  Profile* profile = ProfileManager::GetPrimaryUserProfile();

  settings_manager->ShowOSSettings(
      profile, chromeos::settings::mojom::kManageAccessibilitySubpagePath);

  EXPECT_FALSE(ShouldBrowserBeClosedByAppSessionBrowserHander(app_session));

  Browser* settings_browser = app_session->GetSettingsBrowserForTesting();
  return settings_browser;
}

KioskBaseTest::KioskBaseTest()
    : settings_helper_(false), fake_cws_(new FakeCWS) {
  set_exit_when_last_browser_closes(false);

  // This test does not operate any real App, so App data does not exist.
  // Depending on timing, the asynchronous check for app data may or may not
  // complete before test checks pass. And if the check does complete, it will
  // mark app status KioskAppData::Status::kError, and exclude it from the
  // list of populated apps.
  //
  // Then, any Update UI event (asynchronous) (like
  // LoginDisplayHostCommon::OnStartSignInScreenCommon() will invoke
  // SendKioskApps() and destroy test configuration.
  //
  // We default to ignore test data, as most of test cases use app ids only,
  // So individual checks should revert it to default when needed.
  //
  // TODO(https://crbug.com/937244): Remove this.
  KioskAppData::SetIgnoreKioskAppDataLoadFailuresForTesting(true);
}

KioskBaseTest::~KioskBaseTest() = default;

// static
KioskAppManager::ConsumerKioskAutoLaunchStatus
KioskBaseTest::GetConsumerKioskModeStatus() {
  KioskAppManager::ConsumerKioskAutoLaunchStatus status =
      static_cast<KioskAppManager::ConsumerKioskAutoLaunchStatus>(-1);
  base::RunLoop loop;
  KioskAppManager::Get()->GetConsumerKioskAutoLaunchStatus(base::BindOnce(
      &ConsumerKioskAutoLaunchStatusCheck, &status, loop.QuitClosure()));
  loop.Run();
  EXPECT_NE(status,
            static_cast<KioskAppManager::ConsumerKioskAutoLaunchStatus>(-1));
  return status;
}

// static
int KioskBaseTest::WaitForWidthChange(content::DOMMessageQueue* message_queue,
                                      int current_width) {
  std::string message;
  while (message_queue->WaitForMessage(&message)) {
    absl::optional<base::Value> message_value = base::JSONReader::Read(message);

    if (!message_value.has_value() || !message_value.value().is_dict())
      continue;

    const std::string* name = message_value.value().FindStringKey("name");
    if (!name || *name != kSizeChangedMessage)
      continue;

    const base::Value* data = message_value->FindKey("data");

    if (!data || !data->is_int())
      continue;

    const int new_width = data->GetInt();
    if (new_width == current_width)
      continue;

    return new_width;
  }

  ADD_FAILURE() << "Message wait failed " << kSizeChangedMessage;
  return current_width;
}

// static
KioskLaunchController* KioskBaseTest::GetKioskLaunchController() {
  return LoginDisplayHost::default_host()->GetKioskLaunchController();
}

void KioskBaseTest::SetUp() {
  test_app_id_ = KioskAppsMixin::kKioskAppId;
  set_test_app_version("1.0.0");
  set_test_crx_file(test_app_id() + ".crx");
  needs_background_networking_ = true;
  ProfileHelper::SetAlwaysReturnPrimaryUserForTesting(true);
  skip_splash_wait_override_ =
      KioskLaunchController::SkipSplashScreenWaitForTesting();
  network_wait_override_ = KioskLaunchController::SetNetworkWaitForTesting(
      base::Seconds(kTestNetworkTimeoutSeconds));
  OobeBaseTest::SetUp();
}

void KioskBaseTest::TearDown() {
  ProfileHelper::SetAlwaysReturnPrimaryUserForTesting(false);
  OobeBaseTest::TearDown();
}

void KioskBaseTest::SetUpOnMainThread() {
  OobeBaseTest::SetUpOnMainThread();
  // Needed to avoid showing Gaia screen instead of owner signin for
  // consumer network down test cases.
  StartupUtils::MarkDeviceRegistered(base::OnceClosure());
  settings_helper_.ReplaceDeviceSettingsProviderWithStub();
  owner_settings_service_ = settings_helper_.CreateOwnerSettingsService(
      ProfileManager::GetPrimaryUserProfile());

  // Set up local cache for app update check.
  extensions::browsertest_util::CreateAndInitializeLocalCache();
}

void KioskBaseTest::TearDownOnMainThread() {
  owner_settings_service_.reset();
  settings_helper_.RestoreRealDeviceSettingsProvider();
  KioskLaunchController::SetNetworkTimeoutCallbackForTesting(nullptr);

  OobeBaseTest::TearDownOnMainThread();

  // Clean up while main thread still runs.
  // See http://crbug.com/176659.
  KioskAppManager::Get()->CleanUp();
}

void KioskBaseTest::SetUpCommandLine(base::CommandLine* command_line) {
  OobeBaseTest::SetUpCommandLine(command_line);
  fake_cws_->Init(embedded_test_server());

  if (use_consumer_kiosk_mode_)
    command_line->AppendSwitch(switches::kEnableConsumerKiosk);
}

bool KioskBaseTest::LaunchApp(const std::string& app_id) {
  return LoginScreenTestApi::LaunchApp(app_id);
}

void KioskBaseTest::ReloadKioskApps() {
  SetupTestAppUpdateCheck();

  // Remove then add to ensure UI update.
  KioskAppManager::Get()->RemoveApp(test_app_id_,
                                    owner_settings_service_.get());
  KioskAppManager::Get()->AddApp(test_app_id_, owner_settings_service_.get());
}

void KioskBaseTest::SetupTestAppUpdateCheck() {
  if (test_app_version().empty())
    return;

  fake_cws_->SetUpdateCrx(test_app_id(), test_crx_file(), test_app_version());
}

void KioskBaseTest::ReloadAutolaunchKioskApps() {
  SetupTestAppUpdateCheck();

  KioskAppManager::Get()->AddApp(test_app_id_, owner_settings_service_.get());
  KioskAppManager::Get()->SetAutoLaunchApp(test_app_id_,
                                           owner_settings_service_.get());
}

void KioskBaseTest::PrepareAppLaunch() {
  // Wait for the Kiosk App configuration to reload.
  int ui_update_count = LoginScreenTestApi::GetUiUpdateCount();
  ReloadKioskApps();
  LoginScreenTestApi::WaitForUiUpdate(ui_update_count);
}

void KioskBaseTest::StartAppLaunchFromLoginScreen(
    NetworkPortalDetector::CaptivePortalStatus network_status) {
  PrepareAppLaunch();

  network_portal_detector_.SimulateDefaultNetworkState(network_status);
  EXPECT_TRUE(LaunchApp(test_app_id()));
}

void KioskBaseTest::StartExistingAppLaunchFromLoginScreen(
    NetworkPortalDetector::CaptivePortalStatus network_status) {
  SetupTestAppUpdateCheck();

  network_portal_detector_.SimulateDefaultNetworkState(network_status);
  EXPECT_TRUE(LaunchApp(test_app_id()));
}

const extensions::Extension* KioskBaseTest::GetInstalledApp() {
  Profile* app_profile = ProfileManager::GetPrimaryUserProfile();
  return extensions::ExtensionRegistry::Get(app_profile)
      ->GetInstalledExtension(test_app_id_);
}

const base::Version& KioskBaseTest::GetInstalledAppVersion() {
  return GetInstalledApp()->version();
}

ManifestLocation KioskBaseTest::GetInstalledAppLocation() {
  return GetInstalledApp()->location();
}

void KioskBaseTest::WaitForAppLaunchWithOptions(bool check_launch_data,
                                                bool terminate_app,
                                                bool keep_app_open) {
  ExtensionTestMessageListener launch_data_check_listener(
      "launchData.isKioskSession = true");

  // Wait for the Kiosk App to launch.
  KioskSessionInitializedWaiter().Wait();

  // Default profile switches to app profile after app is launched.
  Profile* app_profile = ProfileManager::GetPrimaryUserProfile();
  ASSERT_TRUE(app_profile);

  // Check ChromeOS preference is initialized.
  EXPECT_TRUE(static_cast<ProfileImpl*>(app_profile)->chromeos_preferences_);

  // Check installer status.
  EXPECT_EQ(KioskAppLaunchError::Error::kNone, KioskAppLaunchError::Get());

  // Check if the kiosk webapp is really installed for the default profile.
  const extensions::Extension* app =
      extensions::ExtensionRegistry::Get(app_profile)
          ->GetInstalledExtension(test_app_id_);
  EXPECT_TRUE(app);

  // App should appear with its window.
  extensions::AppWindowRegistry* app_window_registry =
      extensions::AppWindowRegistry::Get(app_profile);
  extensions::AppWindow* window =
      apps::AppWindowWaiter(app_window_registry, test_app_id_).Wait();
  EXPECT_TRUE(window);

  OobeWindowVisibilityWaiter(false /*target_visibility*/).Wait();

  // Terminate the app.
  if (terminate_app)
    window->GetBaseWindow()->Close();

  // Wait until the app terminates if it is still running.
  if (!keep_app_open &&
      !app_window_registry->GetAppWindowsForApp(test_app_id_).empty())
    RunUntilBrowserProcessQuits();

  // Check that the app had been informed that it is running in a kiosk
  // session.
  if (check_launch_data) {
    EXPECT_TRUE(launch_data_check_listener.WaitUntilSatisfied());
  }
}

void KioskBaseTest::WaitForAppLaunchSuccess() {
  WaitForAppLaunchWithOptions(true /* check_launch_data */,
                              true /* terminate_app */);
}

void KioskBaseTest::WaitForAppLaunchNetworkTimeout() {
  if (GetKioskLaunchController()->network_wait_timedout())
    return;

  base::RunLoop loop;
  base::OnceClosure callback =
      base::BindOnce(&OnNetworkWaitTimedOut, loop.QuitClosure());
  KioskLaunchController::SetNetworkTimeoutCallbackForTesting(&callback);
  loop.Run();
  ASSERT_TRUE(GetKioskLaunchController()->network_wait_timedout());
}

void KioskBaseTest::RunAppLaunchNetworkDownTest() {
  // Mock network could be configured with owner's password.
  ScopedCanConfigureNetwork can_configure_network(true, true);

  // Start app launch and wait for network connectivity timeout.
  StartAppLaunchFromLoginScreen(
      NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_OFFLINE);
  OobeScreenWaiter splash_waiter(AppLaunchSplashScreenView::kScreenId);
  splash_waiter.Wait();
  WaitForAppLaunchNetworkTimeout();

  // Configure network link should be visible.
  test::OobeJS().ExpectVisiblePath(kConfigNetwork);

  // Configure network should bring up lock screen for owner.
  static_cast<AppLaunchSplashScreenView::Delegate*>(GetKioskLaunchController())
      ->OnConfigureNetwork();
  EXPECT_FALSE(LoginScreenTestApi::IsOobeDialogVisible());
  // There should be only one owner pod on this screen.
  EXPECT_EQ(LoginScreenTestApi::GetUsersCount(), 1);

  // A network error screen should be shown after authenticating.
  OobeScreenWaiter error_screen_waiter(ErrorScreenView::kScreenId);
  LoginScreenTestApi::SubmitPassword(test_owner_account_id_, "password",
                                     /*check_if_submittable=*/true);
  error_screen_waiter.Wait();
  EXPECT_TRUE(LoginScreenTestApi::IsOobeDialogVisible());

  ASSERT_TRUE(GetKioskLaunchController()->showing_network_dialog());

  SimulateNetworkOnline();
  WaitForAppLaunchSuccess();
}

void KioskBaseTest::SimulateNetworkOnline() {
  network_portal_detector_.SimulateDefaultNetworkState(
      NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_ONLINE);
}

void KioskBaseTest::SimulateNetworkOffline() {
  network_portal_detector_.SimulateDefaultNetworkState(
      NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_OFFLINE);
}

void KioskBaseTest::BlockAppLaunch(bool block) {
  if (block) {
    block_app_launch_override_ =
        KioskLaunchController::BlockAppLaunchForTesting();
  } else {
    block_app_launch_override_.reset();
  }
}

}  // namespace ash
