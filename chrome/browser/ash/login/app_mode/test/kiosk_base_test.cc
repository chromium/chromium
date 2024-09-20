// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/app_mode/test/kiosk_base_test.h"

#include <optional>
#include <string>

#include "apps/test/app_window_waiter.h"
#include "ash/public/cpp/login_screen_test_api.h"
#include "ash/webui/settings/public/constants/routes.mojom-forward.h"
#include "base/check_deref.h"
#include "base/command_line.h"
#include "base/functional/callback_forward.h"
#include "base/json/json_reader.h"
#include "base/notreached.h"
#include "base/test/test_future.h"
#include "base/values.h"
#include "base/version.h"
#include "chrome/browser/ash/app_mode/consumer_kiosk_test_helper.h"
#include "chrome/browser/ash/app_mode/fake_cws.h"
#include "chrome/browser/ash/app_mode/kiosk_app.h"
#include "chrome/browser/ash/app_mode/kiosk_app_launch_error.h"
#include "chrome/browser/ash/app_mode/kiosk_app_types.h"
#include "chrome/browser/ash/app_mode/kiosk_chrome_app_manager.h"
#include "chrome/browser/ash/app_mode/kiosk_controller.h"
#include "chrome/browser/ash/app_mode/kiosk_test_helper.h"
#include "chrome/browser/ash/login/app_mode/network_ui_controller.h"
#include "chrome/browser/ash/login/app_mode/test/kiosk_apps_mixin.h"
#include "chrome/browser/ash/login/app_mode/test/kiosk_test_helpers.h"
#include "chrome/browser/ash/login/startup_utils.h"
#include "chrome/browser/ash/login/test/js_checker.h"
#include "chrome/browser/ash/login/test/oobe_base_test.h"
#include "chrome/browser/ash/login/test/oobe_window_visibility_waiter.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/extensions/browsertest_util.h"
#include "chrome/browser/profiles/profile_impl.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/ash/login/login_display_host.h"
#include "chrome/browser/ui/settings_window_manager_chromeos.h"
#include "content/public/test/browser_test_utils.h"
#include "extensions/browser/app_window/app_window.h"
#include "extensions/browser/app_window/app_window_registry.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/extension.h"
#include "extensions/test/extension_test_message_listener.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

const char kTestEnterpriseKioskAppId[] = "gcpjojfkologpegommokeppihdbcnahn";
const char kTestEnterpriseAccountId[] = "enterprise-kiosk-app@localhost";

const test::UIPath kConfigNetwork = {"app-launch-splash", "configNetwork"};
const char kSizeChangedMessage[] = "size_changed";

bool DidSessionCloseNewWindow(KioskSystemSession* session) {
  base::test::TestFuture<bool> future;
  session->SetOnHandleBrowserCallbackForTesting(future.GetRepeatingCallback());
  return future.Take();
}

Browser* OpenA11ySettingsBrowser(KioskSystemSession* session) {
  auto* settings_manager = chrome::SettingsWindowManager::GetInstance();
  Profile* profile = ProfileManager::GetPrimaryUserProfile();

  settings_manager->ShowOSSettings(
      profile, chromeos::settings::mojom::kManageAccessibilitySubpagePath);

  EXPECT_FALSE(DidSessionCloseNewWindow(session));

  Browser* settings_browser = session->GetSettingsBrowserForTesting();
  return settings_browser;
}

KioskBaseTest::KioskBaseTest()
    : settings_helper_(false), fake_cws_(new FakeCWS) {
  set_exit_when_last_browser_closes(false);
}

KioskBaseTest::~KioskBaseTest() = default;

// static
int KioskBaseTest::WaitForWidthChange(content::DOMMessageQueue* message_queue,
                                      int current_width) {
  std::string message;
  while (message_queue->WaitForMessage(&message)) {
    std::optional<base::Value> message_value = base::JSONReader::Read(message);
    if (!message_value || !message_value->is_dict()) {
      continue;
    }

    const base::Value::Dict& message_dict = message_value->GetDict();
    const std::string* name = message_dict.FindString("name");
    if (!name || *name != kSizeChangedMessage) {
      continue;
    }

    const std::optional<int> data = message_dict.FindInt("data");
    if (!data || data == current_width) {
      continue;
    }

    return data.value();
  }

  ADD_FAILURE() << "Message wait failed " << kSizeChangedMessage;
  return current_width;
}

void KioskBaseTest::SetUp() {
  SetTestApp(KioskAppsMixin::kTestChromeAppId);
  needs_background_networking_ = true;
  ProfileHelper::SetAlwaysReturnPrimaryUserForTesting(true);

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

  OobeBaseTest::TearDownOnMainThread();
}

void KioskBaseTest::SetUpCommandLine(base::CommandLine* command_line) {
  OobeBaseTest::SetUpCommandLine(command_line);
  fake_cws_->Init(embedded_test_server());
}

