// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <string_view>
#include <tuple>
#include <vector>

#include "apps/test/app_window_waiter.h"
#include "ash/public/cpp/login_accelerators.h"
#include "base/files/file_path.h"
#include "base/location.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "chrome/browser/ash/app_mode/fake_cws.h"
#include "chrome/browser/ash/app_mode/kiosk_app_launch_error.h"
#include "chrome/browser/ash/app_mode/kiosk_chrome_app_manager.h"
#include "chrome/browser/ash/login/app_mode/network_ui_controller.h"
#include "chrome/browser/ash/login/app_mode/test/kiosk_base_test.h"
#include "chrome/browser/ash/login/app_mode/test/kiosk_test_helpers.h"
#include "chrome/browser/ash/login/app_mode/test/test_app_data_load_waiter.h"
#include "chrome/browser/ash/login/oobe_screen.h"
#include "chrome/browser/ash/login/screens/error_screen.h"
#include "chrome/browser/ash/login/test/device_state_mixin.h"
#include "chrome/browser/ash/login/test/embedded_test_server_setup_mixin.h"
#include "chrome/browser/ash/login/test/js_checker.h"
#include "chrome/browser/ash/login/test/oobe_screen_waiter.h"
#include "chrome/browser/ash/policy/core/device_local_account.h"
#include "chrome/browser/device_identity/device_oauth2_token_service.h"
#include "chrome/browser/device_identity/device_oauth2_token_service_factory.h"
#include "chrome/browser/lifetime/termination_notification.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/ui/ash/login/login_display_host.h"
#include "chrome/browser/ui/webui/ash/login/app_launch_splash_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/error_screen_handler.h"
#include "chrome/common/chrome_paths.h"
#include "chromeos/ash/components/dbus/userdataauth/fake_userdataauth_client.h"
#include "chromeos/ash/components/settings/cros_settings_names.h"
#include "components/policy/core/common/device_local_account_type.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "extensions/browser/app_window/app_window.h"
#include "extensions/browser/app_window/app_window_registry.h"
#include "google_apis/gaia/fake_gaia.h"
#include "google_apis/gaia/gaia_constants.h"
#include "google_apis/gaia/gaia_urls.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

namespace {

const char kTestEnterpriseServiceAccountEmail[] =
    "service_account@system.gserviceaccount.com";
const char kTestRefreshToken[] = "fake-refresh-token";
const char kTestUserinfoToken[] = "fake-userinfo-token";
const char kTestLoginToken[] = "fake-login-token";
const char kTestAccessToken[] = "fake-access-token";
const char kTestClientId[] = "fake-client-id";
const char kTestAppScope[] = "https://www.googleapis.com/auth/userinfo.profile";
const test::UIPath kErrorMessageContinueButton = {"error-message",
                                                  "continueButton"};

void PressConfigureNetworkAccelerator() {
  LoginDisplayHost::default_host()->HandleAccelerator(
      LoginAcceleratorAction::kAppLaunchNetworkConfig);
}

void WaitForOobeScreen(OobeScreenId screen) {
  OobeScreenWaiter(screen).Wait();
}

void WaitForNetworkScreen() {
  WaitForOobeScreen(ErrorScreenView::kScreenId);
}

}  // namespace

// Kiosk tests with fake enterprise enroll setup.
class KioskEnterpriseTest : public KioskBaseTest {
 public:
  KioskEnterpriseTest(const KioskEnterpriseTest&) = delete;
  KioskEnterpriseTest& operator=(const KioskEnterpriseTest&) = delete;

 protected:
  KioskEnterpriseTest() = default;

