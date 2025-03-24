// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cstddef>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "apps/test/app_window_waiter.h"
#include "base/check_deref.h"
#include "base/test/test_future.h"
#include "chrome/browser/ash/app_mode/test/fake_cws_chrome_apps.h"
#include "chrome/browser/ash/app_mode/test/kiosk_mixin.h"
#include "chrome/browser/ash/app_mode/test/kiosk_test_utils.h"
#include "chrome/browser/ash/login/test/scoped_policy_update.h"
#include "chrome/browser/device_identity/device_oauth2_token_service.h"
#include "chrome/browser/device_identity/device_oauth2_token_service_factory.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/test/base/fake_gaia_mixin.h"
#include "chrome/test/base/mixin_based_in_process_browser_test.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "extensions/browser/app_window/app_window.h"
#include "extensions/browser/app_window/app_window_registry.h"
#include "google_apis/gaia/fake_gaia.h"
#include "google_apis/gaia/gaia_constants.h"
#include "google_apis/gaia/gaia_urls.h"
#include "net/dns/mock_host_resolver.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

using kiosk::test::CurrentProfile;
using kiosk::test::EnterpriseKioskAppV1;
using kiosk::test::TheKioskChromeApp;
using kiosk::test::WaitKioskLaunched;

namespace {

constexpr std::string_view kFakeServiceAccount =
    "fake_service_account@system.gserviceaccount.com";

constexpr std::string_view kFakeAccessToken = "fake-access-token";

void SetServiceAccountInPolicy(
    std::unique_ptr<ScopedDevicePolicyUpdate> update) {
  auto* identity = update->policy_data()->mutable_service_account_identity();
  *identity = std::string(kFakeServiceAccount);
}

bool ConfigureFakeOauthToken(FakeGaia& fake_gaia,
                             std::string_view access_token) {
  constexpr std::string_view kRefreshToken = "fake-refresh-token";
  constexpr std::string_view kUserinfoToken = "fake-userinfo-token";
  constexpr std::string_view kLoginToken = "fake-login-token";
  constexpr std::string_view kClientId = "fake-client-id";
  constexpr std::string_view kOAuthScope =
      "https://www.googleapis.com/auth/userinfo.profile";

  GaiaUrls* gaia_urls = GaiaUrls::GetInstance();

  // This token satisfies the userinfo.email request from
  // DeviceOAuth2TokenService used in token validation.
  FakeGaia::AccessTokenInfo userinfo_token_info;
  userinfo_token_info.token = kUserinfoToken;
  userinfo_token_info.scopes.insert(
      "https://www.googleapis.com/auth/userinfo.email");
  userinfo_token_info.audience = gaia_urls->oauth2_chrome_client_id();
  userinfo_token_info.email = kFakeServiceAccount;
  fake_gaia.IssueOAuthToken(std::string(kRefreshToken), userinfo_token_info);

  // The any-api access token for accessing the token minting endpoint.
  FakeGaia::AccessTokenInfo login_token_info;
  login_token_info.token = kLoginToken;
  login_token_info.scopes.insert(GaiaConstants::kAnyApiOAuth2Scope);
  login_token_info.audience = gaia_urls->oauth2_chrome_client_id();
  fake_gaia.IssueOAuthToken(std::string(kRefreshToken), login_token_info);

  // This is the access token requested by the app via the identity API.
  FakeGaia::AccessTokenInfo access_token_info;
  access_token_info.token = access_token;
  access_token_info.scopes.insert(std::string(kOAuthScope));
  access_token_info.audience = kClientId;
  access_token_info.email = kFakeServiceAccount;
  fake_gaia.IssueOAuthToken(std::string(kLoginToken), access_token_info);

  base::test::TestFuture<bool> success_future;
  auto& token_service = CHECK_DEREF(DeviceOAuth2TokenServiceFactory::Get());
  token_service.SetAndSaveRefreshToken(std::string(kRefreshToken),
                                       success_future.GetCallback());
  return success_future.Get();
}

content::WebContents& ChromeAppWebContents(Profile& profile,
                                           const std::string& app_id) {
  auto& registry = CHECK_DEREF(extensions::AppWindowRegistry::Get(&profile));
  auto& window = CHECK_DEREF(apps::AppWindowWaiter(&registry, app_id).Wait());
  return CHECK_DEREF(window.web_contents());
}

}  // namespace

// Verifies the `chrome.identity` API works in Kiosk.
class KioskIdentityTest : public MixinBasedInProcessBrowserTest {
 public:
  KioskIdentityTest() = default;
  KioskIdentityTest(const KioskIdentityTest&) = delete;
  KioskIdentityTest& operator=(const KioskIdentityTest&) = delete;
  ~KioskIdentityTest() override = default;

  void SetUpInProcessBrowserTestFixture() override {
    MixinBasedInProcessBrowserTest::SetUpInProcessBrowserTestFixture();
    SetServiceAccountInPolicy(
        kiosk_.device_state_mixin().RequestDevicePolicyUpdate());
  }

  void SetUpOnMainThread() override {
    MixinBasedInProcessBrowserTest::SetUpOnMainThread();

    // Needed so requests reach the fake GAIA server.
    host_resolver()->AddRule("*", "127.0.0.1");

    ASSERT_TRUE(WaitKioskLaunched());
  }

  FakeGaia& fake_gaia() { return CHECK_DEREF(fake_gaia_.fake_gaia()); }

 private:
  FakeGaiaMixin fake_gaia_{&mixin_host_};

  KioskMixin kiosk_{
      &mixin_host_,
      /*cached_configuration=*/KioskMixin::Config{
          /*name=*/{},
          KioskMixin::AutoLaunchAccount{EnterpriseKioskAppV1().account_id},
          {EnterpriseKioskAppV1()}}};
};

IN_PROC_BROWSER_TEST_F(KioskIdentityTest, GetAuthTokenWorks) {
  ASSERT_TRUE(ConfigureFakeOauthToken(fake_gaia(), kFakeAccessToken));

  auto& web_contents = ChromeAppWebContents(
      CurrentProfile(), TheKioskChromeApp().id().app_id.value());
  EXPECT_TRUE(content::WaitForLoadStop(&web_contents));

  EXPECT_EQ(kFakeAccessToken, content::EvalJs(&web_contents, R"(
                  new Promise(
                    (resolve) =>
                      chrome.identity.getAuthToken(
                        { interactive: false },
                        (result) => resolve(result || "no token"))
                  );
                )"));
}

}  // namespace ash
