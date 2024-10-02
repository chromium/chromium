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
#include "components/signin/public/base/signin_metrics.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/navigation_simulator.h"
#include "google_apis/gaia/core_account_id.h"
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
    : public WebSigninInterceptor::Delegate {
 public:
  ~TestDiceWebSigninInterceptorDelegate() override = default;

  bool IsSigninInterceptionSupported(
      const content::WebContents& web_contents) override {
    return false;
  }

  std::unique_ptr<ScopedWebSigninInterceptionBubbleHandle>
  ShowSigninInterceptionBubble(
      content::WebContents* web_contents,
      const BubbleParameters& bubble_parameters,
      base::OnceCallback<void(SigninInterceptionResult)> callback) override {
    std::move(callback).Run(SigninInterceptionResult::kDeclined);
    return nullptr;
  }

  std::unique_ptr<ScopedWebSigninInterceptionBubbleHandle>
  ShowOidcInterceptionDialog(
      content::WebContents* web_contents,
      const BubbleParameters& bubble_parameters,
      signin::SigninChoiceWithConfirmationCallback callback,
      base::OnceClosure done_callback,
      base::OnceClosure retry_callback) override {
    std::move(callback)
        .Then(std::move(done_callback))
        .Run(signin::SIGNIN_CHOICE_CANCEL, base::DoNothing());
    return nullptr;
  }

  void ShowFirstRunExperienceInNewProfile(
      Browser* browser,
      const CoreAccountId& account_id,
      WebSigninInterceptor::SigninInterceptionType interception_type) override {
  }
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
               signin_metrics::AccessPoint access_point,
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
        signin_header_received_(false),
        show_error_called_(false),
        email_("foo@bar.com"),
        auth_error_(GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS) {
    std::string kGaiaId = "12345";
    account_info_.account_id = CoreAccountId::FromGaiaId(kGaiaId);
    account_info_.gaia = kGaiaId;
    account_info_.email = "email@gmail.com";
  }

  ~ProcessDiceHeaderDelegateImplTest() override = default;

  void AddAccount(bool is_primary) {
    if (!identity_test_environment_profile_adaptor_) {
      InitializeIdentityTestEnvironment();
    }
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
      const GURL& redirect_url,
      Reason reason = Reason::kSigninPrimaryAccount) {
    signin_reason_ = reason;
    if (!identity_test_environment_profile_adaptor_) {
      InitializeIdentityTestEnvironment();
    }
    // Load the signin page.
    std::unique_ptr<content::NavigationSimulator> simulator =
        content::NavigationSimulator::CreateRendererInitiated(signin_url_,
                                                              main_rfh());
    simulator->Start();
    if (is_sync_signin_tab) {
      DiceTabHelper::CreateForWebContents(web_contents());
      DiceTabHelper* dice_tab_helper =
          DiceTabHelper::FromWebContents(web_contents());
      dice_tab_helper->InitializeSigninFlow(
          signin_url_, kTestAccessPoint, signin_reason_, kTestPromoAction,
          redirect_url,
          /*record_signin_started_metrics=*/true,
          base::BindRepeating(
              &ProcessDiceHeaderDelegateImplTest::StartSyncCallback,
              base::Unretained(this)),
          base::BindRepeating(
              &ProcessDiceHeaderDelegateImplTest::OnSigninHeaderReceived,
              base::Unretained(this)),
          base::BindRepeating(
              &ProcessDiceHeaderDelegateImplTest::ShowSigninErrorCallback,
              base::Unretained(this)));
    }
    simulator->Commit();
    DCHECK_EQ(signin_url_, web_contents()->GetVisibleURL());

    if (is_sync_signin_tab) {
      return ProcessDiceHeaderDelegateImpl::Create(web_contents());
    } else {
      // Use the constructor rather than the `Create()` method, to specify the
      // error callback.
      return std::make_unique<ProcessDiceHeaderDelegateImpl>(
          web_contents(), /*is_sync_signin_tab=*/false,
          signin_metrics::AccessPoint::ACCESS_POINT_WEB_SIGNIN,
          kTestPromoAction, GURL(),
          ProcessDiceHeaderDelegateImpl::EnableSyncCallback(),
          base::BindRepeating(
              &ProcessDiceHeaderDelegateImplTest::OnSigninHeaderReceived,
              base::Unretained(this)),
          base::BindOnce(
              &ProcessDiceHeaderDelegateImplTest::ShowSigninErrorCallback,
              base::Unretained(this)));
    }
  }

  // ChromeRenderViewHostTestHarness:
  TestingProfile::TestingFactories GetTestingFactories() const override {
    return IdentityTestEnvironmentProfileAdaptor::
        GetIdentityTestEnvironmentFactoriesWithAppendedFactories(
            {TestingProfile::TestingFactory{
                DiceWebSigninInterceptorFactory::GetInstance(),
                base::BindRepeating(&CreateMockDiceWebSigninInterceptor)}});
  }

  void TearDown() override {
    identity_test_environment_profile_adaptor_.reset();
    ChromeRenderViewHostTestHarness::TearDown();
  }

  // Callback for the ProcessDiceHeaderDelegateImpl.
  void StartSyncCallback(Profile* profile,
                         signin_metrics::AccessPoint access_point,
                         signin_metrics::PromoAction promo_action,
                         content::WebContents* contents,
                         const CoreAccountInfo& account_info) {
    EXPECT_EQ(profile, this->profile());
    EXPECT_EQ(access_point, kTestAccessPoint);
    EXPECT_EQ(promo_action, kTestPromoAction);
    EXPECT_EQ(web_contents(), contents);
    EXPECT_EQ(account_info_, account_info);
    enable_sync_called_ = true;
  }

  void OnSigninHeaderReceived() { signin_header_received_ = true; }

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
  bool signin_header_received_;
  bool show_error_called_;
  CoreAccountInfo account_info_;
  std::string email_;
  GoogleServiceAuthError auth_error_;
  Reason signin_reason_ = Reason::kSigninPrimaryAccount;
};