  // KioskBaseTest:
  void SetUpOnMainThread() override {
    KioskBaseTest::SetUpOnMainThread();

    // Configure OAuth authentication.
    GaiaUrls* gaia_urls = GaiaUrls::GetInstance();

    // This token satisfies the userinfo.email request from
    // DeviceOAuth2TokenService used in token validation.
    FakeGaia::AccessTokenInfo userinfo_token_info;
    userinfo_token_info.token = kTestUserinfoToken;
    userinfo_token_info.scopes.insert(
        "https://www.googleapis.com/auth/userinfo.email");
    userinfo_token_info.audience = gaia_urls->oauth2_chrome_client_id();
    userinfo_token_info.email = kTestEnterpriseServiceAccountEmail;
    fake_gaia_.fake_gaia()->IssueOAuthToken(kTestRefreshToken,
                                            userinfo_token_info);

    // The any-api access token for accessing the token minting endpoint.
    FakeGaia::AccessTokenInfo login_token_info;
    login_token_info.token = kTestLoginToken;
    login_token_info.scopes.insert(GaiaConstants::kAnyApiOAuth2Scope);
    login_token_info.audience = gaia_urls->oauth2_chrome_client_id();
    fake_gaia_.fake_gaia()->IssueOAuthToken(kTestRefreshToken,
                                            login_token_info);

    // This is the access token requested by the app via the identity API.
    FakeGaia::AccessTokenInfo access_token_info;
    access_token_info.token = kTestAccessToken;
    access_token_info.scopes.insert(kTestAppScope);
    access_token_info.audience = kTestClientId;
    access_token_info.email = kTestEnterpriseServiceAccountEmail;
    fake_gaia_.fake_gaia()->IssueOAuthToken(kTestLoginToken, access_token_info);

    DeviceOAuth2TokenService* token_service =
        DeviceOAuth2TokenServiceFactory::Get();
    token_service->SetAndSaveRefreshToken(
        kTestRefreshToken, DeviceOAuth2TokenService::StatusCallback());
    base::RunLoop().RunUntilIdle();
  }

  void ConfigureKioskAppInPolicy(const std::string& account_id,
                                 const std::string& app_id,
                                 const std::string& update_url) {
    std::vector<policy::DeviceLocalAccount> accounts;
    accounts.emplace_back(policy::DeviceLocalAccountType::kKioskApp,
                          policy::DeviceLocalAccount::EphemeralMode::kUnset,
                          account_id, app_id, update_url);
    policy::SetDeviceLocalAccountsForTesting(owner_settings_service_.get(),
                                             accounts);
    settings_helper_.SetString(kAccountsPrefDeviceLocalAccountAutoLoginId,
                               account_id);
    settings_helper_.SetString(kServiceAccountIdentity,
                               kTestEnterpriseServiceAccountEmail);
  }

 private:
  DeviceStateMixin device_state_{
      &mixin_host_, DeviceStateMixin::State::OOBE_COMPLETED_CLOUD_ENROLLED};
};

IN_PROC_BROWSER_TEST_F(KioskEnterpriseTest, EnterpriseKioskApp) {
  // Prepare Fake CWS to serve app crx.
  SetTestApp(kTestEnterpriseKioskAppId);
  SetupTestAppUpdateCheck();

  // Configure `kTestEnterpriseKioskAppId` in device policy.
  ConfigureKioskAppInPolicy(kTestEnterpriseAccountId, kTestEnterpriseKioskAppId,
                            /*update_url=*/"");

  PrepareAppLaunch();
  EXPECT_TRUE(LaunchApp(kTestEnterpriseKioskAppId));

  KioskSessionInitializedWaiter().Wait();

  // Check installer status.
  EXPECT_EQ(KioskAppLaunchError::Error::kNone, KioskAppLaunchError::Get());
  EXPECT_EQ(ManifestLocation::kExternalPolicy, GetInstalledAppLocation());

  // Wait for the window to appear.
  extensions::AppWindow* window =
      apps::AppWindowWaiter(extensions::AppWindowRegistry::Get(
                                ProfileManager::GetPrimaryUserProfile()),
                            kTestEnterpriseKioskAppId)
          .Wait();
  ASSERT_TRUE(window);
  EXPECT_TRUE(content::WaitForLoadStop(window->web_contents()));

  // Check whether the app can retrieve an OAuth2 access token.
  EXPECT_EQ(kTestAccessToken,
            content::EvalJs(window->web_contents(),
                            "new Promise(resolve => {"
                            "  chrome.identity.getAuthToken({ 'interactive': "
                            "    false }, resolve);"
                            "});"));

  // Verify that the session is not considered to be logged in with a GAIA
  // account.
  Profile* app_profile = ProfileManager::GetPrimaryUserProfile();
  ASSERT_TRUE(app_profile);
  EXPECT_FALSE(IdentityManagerFactory::GetForProfile(app_profile)
                   ->HasPrimaryAccount(signin::ConsentLevel::kSignin));

  // Terminate the app.
  window->GetBaseWindow()->Close();
  base::RunLoop().RunUntilIdle();
}

