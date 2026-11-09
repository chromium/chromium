// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/net/enterprise_proxy_tab_helper_platform_delegate_android.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/android/tab_android.h"
#include "chrome/browser/signin/android/signin_bridge.h"
#include "chrome/browser/signin/android/signin_bridge_factory.h"
#include "chrome/browser/signin/identity_test_environment_profile_adaptor.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_profile.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/web_contents.h"
#include "google_apis/gaia/core_account_id.h"
#include "google_apis/gaia/gaia_id.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace enterprise_net {
namespace {

class MockSigninBridge : public SigninBridge {
 public:
  MockSigninBridge() = default;
  ~MockSigninBridge() override = default;

  MOCK_METHOD(void,
              StartAddAccountFlow,
              (TabAndroid * tab,
               const std::string& prefilled_email,
               const GURL& continue_url,
               const std::string& extension_name),
              (override));

  MOCK_METHOD(void,
              OpenAccountPickerBottomSheetForWebSignin,
              (content::WebContents * web_contents,
               const GURL& continue_url,
               const std::optional<CoreAccountId>& account_id),
              (override));

  MOCK_METHOD(void,
              StartUpdateCredentialsFlow,
              (TabAndroid * tab,
               const GURL& continue_url,
               const CoreAccountId& account_to_reauth,
               const CoreAccountId& target_account_id),
              (override));
};

std::unique_ptr<KeyedService> BuildMockSigninBridgeForTesting(
    content::BrowserContext* context) {
  return std::make_unique<testing::StrictMock<MockSigninBridge>>();
}

}  // namespace

class EnterpriseProxyTabHelperPlatformDelegateAndroidTest
    : public ChromeRenderViewHostTestHarness {
 public:
  EnterpriseProxyTabHelperPlatformDelegateAndroidTest() = default;
  ~EnterpriseProxyTabHelperPlatformDelegateAndroidTest() override = default;

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    mock_signin_bridge_ = static_cast<MockSigninBridge*>(
        SigninBridgeFactory::GetInstance()->SetTestingFactoryAndUse(
            profile(), base::BindRepeating(&BuildMockSigninBridgeForTesting)));
    identity_test_env_profile_adaptor_ =
        std::make_unique<IdentityTestEnvironmentProfileAdaptor>(profile());
  }

  void TearDown() override {
    identity_test_env_profile_adaptor_.reset();
    mock_signin_bridge_ = nullptr;
    ChromeRenderViewHostTestHarness::TearDown();
  }

  TestingProfile::TestingFactories GetTestingFactories() const override {
    return IdentityTestEnvironmentProfileAdaptor::
        GetIdentityTestEnvironmentFactories();
  }

  MockSigninBridge* signin_bridge() { return mock_signin_bridge_; }

  signin::IdentityTestEnvironment* identity_test_env() {
    return identity_test_env_profile_adaptor_->identity_test_env();
  }

 private:
  std::unique_ptr<IdentityTestEnvironmentProfileAdaptor>
      identity_test_env_profile_adaptor_;
  raw_ptr<MockSigninBridge> mock_signin_bridge_ = nullptr;
};

// Verifies that when no Google accounts exist on the device,
// `StartSignInFlow()` launches the OS add-account flow instead of opening an
// empty bottom sheet (which `SigninBridge` suppresses when no device accounts
// exist).
TEST_F(EnterpriseProxyTabHelperPlatformDelegateAndroidTest,
       StartSignInFlow_NoDeviceAccounts_StartsAddAccountFlow) {
  const GURL test_url("https://destination.example.com/login");
  std::unique_ptr<content::WebContents> test_contents = CreateTestWebContents();
  content::WebContents* raw_contents = test_contents.get();
  std::unique_ptr<TabAndroid> tab =
      TabAndroid::CreateForTesting(profile(), 1, std::move(test_contents));
  ASSERT_EQ(TabAndroid::FromWebContents(raw_contents), tab.get());

  EXPECT_CALL(*signin_bridge(),
              StartAddAccountFlow(tab.get(), "", test_url, ""));

  EnterpriseProxyTabHelperPlatformDelegateAndroid platform_delegate;
  platform_delegate.StartSignInFlow(profile(), raw_contents, test_url);
}

// Verifies that when no device accounts exist and `WebContents` has no attached
// `TabAndroid`, `StartSignInFlow()` safely no-ops without crashing.
TEST_F(EnterpriseProxyTabHelperPlatformDelegateAndroidTest,
       StartSignInFlow_NoDeviceAccounts_NoTabAndroid_DoesNotCrash) {
  const GURL test_url("https://destination.example.com/login");

  EnterpriseProxyTabHelperPlatformDelegateAndroid platform_delegate;
  platform_delegate.StartSignInFlow(profile(), web_contents(), test_url);
}

// Verifies that when at least one Google account exists on the device,
// `StartSignInFlow()` opens the account picker bottom sheet with the first
// device account pre-selected.
TEST_F(EnterpriseProxyTabHelperPlatformDelegateAndroidTest,
       StartSignInFlow_WithDeviceAccount_OpensAccountPicker) {
  const GURL test_url("https://destination.example.com/login");
  AccountInfo account =
      identity_test_env()->MakeAccountAvailable("user@example.com");

  EXPECT_CALL(*signin_bridge(),
              OpenAccountPickerBottomSheetForWebSignin(
                  web_contents(), test_url,
                  testing::Eq(std::make_optional(account.GetAccountId()))));

  EnterpriseProxyTabHelperPlatformDelegateAndroid platform_delegate;
  platform_delegate.StartSignInFlow(profile(), web_contents(), test_url);
}

// Verifies that `StartReauthFlow()` routes to `StartUpdateCredentialsFlow()`
// when `WebContents` is attached to a `TabAndroid`.
TEST_F(EnterpriseProxyTabHelperPlatformDelegateAndroidTest,
       StartReauthFlow_StartsUpdateCredentialsFlow) {
  const GURL test_url("https://destination.example.com/login");
  std::unique_ptr<content::WebContents> test_contents = CreateTestWebContents();
  content::WebContents* raw_contents = test_contents.get();
  std::unique_ptr<TabAndroid> tab =
      TabAndroid::CreateForTesting(profile(), 1, std::move(test_contents));
  ASSERT_EQ(TabAndroid::FromWebContents(raw_contents), tab.get());

  const CoreAccountId account_id =
      CoreAccountId::FromGaiaId(GaiaId("test_gaia_id"));
  EXPECT_CALL(
      *signin_bridge(),
      StartUpdateCredentialsFlow(tab.get(), test_url, account_id, account_id));

  EnterpriseProxyTabHelperPlatformDelegateAndroid platform_delegate;
  platform_delegate.StartReauthFlow(profile(), raw_contents, test_url,
                                    account_id);
}

// Verifies that when `WebContents` is not associated with a `TabAndroid`,
// `StartReauthFlow()` safely no-ops rather than passing a `nullptr` tab.
TEST_F(EnterpriseProxyTabHelperPlatformDelegateAndroidTest,
       StartReauthFlow_NoTabAndroid_DoesNotCrash) {
  const GURL test_url("https://destination.example.com/login");
  const CoreAccountId account_id =
      CoreAccountId::FromGaiaId(GaiaId("test_gaia_id"));

  EnterpriseProxyTabHelperPlatformDelegateAndroid platform_delegate;
  platform_delegate.StartReauthFlow(profile(), web_contents(), test_url,
                                    account_id);
}

}  // namespace enterprise_net