// Check that sync is enabled if the tab is closed during signin.
TEST_F(ProcessDiceHeaderDelegateImplTest, CloseTabWhileStartingSync) {
  std::unique_ptr<ProcessDiceHeaderDelegateImpl> delegate =
      CreateDelegateAndNavigateToSignin(/*is_sync_signin_tab=*/true,
                                        /*redirect_url=*/GURL());

  // Close the tab.
  DeleteContents();

  // Check expectations.
  delegate->EnableSync(account_info_);
  EXPECT_TRUE(enable_sync_called_);
  EXPECT_FALSE(show_error_called_);
}

// Check that the error is still shown if the tab is closed before the error is
// received.
TEST_F(ProcessDiceHeaderDelegateImplTest, CloseTabWhileFailingSignin) {
  std::unique_ptr<ProcessDiceHeaderDelegateImpl> delegate =
      CreateDelegateAndNavigateToSignin(/*is_sync_signin_tab=*/true,
                                        /*redirect_url=*/GURL());

  // Close the tab.
  DeleteContents();

  // Check expectations.
  delegate->HandleTokenExchangeFailure(email_, auth_error_);
  EXPECT_FALSE(enable_sync_called_);
  EXPECT_TRUE(show_error_called_);
}

// Tests that there is no redirect when `redirect_url` is empty.
TEST_F(ProcessDiceHeaderDelegateImplTest, NoRedirect) {
  std::unique_ptr<ProcessDiceHeaderDelegateImpl> delegate =
      CreateDelegateAndNavigateToSignin(/*is_sync_signin_tab=*/true,
                                        /*redirect_url=*/GURL());
  delegate->EnableSync(account_info_);
  EXPECT_TRUE(enable_sync_called_);
  // There was no redirect.
  EXPECT_EQ(signin_url_, web_contents()->GetVisibleURL());
  EXPECT_FALSE(show_error_called_);
  // Check that the sync signin flow is complete.
  DiceTabHelper* dice_tab_helper =
      DiceTabHelper::FromWebContents(web_contents());
  ASSERT_TRUE(dice_tab_helper);
  EXPECT_FALSE(dice_tab_helper->IsSyncSigninInProgress());
}

// Check that a Dice header can still be processed in a reused tab.
// Regression test for https://crbug.com/1471277
TEST_F(ProcessDiceHeaderDelegateImplTest, TabReuse) {
  // Complete a first signin flow.
  std::unique_ptr<ProcessDiceHeaderDelegateImpl> delegate =
      CreateDelegateAndNavigateToSignin(/*is_sync_signin_tab=*/true,
                                        /*redirect_url=*/GURL());
  delegate->EnableSync(account_info_);
  EXPECT_TRUE(enable_sync_called_);
  EXPECT_FALSE(show_error_called_);

  // Receive another Dice header in the same tab.
  enable_sync_called_ = false;
  ProcessDiceHeaderDelegateImpl::Create(web_contents());
  // Calling `EnableSync()` does nothing because the tab has already been used.
  delegate->EnableSync(account_info_);
  EXPECT_FALSE(enable_sync_called_);
  EXPECT_FALSE(show_error_called_);
}

