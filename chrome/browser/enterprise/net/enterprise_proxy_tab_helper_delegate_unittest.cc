// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/net/enterprise_proxy_tab_helper_delegate.h"

#include <memory>
#include <utility>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_test_environment_profile_adaptor.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_profile.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/web_contents.h"
#include "google_apis/gaia/core_account_id.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace enterprise_net {
namespace {

class MockPlatformDelegate
    : public EnterpriseProxyTabHelperDelegate::PlatformDelegate {
 public:
  MockPlatformDelegate() = default;
  ~MockPlatformDelegate() override = default;

  MOCK_METHOD(void,
              StartSignInFlow,
              (Profile * profile,
               content::WebContents* web_contents,
               const GURL& destination_url),
              (override));

  MOCK_METHOD(void,
              StartReauthFlow,
              (Profile * profile,
               content::WebContents* web_contents,
               const GURL& destination_url,
               const CoreAccountId& account_id),
              (override));
};

}  // namespace

class EnterpriseProxyTabHelperDelegateTest
    : public ChromeRenderViewHostTestHarness {
 public:
  EnterpriseProxyTabHelperDelegateTest() = default;
  ~EnterpriseProxyTabHelperDelegateTest() override = default;

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    identity_test_env_profile_adaptor_ =
        std::make_unique<IdentityTestEnvironmentProfileAdaptor>(profile());
    auto platform_delegate =
        std::make_unique<testing::StrictMock<MockPlatformDelegate>>();
    mock_platform_delegate_ = platform_delegate.get();
    delegate_ = std::make_unique<EnterpriseProxyTabHelperDelegate>(
        std::move(platform_delegate));
  }

  void TearDown() override {
    mock_platform_delegate_ = nullptr;
    delegate_.reset();
    identity_test_env_profile_adaptor_.reset();
    ChromeRenderViewHostTestHarness::TearDown();
  }

  TestingProfile::TestingFactories GetTestingFactories() const override {
    return IdentityTestEnvironmentProfileAdaptor::
        GetIdentityTestEnvironmentFactories();
  }

  MockPlatformDelegate* platform_delegate() { return mock_platform_delegate_; }

  EnterpriseProxyTabHelperDelegate* delegate() { return delegate_.get(); }

  signin::IdentityTestEnvironment* identity_test_env() {
    return identity_test_env_profile_adaptor_->identity_test_env();
  }

 private:
  std::unique_ptr<IdentityTestEnvironmentProfileAdaptor>
      identity_test_env_profile_adaptor_;
  std::unique_ptr<EnterpriseProxyTabHelperDelegate> delegate_;
  raw_ptr<MockPlatformDelegate> mock_platform_delegate_ = nullptr;
};

// Verifies that passing a null WebContents safely no-ops. This can happen if
// the tab or its WebContents was destroyed or closed concurrently while the
// error page's "Continue" (sign-in) action was being dispatched.
TEST_F(EnterpriseProxyTabHelperDelegateTest, NullWebContentsDoesNotCrash) {
  delegate()->SignIn(nullptr);
}

// Verifies that a null PlatformDelegate (e.g. on platforms where sign-in UI is
// not yet wired up) safely no-ops without crashing.
TEST_F(EnterpriseProxyTabHelperDelegateTest, NullPlatformDelegateDoesNotCrash) {
  const GURL test_url("https://destination.example.com/login");
  NavigateAndCommit(test_url);

  EnterpriseProxyTabHelperDelegate delegate_without_platform(
      /*platform_delegate=*/nullptr);
  delegate_without_platform.SignIn(web_contents());
  EXPECT_EQ(web_contents()->GetController().GetPendingEntry(), nullptr);
}

// Verifies that when the primary account is already signed in with a valid
// refresh token, `SignIn()` reloads the `WebContents` directly without
// invoking `PlatformDelegate`.
TEST_F(EnterpriseProxyTabHelperDelegateTest,
       SignInWithPrimaryAccount_ValidRefreshToken_ReloadsWebContents) {
  const GURL test_url("https://destination.example.com/login");
  NavigateAndCommit(test_url);
  ASSERT_EQ(web_contents()->GetController().GetPendingEntry(), nullptr);

  identity_test_env()->MakePrimaryAccountAvailable(
      "user@example.com", signin::ConsentLevel::kSignin);

  delegate()->SignIn(web_contents());

  content::NavigationEntry* pending_entry =
      web_contents()->GetController().GetPendingEntry();
  ASSERT_NE(pending_entry, nullptr);
  EXPECT_EQ(pending_entry->GetURL(), test_url);
}

// Verifies that when signed out (no primary account), `SignIn()` delegates to
// `PlatformDelegate::StartSignInFlow()` and does not reload prematurely.
TEST_F(EnterpriseProxyTabHelperDelegateTest,
       SignInWithoutPrimaryAccount_StartsSignInFlow) {
  const GURL test_url("https://destination.example.com/login");
  NavigateAndCommit(test_url);

  EXPECT_CALL(*platform_delegate(),
              StartSignInFlow(profile(), web_contents(), test_url));

  delegate()->SignIn(web_contents());
  EXPECT_EQ(web_contents()->GetController().GetPendingEntry(), nullptr);
}

// Verifies that when the primary account has an invalid refresh token (auth
// error), `SignIn()` delegates to `PlatformDelegate::StartReauthFlow()` and
// does not reload prematurely.
TEST_F(EnterpriseProxyTabHelperDelegateTest,
       SignInWithPrimaryAccount_InvalidRefreshToken_StartsReauthFlow) {
  const GURL test_url("https://destination.example.com/login");
  NavigateAndCommit(test_url);

  CoreAccountId account_id =
      identity_test_env()
          ->MakePrimaryAccountAvailable("user@example.com",
                                        signin::ConsentLevel::kSignin)
          .GetAccountId();
  identity_test_env()->SetInvalidRefreshTokenForAccount(account_id);

  EXPECT_CALL(*platform_delegate(),
              StartReauthFlow(profile(), web_contents(), test_url, account_id));

  delegate()->SignIn(web_contents());
  EXPECT_EQ(web_contents()->GetController().GetPendingEntry(), nullptr);
}

// Verifies that when the primary account is missing a refresh token, `SignIn()`
// delegates to `PlatformDelegate::StartReauthFlow()` and does not reload
// prematurely.
TEST_F(EnterpriseProxyTabHelperDelegateTest,
       SignInWithPrimaryAccount_NoRefreshToken_StartsReauthFlow) {
  const GURL test_url("https://destination.example.com/login");
  NavigateAndCommit(test_url);

  CoreAccountId account_id =
      identity_test_env()
          ->MakePrimaryAccountAvailable("user@example.com",
                                        signin::ConsentLevel::kSignin)
          .GetAccountId();
  identity_test_env()->RemoveRefreshTokenForPrimaryAccount();

  EXPECT_CALL(*platform_delegate(),
              StartReauthFlow(profile(), web_contents(), test_url, account_id));

  delegate()->SignIn(web_contents());
  EXPECT_EQ(web_contents()->GetController().GetPendingEntry(), nullptr);
}

}  // namespace enterprise_net
