// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/process_dice_header_delegate_impl.h"

#include <memory>
#include <string>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/signin/dice_tab_helper.h"
#include "chrome/browser/signin/dice_web_signin_interceptor.h"
#include "chrome/browser/signin/dice_web_signin_interceptor_factory.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/signin/identity_test_environment_profile_adaptor.h"
#include "chrome/browser/ui/webui/signin/signin_ui_error.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/signin/public/base/account_consistency_method.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/navigation_simulator.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using signin_metrics::Reason;

namespace {

signin_metrics::AccessPoint kTestAccessPoint =
    signin_metrics::AccessPoint::ACCESS_POINT_BOOKMARK_BUBBLE;

signin_metrics::PromoAction kTestPromoAction =
    signin_metrics::PromoAction::PROMO_ACTION_NO_SIGNIN_PROMO;

// Dummy delegate that declines all interceptions.
class TestDiceWebSigninInterceptorDelegate
    : public DiceWebSigninInterceptor::Delegate {
 public:
  ~TestDiceWebSigninInterceptorDelegate() override = default;
  std::unique_ptr<ScopedDiceWebSigninInterceptionBubbleHandle>
  ShowSigninInterceptionBubble(
      content::WebContents* web_contents,
      const BubbleParameters& bubble_parameters,
      base::OnceCallback<void(SigninInterceptionResult)> callback) override {
    std::move(callback).Run(SigninInterceptionResult::kDeclined);
    return nullptr;
  }

  void ShowFirstRunExperienceInNewProfile(
      Browser* browser,
      const CoreAccountId& account_id,
      DiceWebSigninInterceptor::SigninInterceptionType interception_type)
      override {}
};

class MockDiceWebSigninInterceptor : public DiceWebSigninInterceptor {
 public:
  explicit MockDiceWebSigninInterceptor(Profile* profile)
      : DiceWebSigninInterceptor(
            profile,
            std::make_unique<TestDiceWebSigninInterceptorDelegate>()) {}
  ~MockDiceWebSigninInterceptor() override = default;