IN_PROC_BROWSER_TEST_F(KioskEnterpriseTest,
                       HittingNetworkAcceleratorShouldShowNetworkScreen) {
  auto auto_reset = NetworkUiController::SetCanConfigureNetworkForTesting(true);

  // Block app loading until the welcome screen is shown.
  BlockAppLaunch(true);

  // Start app launch and wait for network connectivity timeout.
  StartAppLaunchFromLoginScreen(NetworkStatus::kOnline);
  WaitForOobeScreen(AppLaunchSplashScreenView::kScreenId);

  PressConfigureNetworkAccelerator();

  WaitForNetworkScreen();

  // Continue button should be visible since we are online.
  EXPECT_TRUE(test::OobeJS().IsVisible(kErrorMessageContinueButton));

  // Let app launching resume.
  BlockAppLaunch(false);

  // Click on [Continue] button.
  test::OobeJS().TapOnPath(kErrorMessageContinueButton);

  WaitForAppLaunchSuccess();
}

IN_PROC_BROWSER_TEST_F(
    KioskEnterpriseTest,
    LaunchingAppThatRequiresNetworkWhilstOfflineShouldShowNetworkScreen) {
  auto auto_reset = NetworkUiController::SetCanConfigureNetworkForTesting(true);

  // Start app launch with network portal state.
  StartAppLaunchFromLoginScreen(NetworkStatus::kPortal);

  WaitForNetworkScreen();

  SimulateNetworkOnline();
  WaitForAppLaunchSuccess();
}

IN_PROC_BROWSER_TEST_F(KioskEnterpriseTest, LaunchAppUserCancel) {
  StartAppLaunchFromLoginScreen(NetworkStatus::kOnline);
  // Do not let the app be run to avoid race condition.
  BlockAppLaunch(true);

  WaitForOobeScreen(AppLaunchSplashScreenView::kScreenId);

  base::test::TestFuture<void> termination_future_;
  auto subscription = browser_shutdown::AddAppTerminatingCallback(
      termination_future_.GetCallback());
  settings_helper_.SetBoolean(
      kAccountsPrefDeviceLocalAccountAutoLoginBailoutEnabled, true);

  LoginDisplayHost::default_host()->HandleAccelerator(
      LoginAcceleratorAction::kAppLaunchBailout);
  EXPECT_TRUE(termination_future_.Wait());

  EXPECT_EQ(KioskAppLaunchError::Error::kUserCancel,
            KioskAppLaunchError::Get());
}

// Verifies that Kiosk can launch a self hosted Chrome app.
class SelfHostedKioskEnterpriseTest : public KioskEnterpriseTest {
 public:
  SelfHostedKioskEnterpriseTest(const SelfHostedKioskEnterpriseTest&) = delete;
  SelfHostedKioskEnterpriseTest& operator=(
      const SelfHostedKioskEnterpriseTest&) = delete;

  SelfHostedKioskEnterpriseTest() = default;

  ~SelfHostedKioskEnterpriseTest() override = default;

 protected:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    private_store_.InitAsPrivateStore(&test_server_, kPrivateStoreUpdate);
    KioskEnterpriseTest::SetUpCommandLine(command_line);
  }

  void SetUpOnMainThread() override {
    KioskEnterpriseTest::SetUpOnMainThread();
    private_store_.SetUpdateCrx(kTestEnterpriseKioskAppId,
                                std::string(kTestEnterpriseKioskAppId) + ".crx",
                                "1.0.0");
    SetTestApp(kTestEnterpriseKioskAppId);

    ConfigureKioskAppInPolicy(kTestEnterpriseAccountId,
                              kTestEnterpriseKioskAppId,
                              test_server_.GetURL(kPrivateStoreUpdate).spec());
  }

  FakeCWS private_store_;

 private:
  static constexpr std::string_view kPrivateStoreUpdate =
      "/private_store_update";

  net::EmbeddedTestServer test_server_;
  EmbeddedTestServerSetupMixin test_server_setup_mixin_{&mixin_host_,
                                                        &test_server_};
};

