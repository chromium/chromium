// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <vector>

#include "apps/test/app_window_waiter.h"
#include "ash/public/cpp/login_accelerators.h"
#include "base/files/file_path.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "chrome/browser/ash/app_mode/fake_cws.h"
#include "chrome/browser/ash/app_mode/kiosk_app_launch_error.h"
#include "chrome/browser/ash/app_mode/kiosk_app_manager.h"
#include "chrome/browser/ash/login/app_mode/test/kiosk_base_test.h"
#include "chrome/browser/ash/login/app_mode/test/kiosk_test_helpers.h"
#include "chrome/browser/ash/login/app_mode/test/test_app_data_load_waiter.h"
#include "chrome/browser/ash/login/oobe_screen.h"
#include "chrome/browser/ash/login/test/device_state_mixin.h"
#include "chrome/browser/ash/login/test/oobe_screen_waiter.h"
#include "chrome/browser/ash/login/ui/login_display_host.h"
#include "chrome/browser/ash/policy/core/device_local_account.h"
#include "chrome/browser/device_identity/device_oauth2_token_service.h"
#include "chrome/browser/device_identity/device_oauth2_token_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/ui/webui/ash/login/error_screen_handler.h"
#include "chrome/common/chrome_paths.h"
#include "chromeos/ash/components/dbus/userdataauth/fake_userdataauth_client.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "extensions/browser/app_window/app_window.h"
#include "extensions/browser/app_window/app_window_registry.h"
#include "extensions/common/mojom/manifest.mojom-shared.h"
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

}  // namespace

// Kiosk tests with fake enterprise enroll setup.
class KioskEnterpriseTest : public KioskBaseTest {
 public:
  KioskEnterpriseTest(const KioskEnterpriseTest&) = delete;
  KioskEnterpriseTest& operator=(const KioskEnterpriseTest&) = delete;

 protected:
  KioskEnterpriseTest() { set_use_consumer_kiosk_mode(false); }

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
    accounts.emplace_back(policy::DeviceLocalAccount::TYPE_KIOSK_APP,
                          policy::DeviceLocalAccount::EphemeralMode::kUnset,
                          account_id, app_id, update_url);
    policy::SetDeviceLocalAccounts(owner_settings_service_.get(), accounts);
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
  set_test_app_id(kTestEnterpriseKioskApp);
  set_test_app_version("1.0.0");
  set_test_crx_file(test_app_id() + ".crx");
  SetupTestAppUpdateCheck();

  // Configure kTestEnterpriseKioskApp in device policy.
  ConfigureKioskAppInPolicy(kTestEnterpriseAccountId, kTestEnterpriseKioskApp,
                            "");

  PrepareAppLaunch();
  EXPECT_TRUE(LaunchApp(kTestEnterpriseKioskApp));

  KioskSessionInitializedWaiter().Wait();

  // Check installer status.
  EXPECT_EQ(KioskAppLaunchError::Error::kNone, KioskAppLaunchError::Get());
  EXPECT_EQ(ManifestLocation::kExternalPolicy, GetInstalledAppLocation());