  MOCK_METHOD(void,
              MaybeInterceptWebSignin,
              (content::WebContents * web_contents,
               CoreAccountId account_id,
               bool is_new_account,
               bool is_sync_signin),
              (override));
};

std::unique_ptr<KeyedService> CreateMockDiceWebSigninInterceptor(
    content::BrowserContext* context) {
  return std::make_unique<MockDiceWebSigninInterceptor>(
      Profile::FromBrowserContext(context));
}

class ProcessDiceHeaderDelegateImplTest
    : public ChromeRenderViewHostTestHarness {
 public:
  ProcessDiceHeaderDelegateImplTest()
      : enable_sync_called_(false),
        show_error_called_(false),
        account_id_("12345"),
        email_("foo@bar.com"),
        auth_error_(GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS) {}

  ~ProcessDiceHeaderDelegateImplTest() override {}

  void AddAccount(bool is_primary) {
    if (!identity_test_environment_profile_adaptor_)
      InitializeIdentityTestEnvironment();
    if (is_primary) {
      identity_test_environment_profile_adaptor_->identity_test_env()
          ->SetPrimaryAccount(email_, signin::ConsentLevel::kSync);
    } else {
      identity_test_environment_profile_adaptor_->identity_test_env()
          ->MakeAccountAvailable(email_);
    }
  }

  void InitializeIdentityTestEnvironment() {
    DCHECK(profile());
    identity_test_environment_profile_adaptor_ =
        std::make_unique<IdentityTestEnvironmentProfileAdaptor>(profile());
  }

  // Creates a ProcessDiceHeaderDelegateImpl instance.
  std::unique_ptr<ProcessDiceHeaderDelegateImpl>
  CreateDelegateAndNavigateToSignin(
      bool is_sync_signin_tab,
      Reason reason = Reason::kSigninPrimaryAccount) {
    signin_reason_ = reason;
    if (!identity_test_environment_profile_adaptor_)
      InitializeIdentityTestEnvironment();
    // Load the signin page.
    std::unique_ptr<content::NavigationSimulator> simulator =
        content::NavigationSimulator::CreateRendererInitiated(signin_url_,
                                                              main_rfh());
    simulator->Start();
    if (is_sync_signin_tab) {
      DiceTabHelper::CreateForWebContents(web_contents());
      DiceTabHelper* dice_tab_helper =
          DiceTabHelper::FromWebContents(web_contents());
      dice_tab_helper->InitializeSigninFlow(signin_url_, kTestAccessPoint,
                                            signin_reason_, kTestPromoAction,
                                            GURL::EmptyGURL());
    }
    simulator->Commit();
    DCHECK_EQ(signin_url_, web_contents()->GetVisibleURL());
    return std::make_unique<ProcessDiceHeaderDelegateImpl>(
        web_contents(),
        base::BindOnce(&ProcessDiceHeaderDelegateImplTest::StartSyncCallback,
                       base::Unretained(this)),
        base::BindOnce(
            &ProcessDiceHeaderDelegateImplTest::ShowSigninErrorCallback,
            base::Unretained(this)));
  }

  // ChromeRenderViewHostTestHarness:
  TestingProfile::TestingFactories GetTestingFactories() const override {
    TestingProfile::TestingFactories factories = {
        {DiceWebSigninInterceptorFactory::GetInstance(),
         base::BindRepeating(&CreateMockDiceWebSigninInterceptor)}};
    IdentityTestEnvironmentProfileAdaptor::
        AppendIdentityTestEnvironmentFactories(&factories);
    return factories;
  }

  void TearDown() override {
    identity_test_environment_profile_adaptor_.reset();
    ChromeRenderViewHostTestHarness::TearDown();
  }

  // Callback for the ProcessDiceHeaderDelegateImpl.
  void StartSyncCallback(Profile* profile,
                         signin_metrics::AccessPoint access_point,
                         signin_metrics::PromoAction promo_action,
                         signin_metrics::Reason reason,
                         content::WebContents* contents,
                         const CoreAccountId& account_id) {
    EXPECT_EQ(profile, this->profile());
    EXPECT_EQ(access_point, kTestAccessPoint);
    EXPECT_EQ(promo_action, kTestPromoAction);
    EXPECT_EQ(reason, signin_reason_);
    EXPECT_EQ(web_contents(), contents);
    EXPECT_EQ(account_id_, account_id);
    enable_sync_called_ = true;
  }

  // Callback for the ProcessDiceHeaderDelegateImpl.
  void ShowSigninErrorCallback(Profile* profile,
                               content::WebContents* contents,
                               const SigninUIError& error) {
    EXPECT_EQ(profile, this->profile());
    EXPECT_EQ(web_contents(), contents);
    EXPECT_EQ(base::UTF8ToUTF16(auth_error_.ToString()), error.message());
    EXPECT_EQ(base::UTF8ToUTF16(email_), error.email());
    show_error_called_ = true;
  }

  MockDiceWebSigninInterceptor* mock_interceptor() {
    return static_cast<MockDiceWebSigninInterceptor*>(
        DiceWebSigninInterceptorFactory::GetForProfile(profile()));
  }

  std::unique_ptr<IdentityTestEnvironmentProfileAdaptor>
      identity_test_environment_profile_adaptor_;

  const GURL signin_url_ = GURL("https://accounts.google.com");
  bool enable_sync_called_;
  bool show_error_called_;
  CoreAccountId account_id_;
  std::string email_;
  GoogleServiceAuthError auth_error_;
  Reason signin_reason_ = Reason::kSigninPrimaryAccount;
};

// Check that sync is enabled if the tab is closed during signin.
TEST_F(ProcessDiceHeaderDelegateImplTest, CloseTabWhileStartingSync) {
  std::unique_ptr<ProcessDiceHeaderDelegateImpl> delegate =
      CreateDelegateAndNavigateToSignin(true);

  // Close the tab.
  DeleteContents();

  // Check expectations.
  delegate->EnableSync(account_id_);
  EXPECT_TRUE(enable_sync_called_);
  EXPECT_FALSE(show_error_called_);
}

// Check that the error is still shown if the tab is closed before the error is
// received.
TEST_F(ProcessDiceHeaderDelegateImplTest, CloseTabWhileFailingSignin) {
  std::unique_ptr<ProcessDiceHeaderDelegateImpl> delegate =
      CreateDelegateAndNavigateToSignin(true);

  // Close the tab.
  DeleteContents();

  // Check expectations.
  delegate->HandleTokenExchangeFailure(email_, auth_error_);
  EXPECT_FALSE(enable_sync_called_);
  EXPECT_TRUE(show_error_called_);
}

struct TestConfiguration {
  // Test setup.
  bool signed_in;   // User was already signed in at the start of the flow.
  bool signin_tab;  // The tab is marked as a Sync signin tab.