TEST_F(ProcessDiceHeaderDelegateImplTest, SigninHeaderReceived) {
  // Complete a first signin flow.
  std::unique_ptr<ProcessDiceHeaderDelegateImpl> delegate =
      CreateDelegateAndNavigateToSignin(/*is_sync_signin_tab=*/true,
                                        /*redirect_url=*/GURL());
  ASSERT_FALSE(signin_header_received_);

  delegate->OnDiceSigninHeaderReceived();
  EXPECT_TRUE(signin_header_received_);

  // Delete content and reset the received value.
  DeleteContents();
  signin_header_received_ = false;

  delegate->OnDiceSigninHeaderReceived();
  // Make sure the message is not propagated after the content (and the attached
  // DiceTabHelper as well) is deleted.
  EXPECT_FALSE(signin_header_received_);
}

TEST_F(ProcessDiceHeaderDelegateImplTest, SigninHeaderReceived_SyncingTabOff) {
  // Complete a first signin flow.
  std::unique_ptr<ProcessDiceHeaderDelegateImpl> delegate =
      CreateDelegateAndNavigateToSignin(/*is_sync_signin_tab=*/false,
                                        /*redirect_url=*/GURL());
  ASSERT_FALSE(signin_header_received_);

  delegate->OnDiceSigninHeaderReceived();

  // Since there is DiceTabHelper created, we do not expect the message to be
  // redirected.
  EXPECT_FALSE(signin_header_received_);
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
  if (GetParam().signed_in) {
    AddAccount(/*is_primary=*/true);
  }
  const GURL kNtpUrl(chrome::kChromeUINewTabURL);
  std::unique_ptr<ProcessDiceHeaderDelegateImpl> delegate =
      CreateDelegateAndNavigateToSignin(GetParam().signin_tab,
                                        /*redirect_url=*/kNtpUrl);
  delegate->EnableSync(account_info_);
  EXPECT_EQ(GetParam().callback_called, enable_sync_called_);
  GURL expected_url = GetParam().show_ntp ? kNtpUrl : signin_url_;
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
  if (GetParam().signed_in) {
    AddAccount(/*is_primary=*/true);
  }
  const GURL kNtpUrl(chrome::kChromeUINewTabURL);
  std::unique_ptr<ProcessDiceHeaderDelegateImpl> delegate =
      CreateDelegateAndNavigateToSignin(GetParam().signin_tab,
                                        /*redirect_url=*/kNtpUrl);
  delegate->HandleTokenExchangeFailure(email_, auth_error_);
  EXPECT_FALSE(enable_sync_called_);
  EXPECT_EQ(GetParam().callback_called, show_error_called_);
  GURL expected_url = GetParam().show_ntp ? kNtpUrl : signin_url_;
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
  bool is_reauth = false;   // User was already signed in with the account.
  bool signin_tab = false;  // A DiceTabHelper is attached to the tab.
  Reason reason = Reason::kSigninPrimaryAccount;
  // Expected value for the MaybeInterceptWebSigin call.
  bool sync_signin = false;
  signin_metrics::AccessPoint access_point =
      signin_metrics::AccessPoint::ACCESS_POINT_UNKNOWN;
};

TokenExchangeSuccessConfiguration kHandleTokenExchangeSuccessTestCases[] = {
    // clang-format off
    // is_reauth | signin_tab |       reason               |
    //      sync_signin  | access_point
    {  false,      false,     Reason::kSigninPrimaryAccount,
            false, signin_metrics::AccessPoint::ACCESS_POINT_WEB_SIGNIN },
    {  false,      true,      Reason::kSigninPrimaryAccount,
            true, signin_metrics::AccessPoint::ACCESS_POINT_BOOKMARK_BUBBLE },
    {  false,      true,      Reason::kAddSecondaryAccount,
            false, signin_metrics::AccessPoint::ACCESS_POINT_BOOKMARK_BUBBLE },
    {  true,       false,     Reason::kSigninPrimaryAccount,
            false, signin_metrics::AccessPoint::ACCESS_POINT_WEB_SIGNIN },
    {  true,       true,      Reason::kSigninPrimaryAccount,
            true, signin_metrics::AccessPoint::ACCESS_POINT_BOOKMARK_BUBBLE },

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
  if (GetParam().is_reauth) {
    AddAccount(/*is_primary=*/false);
  }
  std::unique_ptr<ProcessDiceHeaderDelegateImpl> delegate =
      CreateDelegateAndNavigateToSignin(
          GetParam().signin_tab,
          /*redirect_url=*/GURL(chrome::kChromeUINewTabURL), GetParam().reason);

  EXPECT_CALL(
      *mock_interceptor(),
      MaybeInterceptWebSignin(web_contents(), account_info_.account_id,
                              GetParam().access_point, !GetParam().is_reauth,
                              GetParam().sync_signin));
  delegate->HandleTokenExchangeSuccess(account_info_.account_id,
                                       !GetParam().is_reauth);

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