  // Wait for the window to appear.
  extensions::AppWindow* window =
      apps::AppWindowWaiter(extensions::AppWindowRegistry::Get(
                                ProfileManager::GetPrimaryUserProfile()),
                            kTestEnterpriseKioskApp)
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

IN_PROC_BROWSER_TEST_F(KioskEnterpriseTest, PrivateStore) {
  set_test_app_id(kTestEnterpriseKioskApp);

  const char kPrivateStoreUpdate[] = "/private_store_update";
  net::EmbeddedTestServer private_server;

  // `private_server` serves crx from test data dir.
  base::FilePath test_data_dir;
  base::PathService::Get(chrome::DIR_TEST_DATA, &test_data_dir);
  private_server.ServeFilesFromDirectory(test_data_dir);
  ASSERT_TRUE(private_server.InitializeAndListen());

  FakeCWS private_store;
  private_store.InitAsPrivateStore(&private_server, kPrivateStoreUpdate);
  private_store.SetUpdateCrx(kTestEnterpriseKioskApp,
                             std::string(kTestEnterpriseKioskApp) + ".crx",
                             "1.0.0");

  private_server.StartAcceptingConnections();

  // Configure kTestEnterpriseKioskApp in device policy.
  ConfigureKioskAppInPolicy(kTestEnterpriseAccountId, kTestEnterpriseKioskApp,
                            private_server.GetURL(kPrivateStoreUpdate).spec());

  // Meta should be able to be extracted from crx before launching.
  KioskAppManager* manager = KioskAppManager::Get();
  TestAppDataLoadWaiter waiter(manager, kTestEnterpriseKioskApp, std::string());
  waiter.WaitForAppData();

  PrepareAppLaunch();
  EXPECT_TRUE(LaunchApp(kTestEnterpriseKioskApp));
  WaitForAppLaunchWithOptions(false /* check_launch_data */,
                              true /* terminate_app */);

  // Private store should serve crx and CWS should not.
  DCHECK_GT(private_store.GetUpdateCheckCountAndReset(), 0);
  DCHECK_EQ(0, fake_cws()->GetUpdateCheckCountAndReset());
  EXPECT_EQ(ManifestLocation::kExternalPolicy, GetInstalledAppLocation());
}
IN_PROC_BROWSER_TEST_F(KioskEnterpriseTest,
                       HittingNetworkAcceleratorShouldShowNetworkScreen) {
  ScopedCanConfigureNetwork can_configure_network(true);

  // Block app loading until the welcome screen is shown.
  BlockAppLaunch(true);

  // Start app launch and wait for network connectivity timeout.
  StartAppLaunchFromLoginScreen(
      NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_ONLINE);
  WaitForOobeScreen(AppLaunchSplashScreenView::kScreenId);

  PressConfigureNetworkAccelerator();

  // `ErrorScreenView` is the network screen
  WaitForOobeScreen(ErrorScreenView::kScreenId);
  ASSERT_TRUE(GetKioskLaunchController()->showing_network_dialog());

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
    LaunchingAppThatRequiresNetworkWhilstOnlineShouldShowNetworkScreen) {
  ScopedCanConfigureNetwork can_configure_network(true);

  // Start app launch with network portal state.
  StartAppLaunchFromLoginScreen(
      NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_PORTAL);

  WaitForOobeScreen(AppLaunchSplashScreenView::kScreenId);

  // Network error should show up automatically since this test does not
  // require owner auth to configure network.
  WaitForOobeScreen(ErrorScreenView::kScreenId);

  ASSERT_TRUE(GetKioskLaunchController()->showing_network_dialog());
  SimulateNetworkOnline();
  WaitForAppLaunchSuccess();
}

class KioskEnterpriseEphemeralTest
    : public KioskEnterpriseTest,
      public testing::WithParamInterface<std::tuple<
          /* ephemeral_users_enabled */ bool,
          /* kiosk_ephemeral_mode */ policy::DeviceLocalAccount::
              EphemeralMode>> {
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
    accounts.emplace_back(policy::DeviceLocalAccount::TYPE_KIOSK_APP,
                          ephemeral_mode, account_id, app_id, update_url);
    policy::SetDeviceLocalAccounts(owner_settings_service_.get(), accounts);
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
  set_test_app_id(kTestEnterpriseKioskApp);
  set_test_app_version("1.0.0");
  set_test_crx_file(test_app_id() + ".crx");
  SetupTestAppUpdateCheck();

  // Configure device policies.
  ConfigureEphemeralPolicies(kTestEnterpriseAccountId, kTestEnterpriseKioskApp,
                             "", GetKioskEphemeralMode(),
                             GetEphemeralUsersEnabled());

  EXPECT_TRUE(LaunchApp(kTestEnterpriseKioskApp));

  KioskSessionInitializedWaiter().Wait();

  // Check installer status.
  EXPECT_EQ(KioskAppLaunchError::Error::kNone, KioskAppLaunchError::Get());
  EXPECT_EQ(ManifestLocation::kExternalPolicy, GetInstalledAppLocation());

  EXPECT_EQ(
      GetExpectedEphemeralUser(),
      FakeUserDataAuthClient::TestApi::Get()->IsCurrentSessionEphemeral());
}

}  // namespace ash