  // Test expectations.
  bool callback_called;  // The relevant callback was called.
  bool show_ntp;         // The NTP was shown.
};

TestConfiguration kEnableSyncTestCases[] = {
    // clang-format off
    // signed_in | signin_tab | callback_called | show_ntp
    {  false,      false,       false,            false},
    {  false,      true,        true,             true},
    {  true,       false,       false,            false},
    {  true,       true,        false,            false},
    // clang-format on
};

// Parameterized version of ProcessDiceHeaderDelegateImplTest.
class ProcessDiceHeaderDelegateImplTestEnableSync
    : public ProcessDiceHeaderDelegateImplTest,
      public ::testing::WithParamInterface<TestConfiguration> {};

// Test the EnableSync() method in all configurations.
TEST_P(ProcessDiceHeaderDelegateImplTestEnableSync, EnableSync) {
  if (GetParam().signed_in)
    AddAccount(/*is_primary=*/true);
  std::unique_ptr<ProcessDiceHeaderDelegateImpl> delegate =
      CreateDelegateAndNavigateToSignin(GetParam().signin_tab);
  delegate->EnableSync(account_id_);
  EXPECT_EQ(GetParam().callback_called, enable_sync_called_);
  GURL expected_url =
      GetParam().show_ntp ? GURL(chrome::kChromeUINewTabURL) : signin_url_;
  EXPECT_EQ(expected_url, web_contents()->GetVisibleURL());
  EXPECT_FALSE(show_error_called_);
  // Check that the sync signin flow is complete.
  if (GetParam().signin_tab) {
    DiceTabHelper* dice_tab_helper =
        DiceTabHelper::FromWebContents(web_contents());
    ASSERT_TRUE(dice_tab_helper);
    EXPECT_FALSE(dice_tab_helper->IsSyncSigninInProgress());
  }
}

INSTANTIATE_TEST_SUITE_P(All,
                         ProcessDiceHeaderDelegateImplTestEnableSync,
                         ::testing::ValuesIn(kEnableSyncTestCases));

TestConfiguration kHandleTokenExchangeFailureTestCases[] = {
    // clang-format off
    // signed_in | signin_tab | callback_called | show_ntp
    {  false,      false,       true,             false},
    {  false,      true,        true,             true},
    {  true,       false,       true,             false},
    {  true,       true,        true,             false},
    // clang-format on
};

// Parameterized version of ProcessDiceHeaderDelegateImplTest.
class ProcessDiceHeaderDelegateImplTestHandleTokenExchangeFailure
    : public ProcessDiceHeaderDelegateImplTest,
      public ::testing::WithParamInterface<TestConfiguration> {};

// Test the HandleTokenExchangeFailure() method in all configurations.
TEST_P(ProcessDiceHeaderDelegateImplTestHandleTokenExchangeFailure,
       HandleTokenExchangeFailure) {
  if (GetParam().signed_in)
    AddAccount(/*is_primary=*/true);
  std::unique_ptr<ProcessDiceHeaderDelegateImpl> delegate =
      CreateDelegateAndNavigateToSignin(GetParam().signin_tab);
  delegate->HandleTokenExchangeFailure(email_, auth_error_);
  EXPECT_FALSE(enable_sync_called_);
  EXPECT_EQ(GetParam().callback_called, show_error_called_);
  GURL expected_url =
      GetParam().show_ntp ? GURL(chrome::kChromeUINewTabURL) : signin_url_;
  EXPECT_EQ(expected_url, web_contents()->GetVisibleURL());
  // Check that the sync signin flow is complete.
  if (GetParam().signin_tab) {
    DiceTabHelper* dice_tab_helper =
        DiceTabHelper::FromWebContents(web_contents());
    ASSERT_TRUE(dice_tab_helper);
    EXPECT_FALSE(dice_tab_helper->IsSyncSigninInProgress());
  }
}

INSTANTIATE_TEST_SUITE_P(
    All,
    ProcessDiceHeaderDelegateImplTestHandleTokenExchangeFailure,
    ::testing::ValuesIn(kHandleTokenExchangeFailureTestCases));

struct TokenExchangeSuccessConfiguration {
  bool is_reauth;   // User was already signed in with the account.
  bool signin_tab;  // A DiceTabHelper is attached to the tab.
  Reason reason;
  bool sync_signin;  // Expected value for the MaybeInterceptWebSigin call.
};

TokenExchangeSuccessConfiguration kHandleTokenExchangeSuccessTestCases[] = {
    // clang-format off
    // is_reauth | signin_tab |       reason               | sync_signin
    {  false,      false,     Reason::kSigninPrimaryAccount, false },
    {  false,      true,      Reason::kSigninPrimaryAccount, true },
    {  false,      true,      Reason::kAddSecondaryAccount,  false },
    {  true,       false,     Reason::kSigninPrimaryAccount, false },
    {  true,       true,      Reason::kSigninPrimaryAccount, true },
    // clang-format on
};

// Parameterized version of ProcessDiceHeaderDelegateImplTest.
class ProcessDiceHeaderDelegateImplTestHandleTokenExchangeSuccess
    : public ProcessDiceHeaderDelegateImplTest,
      public ::testing::WithParamInterface<TokenExchangeSuccessConfiguration> {
};

// Test the HandleTokenExchangeSuccess() method in all configurations.
TEST_P(ProcessDiceHeaderDelegateImplTestHandleTokenExchangeSuccess,
       HandleTokenExchangeSuccess) {
  if (GetParam().is_reauth)
    AddAccount(/*is_primary=*/false);
  std::unique_ptr<ProcessDiceHeaderDelegateImpl> delegate =
      CreateDelegateAndNavigateToSignin(GetParam().signin_tab,
                                        GetParam().reason);
  EXPECT_CALL(
      *mock_interceptor(),
      MaybeInterceptWebSignin(web_contents(), account_id_,
                              !GetParam().is_reauth, GetParam().sync_signin));
  delegate->HandleTokenExchangeSuccess(account_id_, !GetParam().is_reauth);

  // Check that the sync signin flow is complete.
  if (GetParam().signin_tab) {
    DiceTabHelper* dice_tab_helper =
        DiceTabHelper::FromWebContents(web_contents());
    ASSERT_TRUE(dice_tab_helper);
    EXPECT_EQ(GetParam().sync_signin,
              dice_tab_helper->IsSyncSigninInProgress());
  }
}

INSTANTIATE_TEST_SUITE_P(
    All,
    ProcessDiceHeaderDelegateImplTestHandleTokenExchangeSuccess,
    ::testing::ValuesIn(kHandleTokenExchangeSuccessTestCases));

}  // namespace