IN_PROC_BROWSER_TEST_F(SelfHostedKioskEnterpriseTest, SelfHostedChromeApp) {
  // Should be able to extract metadata from crx before launching.
  KioskChromeAppManager* manager = KioskChromeAppManager::Get();
  TestAppDataLoadWaiter waiter(manager, kTestEnterpriseKioskAppId,
                               std::string());
  waiter.WaitForAppData();

  PrepareAppLaunch();
  EXPECT_TRUE(LaunchApp(kTestEnterpriseKioskAppId));
  WaitForAppLaunchWithOptions(/*check_launch_data=*/false,
                              /*terminate_app=*/true);

  // Update checks should be made to the private store instead of CWS.
  EXPECT_GT(private_store_.GetUpdateCheckCountAndReset(), 0);
  EXPECT_EQ(ManifestLocation::kExternalPolicy, GetInstalledAppLocation());
}

class KioskEnterpriseEphemeralTest
    : public KioskEnterpriseTest,
      public testing::WithParamInterface<std::tuple<
          /*ephemeral_users_enabled=*/bool,
          /*kiosk_ephemeral_mode=*/policy::DeviceLocalAccount::EphemeralMode>> {
 public:
  KioskEnterpriseEphemeralTest(const KioskEnterpriseEphemeralTest&) = delete;
  KioskEnterpriseEphemeralTest& operator=(const KioskEnterpriseEphemeralTest&) =
      delete;

 protected:
  KioskEnterpriseEphemeralTest() = default;

  bool GetEphemeralUsersEnabled() const { return std::get<0>(GetParam()); }

  policy::DeviceLocalAccount::EphemeralMode GetKioskEphemeralMode() const {
    return std::get<1>(GetParam());
  }

  bool GetExpectedEphemeralUser() const {
    switch (GetKioskEphemeralMode()) {
      case policy::DeviceLocalAccount::EphemeralMode::kUnset:
      case policy::DeviceLocalAccount::EphemeralMode::kFollowDeviceWidePolicy:
        return GetEphemeralUsersEnabled();
      case policy::DeviceLocalAccount::EphemeralMode::kDisable:
        return false;
      case policy::DeviceLocalAccount::EphemeralMode::kEnable:
        return true;
    }
  }

  void ConfigureEphemeralPolicies(
      const std::string& account_id,
      const std::string& app_id,
      const std::string& update_url,
      policy::DeviceLocalAccount::EphemeralMode ephemeral_mode,
      bool ephemeral_users_enabled) {
    std::vector<policy::DeviceLocalAccount> accounts;
    accounts.emplace_back(policy::DeviceLocalAccountType::kKioskApp,
                          ephemeral_mode, account_id, app_id, update_url);
    policy::SetDeviceLocalAccountsForTesting(owner_settings_service_.get(),
                                             accounts);
    settings_helper_.SetBoolean(kAccountsPrefEphemeralUsersEnabled,
                                ephemeral_users_enabled);
  }
};

INSTANTIATE_TEST_SUITE_P(
    All,
    KioskEnterpriseEphemeralTest,
    testing::Combine(
        testing::Values(false, true),
        testing::Values(
            policy::DeviceLocalAccount::EphemeralMode::kUnset,
            policy::DeviceLocalAccount::EphemeralMode::kFollowDeviceWidePolicy,
            policy::DeviceLocalAccount::EphemeralMode::kDisable,
            policy::DeviceLocalAccount::EphemeralMode::kEnable)));

IN_PROC_BROWSER_TEST_P(KioskEnterpriseEphemeralTest,
                       EnterpriseKioskAppEphemeral) {
  // Prepare Fake CWS to serve app crx.
  SetTestApp(kTestEnterpriseKioskAppId);
  SetupTestAppUpdateCheck();

  // Configure device policies.
  ConfigureEphemeralPolicies(
      kTestEnterpriseAccountId, kTestEnterpriseKioskAppId, /*update_url=*/"",
      GetKioskEphemeralMode(), GetEphemeralUsersEnabled());

  EXPECT_TRUE(LaunchApp(kTestEnterpriseKioskAppId));

  KioskSessionInitializedWaiter().Wait();

  // Check installer status.
  EXPECT_EQ(KioskAppLaunchError::Error::kNone, KioskAppLaunchError::Get());
  EXPECT_EQ(ManifestLocation::kExternalPolicy, GetInstalledAppLocation());

  EXPECT_EQ(
      GetExpectedEphemeralUser(),
      FakeUserDataAuthClient::TestApi::Get()->IsCurrentSessionEphemeral());
}

}  // namespace ash
