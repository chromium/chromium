// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/net/enterprise_proxy_tab_helper_delegate.h"

#include <memory>
#include <optional>

#include "base/functional/bind.h"
#include "chrome/browser/android/tab_android.h"
#include "chrome/browser/signin/android/signin_bridge.h"
#include "chrome/browser/signin/android/signin_bridge_factory.h"
#include "chrome/browser/signin/identity_test_environment_profile_adaptor.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/navigation_simulator.h"
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
              OpenAccountPickerBottomSheetForWebSignin,
              (content::WebContents * web_contents,
               const GURL& continue_url,
               const std::optional<CoreAccountId>& account_id),
              (override));

  MOCK_METHOD(void,
              StartUpdateCredentialsFlow,
              (TabAndroid * tab,
               const GURL& continue_url,
               const CoreAccountId& account_id),
              (override));
};

std::unique_ptr<KeyedService> BuildMockSigninBridgeForTesting(
    content::BrowserContext* context) {
  return std::make_unique<MockSigninBridge>();
}

}  // namespace

class EnterpriseProxyTabHelperDelegateTest
    : public ChromeRenderViewHostTestHarness {
 public:
  EnterpriseProxyTabHelperDelegateTest() = default;
  ~EnterpriseProxyTabHelperDelegateTest() override = default;

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

// Verifies that passing a null WebContents safely no-ops. This can happen if
// the tab or its WebContents was destroyed or closed concurrently while the
// error page's "Continue" (sign-in) action was being dispatched.
TEST_F(EnterpriseProxyTabHelperDelegateTest, NullWebContentsDoesNotCrash) {
  EnterpriseProxyTabHelperDelegate delegate;
  delegate.SignIn(nullptr);
}

TEST_F(EnterpriseProxyTabHelperDelegateTest,
       SignInWithoutPrimaryAccount_OpensAccountPicker) {
  const GURL test_url("https://destination.example.com/login");
  NavigateAndCommit(test_url);

  EnterpriseProxyTabHelperDelegate delegate;
  EXPECT_CALL(*signin_bridge(),
              OpenAccountPickerBottomSheetForWebSignin(
                  web_contents(), test_url, testing::Eq(std::nullopt)));

  delegate.SignIn(web_contents());
}

TEST_F(EnterpriseProxyTabHelperDelegateTest,
       SignInWithPrimaryAccount_StartsUpdateCredentialsFlow) {
  const GURL test_url("https://destination.example.com/login");
  std::unique_ptr<content::WebContents> test_contents = CreateTestWebContents();
  content::WebContents* raw_contents = test_contents.get();
  content::NavigationSimulator::NavigateAndCommitFromBrowser(raw_contents,
                                                             test_url);

  std::unique_ptr<TabAndroid> tab =
      TabAndroid::CreateForTesting(profile(), 1, std::move(test_contents));
  ASSERT_EQ(TabAndroid::FromWebContents(raw_contents), tab.get());

  CoreAccountId account_id =
      identity_test_env()
          ->MakePrimaryAccountAvailable("user@example.com",
                                        signin::ConsentLevel::kSignin)
          .GetAccountId();

  EnterpriseProxyTabHelperDelegate delegate;
  EXPECT_CALL(*signin_bridge(),
              StartUpdateCredentialsFlow(tab.get(), test_url, account_id));

  delegate.SignIn(raw_contents);
}

// Verifies that when the primary account has an invalid refresh token (auth
// error), SignIn still routes to StartUpdateCredentialsFlow so the user can
// re-authenticate.
TEST_F(
    EnterpriseProxyTabHelperDelegateTest,
    SignInWithPrimaryAccount_InvalidRefreshToken_StartsUpdateCredentialsFlow) {
  const GURL test_url("https://destination.example.com/login");
  std::unique_ptr<content::WebContents> test_contents = CreateTestWebContents();
  content::WebContents* raw_contents = test_contents.get();
  content::NavigationSimulator::NavigateAndCommitFromBrowser(raw_contents,
                                                             test_url);

  std::unique_ptr<TabAndroid> tab =
      TabAndroid::CreateForTesting(profile(), 1, std::move(test_contents));
  ASSERT_EQ(TabAndroid::FromWebContents(raw_contents), tab.get());

  CoreAccountId account_id =
      identity_test_env()
          ->MakePrimaryAccountAvailable("user@example.com",
                                        signin::ConsentLevel::kSignin)
          .GetAccountId();
  identity_test_env()->SetInvalidRefreshTokenForAccount(account_id);

  EnterpriseProxyTabHelperDelegate delegate;
  EXPECT_CALL(*signin_bridge(),
              StartUpdateCredentialsFlow(tab.get(), test_url, account_id));

  delegate.SignIn(raw_contents);
}

// Verifies that when a primary account is available but the WebContents is not
// associated with a native TabAndroid (e.g. background or prerendering
// contents, or contents detached during tab model transitions), SignIn safely
// no-ops rather than passing a nullptr tab or dereferencing it.
TEST_F(EnterpriseProxyTabHelperDelegateTest,
       SignInWithPrimaryAccount_NoTabAndroid_DoesNotCrash) {
  const GURL test_url("https://destination.example.com/login");
  NavigateAndCommit(test_url);

  identity_test_env()->MakePrimaryAccountAvailable(
      "user@example.com", signin::ConsentLevel::kSignin);

  // web_contents() has no TabAndroid attached, so StartUpdateCredentialsFlow
  // should not be invoked and it should not crash.
  EXPECT_CALL(*signin_bridge(),
              StartUpdateCredentialsFlow(testing::_, testing::_, testing::_))
      .Times(0);

  EnterpriseProxyTabHelperDelegate delegate;
  delegate.SignIn(web_contents());
}

}  // namespace enterprise_net