bool KioskBaseTest::LaunchApp(const std::string& app_id) {
  return LoginScreenTestApi::LaunchApp(app_id);
}

void KioskBaseTest::ReloadKioskApps() {
  SetupTestAppUpdateCheck();

  // Remove then add to ensure UI update.
  RemoveConsumerKioskChromeAppForTesting(
      CHECK_DEREF(KioskChromeAppManager::Get()),
      CHECK_DEREF(owner_settings_service_.get()), test_app_id());
  AddConsumerKioskChromeAppForTesting(
      CHECK_DEREF(owner_settings_service_.get()), test_app_id());
}

void KioskBaseTest::SetupTestAppUpdateCheck() {
  if (test_app_version().empty()) {
    return;
  }

  fake_cws_->SetUpdateCrx(test_app_id(), test_crx_file(), test_app_version());
}

void KioskBaseTest::ReloadAutolaunchKioskApps() {
  SetupTestAppUpdateCheck();

  AddConsumerKioskChromeAppForTesting(
      CHECK_DEREF(owner_settings_service_.get()), test_app_id());
  SetConsumerKioskAutoLaunchChromeAppForTesting(
      CHECK_DEREF(KioskChromeAppManager::Get()),
      CHECK_DEREF(owner_settings_service_.get()), test_app_id());
}

void KioskBaseTest::PrepareAppLaunch() {
  // Wait for the Kiosk App configuration to reload.
  int ui_update_count = LoginScreenTestApi::GetUiUpdateCount();
  ReloadKioskApps();
  LoginScreenTestApi::WaitForUiUpdate(ui_update_count);
}

void KioskBaseTest::StartAppLaunchFromLoginScreen(
    NetworkStatus network_status) {
  PrepareAppLaunch();

  network_portal_detector_.SimulateDefaultNetworkState(network_status);
  EXPECT_TRUE(LaunchApp(test_app_id()));
}

void KioskBaseTest::StartExistingAppLaunchFromLoginScreen(
    NetworkStatus network_status) {
  SetupTestAppUpdateCheck();

  network_portal_detector_.SimulateDefaultNetworkState(network_status);
  EXPECT_TRUE(LaunchApp(test_app_id()));
}

const extensions::Extension* KioskBaseTest::GetInstalledApp() {
  Profile* app_profile = ProfileManager::GetPrimaryUserProfile();
  return extensions::ExtensionRegistry::Get(app_profile)
      ->GetInstalledExtension(test_app_id());
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
          ->GetInstalledExtension(test_app_id());
  EXPECT_TRUE(app);

  // App should appear with its window.
  extensions::AppWindowRegistry* app_window_registry =
      extensions::AppWindowRegistry::Get(app_profile);
  extensions::AppWindow* window =
      apps::AppWindowWaiter(app_window_registry, test_app_id()).Wait();
  EXPECT_TRUE(window);

  OobeWindowVisibilityWaiter(false /*target_visibility*/).Wait();

  // Terminate the app.
  if (terminate_app) {
    window->GetBaseWindow()->Close();
  }

  // Wait until the app terminates if it is still running.
  if (!keep_app_open &&
      !app_window_registry->GetAppWindowsForApp(test_app_id()).empty()) {
    RunUntilBrowserProcessQuits();
  }

  // Check that the app had been informed that it is running in a kiosk
  // session.
  if (check_launch_data) {
    EXPECT_TRUE(launch_data_check_listener.WaitUntilSatisfied());
  }
}

void KioskBaseTest::WaitForAppLaunchSuccess() {
  WaitForAppLaunchWithOptions(/*check_launch_data=*/true,
                              /*terminate_app=*/true);
}

void KioskBaseTest::SimulateNetworkOnline() {
  network_portal_detector_.SimulateDefaultNetworkState(NetworkStatus::kOnline);
}

void KioskBaseTest::SimulateNetworkOffline() {
  network_portal_detector_.SimulateDefaultNetworkState(NetworkStatus::kOffline);
}

void KioskBaseTest::BlockAppLaunch(bool block) {
  if (block) {
    block_app_launch_override_ = KioskTestHelper::BlockAppLaunch();
  } else {
    block_app_launch_override_.reset();
  }
}

void KioskBaseTest::SetTestApp(const std::string& app_id,
                               const std::string& version,
                               const std::string& crx_file) {
  test_app_id_ = app_id;
  test_crx_file_ = (crx_file == "") ? app_id + ".crx" : crx_file;
  test_app_version_ = version;
}

KioskApp KioskBaseTest::test_kiosk_app() const {
  for (const KioskApp& app : KioskController::Get().GetApps()) {
    if (app.id().type == KioskAppType::kChromeApp &&
        app.id().app_id.value() == test_app_id()) {
      return app;
    }
  }
  NOTREACHED() << "App not in KioskController: " << test_app_id();
}

}  // namespace ash
