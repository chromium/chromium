// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/dice_web_signin_interceptor.h"

#include <memory>
#include <optional>
#include <tuple>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "build/buildflag.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/enterprise/browser_management/management_service_factory.h"
#include "chrome/browser/profiles/profile_attributes_entry.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/signin/chrome_signin_client_factory.h"
#include "chrome/browser/signin/chrome_signin_client_test_util.h"
#include "chrome/browser/signin/identity_test_environment_profile_adaptor.h"
#include "chrome/browser/signin/web_signin_interceptor.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/policy/core/browser/signin/profile_separation_policies.h"
#include "components/policy/core/common/management/scoped_management_service_override_for_testing.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/base/signin_metrics.h"
#include "components/signin/public/base/signin_pref_names.h"
#include "components/signin/public/base/signin_prefs.h"
#include "components/signin/public/base/signin_switches.h"
#include "components/signin/public/identity_manager/account_capabilities_test_mutator.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/signin/public/identity_manager/tribool.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace {

class MockDiceWebSigninInterceptorDelegate
    : public WebSigninInterceptor::Delegate {
 public:
  bool IsSigninInterceptionSupported(
      const content::WebContents& web_contents) override {
    return true;
  }

  MOCK_METHOD(std::unique_ptr<ScopedWebSigninInterceptionBubbleHandle>,
              ShowSigninInterceptionBubble,
              (content::WebContents * web_contents,
               const WebSigninInterceptor::Delegate::BubbleParameters&
                   bubble_parameters,
               base::OnceCallback<void(SigninInterceptionResult)> callback),
              (override));
  MOCK_METHOD(std::unique_ptr<ScopedWebSigninInterceptionBubbleHandle>,
              ShowOidcInterceptionDialog,
              (content::WebContents*,
               const WebSigninInterceptor::Delegate::BubbleParameters&,
               signin::SigninChoiceWithConfirmationCallback,
               base::OnceClosure,
               base::OnceClosure),
              (override));
  void ShowFirstRunExperienceInNewProfile(
      Browser* browser,
      const CoreAccountId& account_id,
      WebSigninInterceptor::SigninInterceptionType interception_type) override {
  }
};

MATCHER_P(HasSameAccountIdAs, other, "") {
  return arg.account_id == other.account_id;
}

// Matches BubbleParameters fields excepting the color. This is useful in the
// test because the color is randomly generated.
testing::Matcher<const WebSigninInterceptor::Delegate::BubbleParameters&>
MatchBubbleParameters(
    const WebSigninInterceptor::Delegate::BubbleParameters& parameters) {
  return testing::AllOf(
      testing::Field(
          "interception_type",
          &WebSigninInterceptor::Delegate::BubbleParameters::interception_type,
          parameters.interception_type),
      testing::Field("intercepted_account",
                     &WebSigninInterceptor::Delegate::BubbleParameters::
                         intercepted_account,
                     HasSameAccountIdAs(parameters.intercepted_account)),
      testing::Field(
          "primary_account",
          &WebSigninInterceptor::Delegate::BubbleParameters::primary_account,
          HasSameAccountIdAs(parameters.primary_account)),
      testing::Field("show_link_data_option",
                     &WebSigninInterceptor::Delegate::BubbleParameters::
                         show_link_data_option,
                     parameters.show_link_data_option),
      testing::Field("show_managed_disclaimer",
                     &WebSigninInterceptor::Delegate::BubbleParameters::
                         show_managed_disclaimer,
                     parameters.show_managed_disclaimer));
}

void MakeValidAccountCapabilities(AccountInfo* info) {
  AccountCapabilitiesTestMutator mutator(&info->capabilities);
  mutator.set_is_subject_to_parental_controls(true);
}

void MakeValidAccountInfoWithoutCapabilities(
    AccountInfo* info,
    const std::string& hosted_domain = kNoHostedDomainFound) {
  if (info->IsValid()) {
    return;
  }
  info->full_name = "fullname";
  info->given_name = "givenname";
  info->hosted_domain = hosted_domain;
  info->locale = "en";
  info->picture_url = "https://example.com";
  DCHECK(info->IsValid());
}

// If the account info is valid, does nothing. Otherwise fills the extended
// fields with default values.
void MakeValidAccountInfo(
    AccountInfo* info,
    const std::string& hosted_domain = kNoHostedDomainFound) {
  if (info->IsValid()) {
    return;
  }
  MakeValidAccountInfoWithoutCapabilities(info, hosted_domain);
  MakeValidAccountCapabilities(info);
}

std::string ParamToTestSuffixForInterceptionAndSyncPromo(
    const ::testing::TestParamInfo<bool> info) {
  bool interception_enabled = info.param;
  return interception_enabled ? "Intercept" : "NoIntercept";
}

}  // namespace

class DiceWebSigninInterceptorTest : public BrowserWithTestWindowTest {
 public:
  DiceWebSigninInterceptorTest()
      : BrowserWithTestWindowTest(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}
  ~DiceWebSigninInterceptorTest() override = default;

  DiceWebSigninInterceptor* interceptor() {
    return dice_web_signin_interceptor_.get();
  }

  MockDiceWebSigninInterceptorDelegate* mock_delegate() {
    return mock_delegate_;
  }

  content::WebContents* web_contents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

  ProfileAttributesStorage* profile_attributes_storage() {
    return profile_manager()->profile_attributes_storage();
  }

  signin::IdentityTestEnvironment* identity_test_env() {
    return identity_test_env_profile_adaptor_->identity_test_env();
  }

  Profile* CreateTestingProfile(const std::string& name) {
    return profile_manager()->CreateTestingProfile(name);
  }

  // Helper function that calls MaybeInterceptWebSignin with parameters
  // compatible with interception.
  void MaybeIntercept(CoreAccountId account_id) {
    interceptor()->MaybeInterceptWebSignin(
        web_contents(), account_id,
        signin_metrics::AccessPoint::ACCESS_POINT_WEB_SIGNIN,
        /*is_new_account=*/true,
        /*is_sync_signin=*/false);
  }

  // Calls MaybeInterceptWebSignin and verifies the heuristic outcome, the
  // histograms and whether the interception is in progress.
  // This function only works if the interception decision can be made
  // synchronously (GetHeuristicOutcome() returns a value).
  void TestSynchronousInterception(
      AccountInfo account_info,
      bool is_new_account,
      bool is_sync_signin,
      SigninInterceptionHeuristicOutcome expected_outcome) {
    ASSERT_EQ(interceptor()->GetHeuristicOutcome(is_new_account, is_sync_signin,
                                                 account_info.email),
              expected_outcome);
    base::HistogramTester histogram_tester;
    interceptor()->MaybeInterceptWebSignin(
        web_contents(), account_info.account_id,
        signin_metrics::AccessPoint::ACCESS_POINT_WEB_SIGNIN, is_new_account,
        is_sync_signin);
    testing::Mock::VerifyAndClearExpectations(mock_delegate());
    histogram_tester.ExpectUniqueSample("Signin.Intercept.HeuristicOutcome",
                                        expected_outcome, 1);
    histogram_tester.ExpectUniqueTimeSample("Signin.Intercept.HeuristicLatency",
                                            base::Milliseconds(0), 1);

    EXPECT_EQ(interceptor()->is_interception_in_progress(),
              SigninInterceptionHeuristicOutcomeIsSuccess(expected_outcome));
  }

  // Calls MaybeInterceptWebSignin and verifies the heuristic outcome and the
  // histograms.
  // This function only works if the interception decision cannot be made
  // synchronously (GetHeuristicOutcome() returns no value).
  void TestAsynchronousInterception(
      AccountInfo account_info,
      bool is_new_account,
      bool is_sync_signin,
      SigninInterceptionHeuristicOutcome expected_outcome) {
    ASSERT_EQ(interceptor()->GetHeuristicOutcome(is_new_account, is_sync_signin,
                                                 account_info.email),
              std::nullopt);
    base::HistogramTester histogram_tester;
    interceptor()->MaybeInterceptWebSignin(
        web_contents(), account_info.account_id,
        signin_metrics::AccessPoint::ACCESS_POINT_WEB_SIGNIN, is_new_account,
        is_sync_signin);
    testing::Mock::VerifyAndClearExpectations(mock_delegate());
    histogram_tester.ExpectUniqueSample("Signin.Intercept.HeuristicOutcome",
                                        expected_outcome, 1);
    histogram_tester.ExpectUniqueTimeSample("Signin.Intercept.HeuristicLatency",
                                            base::Milliseconds(0), 1);
    EXPECT_EQ(interceptor()->is_interception_in_progress(),
              SigninInterceptionHeuristicOutcomeIsSuccess(expected_outcome));
  }

 protected:
  // testing::Test:
  void SetUp() override {
    BrowserWithTestWindowTest::SetUp();

    identity_test_env_profile_adaptor_ =
        std::make_unique<IdentityTestEnvironmentProfileAdaptor>(profile());
    identity_test_env_profile_adaptor_->identity_test_env()
        ->SetTestURLLoaderFactory(&test_url_loader_factory_);

    auto delegate = std::make_unique<
        testing::StrictMock<MockDiceWebSigninInterceptorDelegate>>();
    mock_delegate_ = delegate.get();
    dice_web_signin_interceptor_ = std::make_unique<DiceWebSigninInterceptor>(
        profile(), std::move(delegate));

    // Create the first tab so that web_contents() exists.
    AddTab(browser(), GURL("http://foo/1"));
  }

 private:
  void TearDown() override {
    dice_web_signin_interceptor_->Shutdown();
    identity_test_env_profile_adaptor_.reset();
    BrowserWithTestWindowTest::TearDown();
  }

  TestingProfile::TestingFactories GetTestingFactories() override {
    TestingProfile::TestingFactories factories =
        IdentityTestEnvironmentProfileAdaptor::
            GetIdentityTestEnvironmentFactories();
    factories.push_back(
        {ChromeSigninClientFactory::GetInstance(),
         base::BindRepeating(&BuildChromeSigninClientWithURLLoader,
                             &test_url_loader_factory_)});
    return factories;
  }

  // Force local machine to be unmanaged, so that variations in try bots and
  // developer machines don't affect the tests. See https://crbug.com/1445255.
  policy::ScopedManagementServiceOverrideForTesting platform_browser_mgmt_ = {
      policy::ManagementServiceFactory::GetForPlatform(),
      policy::EnterpriseManagementAuthority::NONE};
  network::TestURLLoaderFactory test_url_loader_factory_;
  std::unique_ptr<IdentityTestEnvironmentProfileAdaptor>
      identity_test_env_profile_adaptor_;
  std::unique_ptr<DiceWebSigninInterceptor> dice_web_signin_interceptor_;
  raw_ptr<MockDiceWebSigninInterceptorDelegate> mock_delegate_ = nullptr;
};

TEST_F(DiceWebSigninInterceptorTest, ShouldShowProfileSwitchBubble) {
  AccountInfo account_info =
      identity_test_env()->MakeAccountAvailable("bob@example.com");
  const std::string& email = account_info.email;
  EXPECT_FALSE(interceptor()->ShouldShowProfileSwitchBubble(
      email, profile_attributes_storage()));

  // Add another profile with no account.
  CreateTestingProfile("Profile 1");
  EXPECT_FALSE(interceptor()->ShouldShowProfileSwitchBubble(
      email, profile_attributes_storage()));

  // Add another profile with a different account.
  Profile* profile_2 = CreateTestingProfile("Profile 2");
  ProfileAttributesEntry* entry =
      profile_attributes_storage()->GetProfileAttributesWithPath(
          profile_2->GetPath());
  ASSERT_NE(entry, nullptr);
  std::string kOtherGaiaID = "SomeOtherGaiaID";
  ASSERT_NE(kOtherGaiaID, account_info.gaia);
  entry->SetAuthInfo(kOtherGaiaID, u"alice@gmail.com",
                     /*is_consented_primary_account=*/true);
  EXPECT_FALSE(interceptor()->ShouldShowProfileSwitchBubble(
      email, profile_attributes_storage()));

  // Change the account to match.
  entry->SetAuthInfo(account_info.gaia, base::UTF8ToUTF16(email),
                     /*is_consented_primary_account=*/false);
  const ProfileAttributesEntry* switch_to_entry =
      interceptor()->ShouldShowProfileSwitchBubble(
          email, profile_attributes_storage());
  EXPECT_EQ(entry, switch_to_entry);
}

TEST_F(DiceWebSigninInterceptorTest, NoBubbleWithSingleAccount) {
  AccountInfo account_info =
      identity_test_env()->MakeAccountAvailable("bob@example.com");
  MakeValidAccountInfo(&account_info, "example.com");
  identity_test_env()->UpdateAccountInfoForAccount(account_info);

  // Without Primary account.
  EXPECT_FALSE(interceptor()->ShouldShowEnterpriseBubble(account_info));

  // With UPA.
  identity_test_env()->SetPrimaryAccount("bob@example.com",
                                         signin::ConsentLevel::kSignin);
  EXPECT_FALSE(interceptor()->ShouldShowEnterpriseBubble(account_info));
}

TEST_F(DiceWebSigninInterceptorTest, ShouldShowEnterpriseBubble) {
  // Setup 3 accounts in the profile:
  // - primary account
  // - other enterprise account that is not primary (should be ignored)
  // - intercepted account.
  AccountInfo primary_account_info =
      identity_test_env()->MakePrimaryAccountAvailable(
          "alice@example.com", signin::ConsentLevel::kSignin);
  AccountInfo other_account_info =
      identity_test_env()->MakeAccountAvailable("dummy@example.com");
  MakeValidAccountInfo(&other_account_info);
  other_account_info.hosted_domain = "example.com";
  identity_test_env()->UpdateAccountInfoForAccount(other_account_info);
  AccountInfo account_info =
      identity_test_env()->MakeAccountAvailable("bob@example.com");
  MakeValidAccountInfo(&account_info);
  identity_test_env()->UpdateAccountInfoForAccount(account_info);
  ASSERT_EQ(identity_test_env()->identity_manager()->GetPrimaryAccountId(
                signin::ConsentLevel::kSignin),
            primary_account_info.account_id);

  // The primary account does not have full account info (empty domain).
  ASSERT_TRUE(identity_test_env()
                  ->identity_manager()
                  ->FindExtendedAccountInfo(primary_account_info)
                  .hosted_domain.empty());
  EXPECT_FALSE(interceptor()->ShouldShowEnterpriseBubble(account_info));
  account_info.hosted_domain = "example.com";
  identity_test_env()->UpdateAccountInfoForAccount(account_info);
  EXPECT_TRUE(interceptor()->ShouldShowEnterpriseBubble(account_info));

  // The primary account has full info.
  MakeValidAccountInfo(&primary_account_info);
  identity_test_env()->UpdateAccountInfoForAccount(primary_account_info);
  // The intercepted account is enterprise.
  EXPECT_TRUE(interceptor()->ShouldShowEnterpriseBubble(account_info));
  // Two consummer accounts.
  account_info.hosted_domain = kNoHostedDomainFound;
  identity_test_env()->UpdateAccountInfoForAccount(account_info);
  EXPECT_FALSE(interceptor()->ShouldShowEnterpriseBubble(account_info));
  // The primary account is enterprise.
  primary_account_info.hosted_domain = "example.com";
  identity_test_env()->UpdateAccountInfoForAccount(primary_account_info);
  EXPECT_TRUE(interceptor()->ShouldShowEnterpriseBubble(account_info));
}

TEST_F(DiceWebSigninInterceptorTest, ShouldEnforceEnterpriseProfileSeparation) {
  profile()->GetPrefs()->SetBoolean(
      prefs::kManagedAccountsSigninRestrictionScopeMachine, true);
  profile()->GetPrefs()->SetString(prefs::kManagedAccountsSigninRestriction,
                                   "primary_account_strict");

  // Setup 3 accounts in the profile:
  // - primary account
  // - other enterprise account that is not primary (should be ignored)
  // - intercepted account.
  AccountInfo primary_account_info =
      identity_test_env()->MakePrimaryAccountAvailable(
          "alice@gmail.com", signin::ConsentLevel::kSignin);

  AccountInfo other_account_info =
      identity_test_env()->MakeAccountAvailable("dummy@example.com");
  MakeValidAccountInfo(&other_account_info);
  other_account_info.hosted_domain = "example.com";
  identity_test_env()->UpdateAccountInfoForAccount(other_account_info);
  AccountInfo account_info =
      identity_test_env()->MakeAccountAvailable("bob@example.com");
  MakeValidAccountInfo(&account_info);
  identity_test_env()->UpdateAccountInfoForAccount(account_info);
  ASSERT_EQ(identity_test_env()->identity_manager()->GetPrimaryAccountId(
                signin::ConsentLevel::kSignin),
            primary_account_info.account_id);
  interceptor()->state_->new_account_interception_ = true;
  // Consumer account not intercepted.
  EXPECT_FALSE(
      interceptor()->ShouldEnforceEnterpriseProfileSeparation(account_info));
  account_info.hosted_domain = "example.com";
  identity_test_env()->UpdateAccountInfoForAccount(account_info);
  // Managed account intercepted.
  EXPECT_TRUE(
      interceptor()->ShouldEnforceEnterpriseProfileSeparation(account_info));
}

TEST_F(DiceWebSigninInterceptorTest,
       ShouldEnforceEnterpriseProfileSeparationWithoutUPA) {
  profile()->GetPrefs()->SetBoolean(
      prefs::kManagedAccountsSigninRestrictionScopeMachine, true);
  profile()->GetPrefs()->SetString(prefs::kManagedAccountsSigninRestriction,
                                   "primary_account_strict");
  AccountInfo account_info_1 =
      identity_test_env()->MakeAccountAvailable("bob@example.com");
  MakeValidAccountInfo(&account_info_1);
  account_info_1.hosted_domain = "example.com";
  identity_test_env()->UpdateAccountInfoForAccount(account_info_1);

  interceptor()->state_->new_account_interception_ = true;
  // Primary account is not set.
  ASSERT_FALSE(identity_test_env()->identity_manager()->HasPrimaryAccount(
      signin::ConsentLevel::kSignin));
  EXPECT_TRUE(
      interceptor()->ShouldEnforceEnterpriseProfileSeparation(account_info_1));
}

TEST_F(DiceWebSigninInterceptorTest,
       ShouldEnforceEnterpriseProfileSeparationReauth) {
  profile()->GetPrefs()->SetString(prefs::kManagedAccountsSigninRestriction,
                                   "primary_account_strict");
  AccountInfo primary_account_info =
      identity_test_env()->MakePrimaryAccountAvailable(
          "alice@example.com", signin::ConsentLevel::kSignin);
  MakeValidAccountInfo(&primary_account_info);
  primary_account_info.hosted_domain = "example.com";
  identity_test_env()->UpdateAccountInfoForAccount(primary_account_info);

  // Primary account is set.
  ASSERT_TRUE(identity_test_env()->identity_manager()->HasPrimaryAccount(
      signin::ConsentLevel::kSignin));
  ASSERT_TRUE(primary_account_info.IsManaged());
  EXPECT_TRUE(interceptor()->ShouldEnforceEnterpriseProfileSeparation(
      primary_account_info));

  ProfileAttributesEntry* entry =
      profile_attributes_storage()->GetProfileAttributesWithPath(
          profile()->GetPath());
  entry->SetUserAcceptedAccountManagement(true);

  EXPECT_FALSE(interceptor()->ShouldEnforceEnterpriseProfileSeparation(
      primary_account_info));
}

class DiceWebSigninInterceptorManagedAccountTest
    : public DiceWebSigninInterceptorTest,
      public testing::WithParamInterface<bool> {
 public:
  DiceWebSigninInterceptorManagedAccountTest()
      : signin_interception_enabled_(GetParam()) {}

 protected:
  void SetUp() override {
    DiceWebSigninInterceptorTest::SetUp();
    profile()->GetPrefs()->SetBoolean(prefs::kSigninInterceptionEnabled,
                                      signin_interception_enabled_);
  }

  bool signin_interception_enabled_;
};

TEST_P(DiceWebSigninInterceptorManagedAccountTest,
       NoForcedInterceptionShowsDialogIfFeatureEnabled) {
  base::test::ScopedFeatureList scoped_list;
  scoped_list.InitAndEnableFeature(
      features::kEnterpriseUpdatedProfileCreationScreen);
  // Reauth intercepted if enterprise confirmation not shown yet for forced
  // managed separation.
  AccountInfo account_info = identity_test_env()->MakePrimaryAccountAvailable(
      "alice@example.com", signin::ConsentLevel::kSignin);
  MakeValidAccountInfo(&account_info, "example.com");
  identity_test_env()->UpdateAccountInfoForAccount(account_info);
  interceptor()->SetInterceptedAccountProfileSeparationPoliciesForTesting(
      policy::ProfileSeparationPolicies(""));

  WebSigninInterceptor::Delegate::BubbleParameters expected_parameters(
      WebSigninInterceptor::SigninInterceptionType::kEnterpriseAcceptManagement,
      account_info, account_info, SkColor(),
      /*show_link_data_option=*/true, /*show_managed_disclaimer=*/true);
  EXPECT_CALL(*mock_delegate(),
              ShowSigninInterceptionBubble(
                  web_contents(), MatchBubbleParameters(expected_parameters),
                  testing::_));
  TestAsynchronousInterception(
      account_info, /*is_new_account=*/true, /*is_sync_signin=*/false,
      SigninInterceptionHeuristicOutcome::kInterceptEnterprise);
}

TEST_P(
    DiceWebSigninInterceptorManagedAccountTest,
    NoForcedInterceptionShowsNoDialogIfFeatureEnabledButDisabledDialogByPolicy) {
  base::test::ScopedFeatureList scoped_list;
  scoped_list.InitAndEnableFeature(
      features::kEnterpriseUpdatedProfileCreationScreen);
  // Reauth intercepted if enterprise confirmation not shown yet for forced
  // managed separation.
  AccountInfo account_info = identity_test_env()->MakePrimaryAccountAvailable(
      "alice@example.com", signin::ConsentLevel::kSignin);
  MakeValidAccountInfo(&account_info, "example.com");
  identity_test_env()->UpdateAccountInfoForAccount(account_info);
  interceptor()->SetInterceptedAccountProfileSeparationPoliciesForTesting(
      policy::ProfileSeparationPolicies(
          policy::ProfileSeparationSettings::DISABLED, std::nullopt));

  if (signin_interception_enabled_) {
    TestAsynchronousInterception(
        account_info, /*is_new_account=*/true, /*is_sync_signin=*/false,
        SigninInterceptionHeuristicOutcome::kAbortAccountInfoNotCompatible);
  } else {
    TestAsynchronousInterception(
        account_info, /*is_new_account=*/true, /*is_sync_signin=*/false,
        SigninInterceptionHeuristicOutcome::kAbortInterceptionDisabled);
  }
}

TEST_P(DiceWebSigninInterceptorManagedAccountTest,
       NoForcedInterceptionShowsNoBubble) {
  // Reauth intercepted if enterprise confirmation not shown yet for forced
  // managed separation.
  AccountInfo account_info = identity_test_env()->MakePrimaryAccountAvailable(
      "alice@example.com", signin::ConsentLevel::kSignin);
  MakeValidAccountInfo(&account_info, "example.com");
  identity_test_env()->UpdateAccountInfoForAccount(account_info);
  interceptor()->SetInterceptedAccountProfileSeparationPoliciesForTesting(
      policy::ProfileSeparationPolicies(""));

  if (signin_interception_enabled_) {
    TestAsynchronousInterception(
        account_info, /*is_new_account=*/true, /*is_sync_signin=*/false,
        SigninInterceptionHeuristicOutcome::kAbortAccountInfoNotCompatible);
  } else {
    TestAsynchronousInterception(
        account_info, /*is_new_account=*/true, /*is_sync_signin=*/false,
        SigninInterceptionHeuristicOutcome::kAbortInterceptionDisabled);
  }
}

TEST_P(DiceWebSigninInterceptorManagedAccountTest,
       EnforceManagedAccountAsPrimaryReauth) {
  profile()->GetPrefs()->SetBoolean(
      prefs::kManagedAccountsSigninRestrictionScopeMachine, true);
  profile()->GetPrefs()->SetString(prefs::kManagedAccountsSigninRestriction,
                                   "primary_account");

  // Reauth intercepted if enterprise confirmation not shown yet for forced
  // managed separation.
  AccountInfo account_info = identity_test_env()->MakePrimaryAccountAvailable(
      "alice@example.com", signin::ConsentLevel::kSignin);
  MakeValidAccountInfo(&account_info, "example.com");
  identity_test_env()->UpdateAccountInfoForAccount(account_info);
  profile()->GetPrefs()->SetString(prefs::kManagedAccountsSigninRestriction,
                                   "primary_account");

  // Check that interception works otherwise, as a sanity check.
  WebSigninInterceptor::Delegate::BubbleParameters expected_parameters(
      WebSigninInterceptor::SigninInterceptionType::kEnterpriseForced,
      account_info, account_info, SkColor(),
      /*show_link_data_option=*/false, /*show_managed_disclaimer=*/true);
  EXPECT_CALL(*mock_delegate(),
              ShowSigninInterceptionBubble(
                  web_contents(), MatchBubbleParameters(expected_parameters),
                  testing::_));

  TestSynchronousInterception(
      account_info, /*is_new_account=*/false, /*is_sync_signin=*/false,
      SigninInterceptionHeuristicOutcome::kInterceptEnterpriseForced);
}

TEST_P(DiceWebSigninInterceptorManagedAccountTest,
       EnforceManagedAccountAsPrimaryManaged) {
  AccountInfo account_info =
      identity_test_env()->MakeAccountAvailable("alice@example.com");
  MakeValidAccountInfo(&account_info, "example.com");
  identity_test_env()->UpdateAccountInfoForAccount(account_info);

  profile()->GetPrefs()->SetString(prefs::kManagedAccountsSigninRestriction,
                                   "primary_account_strict");

  // Check that interception works otherwise, as a sanity check.
  WebSigninInterceptor::Delegate::BubbleParameters expected_parameters(
      WebSigninInterceptor::SigninInterceptionType::kEnterpriseForced,
      account_info, AccountInfo(), SkColor(),
      /*show_link_data_option=*/false, /*show_managed_disclaimer=*/true);
  EXPECT_CALL(*mock_delegate(),
              ShowSigninInterceptionBubble(
                  web_contents(), MatchBubbleParameters(expected_parameters),
                  testing::_));
  TestSynchronousInterception(
      account_info, /*is_new_account=*/true, /*is_sync_signin=*/false,
      SigninInterceptionHeuristicOutcome::kInterceptEnterpriseForced);
}

TEST_P(DiceWebSigninInterceptorManagedAccountTest,
       EnforceManagedAccountAsPrimaryManagedLinkData) {
  AccountInfo account_info =
      identity_test_env()->MakeAccountAvailable("alice@example.com");
  MakeValidAccountInfo(&account_info, "example.com");
  identity_test_env()->UpdateAccountInfoForAccount(account_info);

  interceptor()->SetInterceptedAccountProfileSeparationPoliciesForTesting(
      policy::ProfileSeparationPolicies("primary_account_keep_existing_data"));

  // Check that interception works otherwise, as a sanity check.
  WebSigninInterceptor::Delegate::BubbleParameters expected_parameters(
      WebSigninInterceptor::SigninInterceptionType::kEnterpriseForced,
      account_info, AccountInfo(), SkColor(),
      /*show_link_data_option=*/true, /*show_managed_disclaimer=*/true);
  EXPECT_CALL(*mock_delegate(),
              ShowSigninInterceptionBubble(
                  web_contents(), MatchBubbleParameters(expected_parameters),
                  testing::_));
  TestAsynchronousInterception(
      account_info, /*is_new_account=*/true, /*is_sync_signin=*/false,
      SigninInterceptionHeuristicOutcome::kInterceptEnterpriseForced);
}

TEST_P(DiceWebSigninInterceptorManagedAccountTest,
       EnforceManagedAccountAsPrimaryManagedLinkDataSecondaryAccount) {
  AccountInfo primary_account_info =
      identity_test_env()->MakePrimaryAccountAvailable(
          "alice@example.com", signin::ConsentLevel::kSignin);
  MakeValidAccountInfo(&primary_account_info, "example.com");
  identity_test_env()->UpdateAccountInfoForAccount(primary_account_info);

  std::string email = "bob@example.com";
  AccountInfo account_info = identity_test_env()->MakeAccountAvailable(email);
  MakeValidAccountInfo(&account_info, "example.com");
  identity_test_env()->UpdateAccountInfoForAccount(account_info);

  profile()->GetPrefs()->SetBoolean(
      prefs::kManagedAccountsSigninRestrictionScopeMachine, true);
  profile()->GetPrefs()->SetString(prefs::kManagedAccountsSigninRestriction,
                                   "primary_account_keep_existing_data");

  // Check that interception works otherwise, as a sanity check.
  WebSigninInterceptor::Delegate::BubbleParameters expected_parameters(
      WebSigninInterceptor::SigninInterceptionType::kEnterpriseForced,
      account_info, primary_account_info, SkColor(),
      /*show_link_data_option=*/false, /*show_managed_disclaimer=*/true);
  EXPECT_CALL(*mock_delegate(),
              ShowSigninInterceptionBubble(
                  web_contents(), MatchBubbleParameters(expected_parameters),
                  testing::_));
  TestSynchronousInterception(
      account_info, /*is_new_account=*/true, /*is_sync_signin=*/false,
      SigninInterceptionHeuristicOutcome::kInterceptEnterpriseForced);
}

TEST_P(DiceWebSigninInterceptorManagedAccountTest,
       EnforceManagedAccountAsPrimaryManagedStrictLinkData) {
  AccountInfo account_info =
      identity_test_env()->MakeAccountAvailable("alice@example.com");
  MakeValidAccountInfo(&account_info, "example.com");
  identity_test_env()->UpdateAccountInfoForAccount(account_info);

  profile()->GetPrefs()->SetString(prefs::kManagedAccountsSigninRestriction,
                                   "primary_account_strict_keep_existing_data");

  // Check that interception works otherwise, as a sanity check.
  WebSigninInterceptor::Delegate::BubbleParameters expected_parameters(
      WebSigninInterceptor::SigninInterceptionType::kEnterpriseForced,
      account_info, AccountInfo(), SkColor(),
      /*show_link_data_option=*/true, /*show_managed_disclaimer=*/true);
  EXPECT_CALL(*mock_delegate(),
              ShowSigninInterceptionBubble(
                  web_contents(), MatchBubbleParameters(expected_parameters),
                  testing::_));
  TestSynchronousInterception(
      account_info, /*is_new_account=*/true, /*is_sync_signin=*/false,
      SigninInterceptionHeuristicOutcome::kInterceptEnterpriseForced);
}

TEST_P(DiceWebSigninInterceptorManagedAccountTest,
       EnforceManagedAccountAsPrimaryManagedStrictLinkDataSecondaryAccount) {
  AccountInfo primary_account_info =
      identity_test_env()->MakePrimaryAccountAvailable(
          "alice@example.com", signin::ConsentLevel::kSignin);
  MakeValidAccountInfo(&primary_account_info, "example.com");
  identity_test_env()->UpdateAccountInfoForAccount(primary_account_info);

  std::string email = "bob@example.com";
  AccountInfo account_info = identity_test_env()->MakeAccountAvailable(email);
  MakeValidAccountInfo(&account_info, "example.com");
  identity_test_env()->UpdateAccountInfoForAccount(account_info);

  profile()->GetPrefs()->SetString(prefs::kManagedAccountsSigninRestriction,
                                   "primary_account_strict_keep_existing_data");

  // Check that interception works otherwise, as a sanity check.
  WebSigninInterceptor::Delegate::BubbleParameters expected_parameters(
      WebSigninInterceptor::SigninInterceptionType::kEnterpriseForced,
      account_info, primary_account_info, SkColor(),
      /*show_link_data_option=*/false, /*show_managed_disclaimer=*/true);
  EXPECT_CALL(*mock_delegate(),
              ShowSigninInterceptionBubble(
                  web_contents(), MatchBubbleParameters(expected_parameters),
                  testing::_));
  TestSynchronousInterception(
      account_info, /*is_new_account=*/true, /*is_sync_signin=*/false,
      SigninInterceptionHeuristicOutcome::kInterceptEnterpriseForced);
}

TEST_P(DiceWebSigninInterceptorManagedAccountTest,
       EnforceManagedAccountAsPrimaryProfileSwitch) {
  AccountInfo account_info =
      identity_test_env()->MakeAccountAvailable("alice@example.com");
  MakeValidAccountInfo(&account_info, "example.com");
  identity_test_env()->UpdateAccountInfoForAccount(account_info);

  profile()->GetPrefs()->SetBoolean(
      prefs::kManagedAccountsSigninRestrictionScopeMachine, true);
  profile()->GetPrefs()->SetString(prefs::kManagedAccountsSigninRestriction,
                                   "primary_account_strict");

  // Setup for profile switch interception.
  Profile* profile_2 = CreateTestingProfile("Profile 2");
  ProfileAttributesEntry* entry =
      profile_attributes_storage()->GetProfileAttributesWithPath(
          profile_2->GetPath());
  ASSERT_NE(entry, nullptr);
  entry->SetAuthInfo(account_info.gaia, base::UTF8ToUTF16(account_info.email),
                     /*is_consented_primary_account=*/false);
  // Check that interception works otherwise, as a sanity check.
  WebSigninInterceptor::Delegate::BubbleParameters expected_parameters(
      WebSigninInterceptor::SigninInterceptionType::kProfileSwitchForced,
      account_info, AccountInfo(), SkColor(),
      /*show_link_data_option=*/false, /*show_managed_disclaimer=*/true);
  EXPECT_CALL(*mock_delegate(),
              ShowSigninInterceptionBubble(
                  web_contents(), MatchBubbleParameters(expected_parameters),
                  testing::_));
  TestSynchronousInterception(account_info, /*is_new_account=*/true,
                              /*is_sync_signin=*/false,
                              SigninInterceptionHeuristicOutcome::
                                  kInterceptEnterpriseForcedProfileSwitch);
}

TEST_P(DiceWebSigninInterceptorManagedAccountTest,
       EnforceManagedAccountSecondaryAccountNotAllowed) {
  AccountInfo primary_account_info =
      identity_test_env()->MakePrimaryAccountAvailable(
          "alice@example.com", signin::ConsentLevel::kSignin);
  MakeValidAccountInfo(&primary_account_info, "example.com");
  identity_test_env()->UpdateAccountInfoForAccount(primary_account_info);

  std::string email = "bob@example.com";
  AccountInfo account_info = identity_test_env()->MakeAccountAvailable(email);
  MakeValidAccountInfo(&account_info, "example.com");
  identity_test_env()->UpdateAccountInfoForAccount(account_info);

  base::Value::List profile_separation_exception_list;
  profile_separation_exception_list.Append(base::Value("notexample.com"));
  profile()->GetPrefs()->SetList(prefs::kProfileSeparationDomainExceptionList,
                                 std::move(profile_separation_exception_list));

  // Check that interception works otherwise, as a sanity check.
  WebSigninInterceptor::Delegate::BubbleParameters expected_parameters(
      WebSigninInterceptor::SigninInterceptionType::kEnterpriseForced,
      account_info, primary_account_info, SkColor(),
      /*show_link_data_option=*/false, /*show_managed_disclaimer=*/true);
  EXPECT_CALL(*mock_delegate(),
              ShowSigninInterceptionBubble(
                  web_contents(), MatchBubbleParameters(expected_parameters),
                  testing::_));
  TestSynchronousInterception(
      account_info, /*is_new_account=*/true, /*is_sync_signin=*/false,
      SigninInterceptionHeuristicOutcome::kInterceptEnterpriseForced);
}

TEST_P(DiceWebSigninInterceptorManagedAccountTest,
       EnforceManagedAccountSecondaryAccountAllowedReauth) {
  AccountInfo primary_account_info =
      identity_test_env()->MakePrimaryAccountAvailable(
          "alice@example.com", signin::ConsentLevel::kSignin);
  MakeValidAccountInfo(&primary_account_info, "example.com");
  identity_test_env()->UpdateAccountInfoForAccount(primary_account_info);

  base::Value::List profile_separation_exception_list;
  profile_separation_exception_list.Append(base::Value("notexample.com"));
  profile()->GetPrefs()->SetList(prefs::kProfileSeparationDomainExceptionList,
                                 std::move(profile_separation_exception_list));

  TestSynchronousInterception(
      primary_account_info, /*is_new_account=*/false, /*is_sync_signin=*/false,
      profile()->GetPrefs()->GetBoolean(prefs::kSigninInterceptionEnabled)
          ? SigninInterceptionHeuristicOutcome::kAbortAccountNotNew
          : SigninInterceptionHeuristicOutcome::kAbortInterceptionDisabled);
}

TEST_P(DiceWebSigninInterceptorManagedAccountTest,
       EnforceManagedAccountSecondaryAccountNotAllowedReauth) {
  AccountInfo primary_account_info =
      identity_test_env()->MakePrimaryAccountAvailable(
          "alice@example.com", signin::ConsentLevel::kSignin);
  MakeValidAccountInfo(&primary_account_info, "example.com");
  identity_test_env()->UpdateAccountInfoForAccount(primary_account_info);

  std::string email = "bob@example.com";
  AccountInfo account_info = identity_test_env()->MakeAccountAvailable(email);
  MakeValidAccountInfo(&account_info, "example.com");
  identity_test_env()->UpdateAccountInfoForAccount(account_info);

  base::Value::List profile_separation_exception_list;
  profile_separation_exception_list.Append(base::Value("notexample.com"));
  profile()->GetPrefs()->SetList(prefs::kProfileSeparationDomainExceptionList,
                                 std::move(profile_separation_exception_list));

  // Check that interception works otherwise, as a sanity check.
  WebSigninInterceptor::Delegate::BubbleParameters expected_parameters(
      WebSigninInterceptor::SigninInterceptionType::kEnterpriseForced,
      account_info, primary_account_info, SkColor(),
      /*show_link_data_option=*/false, /*show_managed_disclaimer=*/true);
  EXPECT_CALL(*mock_delegate(),
              ShowSigninInterceptionBubble(
                  web_contents(), MatchBubbleParameters(expected_parameters),
                  testing::_));
  TestSynchronousInterception(
      account_info, /*is_new_account=*/false, /*is_sync_signin=*/false,
      SigninInterceptionHeuristicOutcome::kInterceptEnterpriseForced);
}

TEST_P(DiceWebSigninInterceptorManagedAccountTest,
       EnforceManagedAccountSecondaryConsumerAccountNotAllowed) {
  AccountInfo primary_account_info =
      identity_test_env()->MakePrimaryAccountAvailable(
          "alice@example.com", signin::ConsentLevel::kSignin);
  MakeValidAccountInfo(&primary_account_info, "example.com");
  identity_test_env()->UpdateAccountInfoForAccount(primary_account_info);

  std::string email = "bob@gmail.com";
  AccountInfo account_info = identity_test_env()->MakeAccountAvailable(email);
  MakeValidAccountInfo(&account_info);
  identity_test_env()->UpdateAccountInfoForAccount(account_info);

  base::Value::List profile_separation_exception_list;
  profile_separation_exception_list.Append(base::Value("example.com"));
  profile()->GetPrefs()->SetList(prefs::kProfileSeparationDomainExceptionList,
                                 std::move(profile_separation_exception_list));

  // Check that interception works otherwise, as a sanity check.
  WebSigninInterceptor::Delegate::BubbleParameters expected_parameters(
      WebSigninInterceptor::SigninInterceptionType::kEnterpriseForced,
      account_info, primary_account_info, SkColor(),
      /*show_link_data_option=*/false, /*show_managed_disclaimer=*/false);
  EXPECT_CALL(*mock_delegate(),
              ShowSigninInterceptionBubble(
                  web_contents(), MatchBubbleParameters(expected_parameters),
                  testing::_));
  TestSynchronousInterception(
      account_info, /*is_new_account=*/true, /*is_sync_signin=*/false,
      SigninInterceptionHeuristicOutcome::kInterceptEnterpriseForced);
}

TEST_P(DiceWebSigninInterceptorManagedAccountTest,
       EnforceManagedAccountSecondaryAccountAllowed) {
  AccountInfo primary_account_info =
      identity_test_env()->MakePrimaryAccountAvailable(
          "alice@example.com", signin::ConsentLevel::kSignin);
  MakeValidAccountInfo(&primary_account_info, "example.com");
  identity_test_env()->UpdateAccountInfoForAccount(primary_account_info);

  std::string email = "bob@gmail.com";
  AccountInfo account_info = identity_test_env()->MakeAccountAvailable(email);
  MakeValidAccountInfo(&account_info);
  identity_test_env()->UpdateAccountInfoForAccount(account_info);

  base::Value::List profile_separation_exception_list;
  profile_separation_exception_list.Append(base::Value("gmail.com"));
  profile()->GetPrefs()->SetList(prefs::kProfileSeparationDomainExceptionList,
                                 std::move(profile_separation_exception_list));

  if (!profile()->GetPrefs()->GetBoolean(prefs::kSigninInterceptionEnabled)) {
    TestSynchronousInterception(
        account_info, /*is_new_account=*/true, /*is_sync_signin=*/false,
        SigninInterceptionHeuristicOutcome::kAbortInterceptionDisabled);
    return;
  }
  // Check that interception works otherwise, as a sanity check.
  WebSigninInterceptor::Delegate::BubbleParameters expected_parameters(
      WebSigninInterceptor::SigninInterceptionType::kEnterprise, account_info,
      primary_account_info, SkColor(),
      /*show_link_data_option=*/false, /*show_managed_disclaimer=*/false);
  EXPECT_CALL(*mock_delegate(),
              ShowSigninInterceptionBubble(
                  web_contents(), MatchBubbleParameters(expected_parameters),
                  testing::_));
  TestAsynchronousInterception(
      account_info, /*is_new_account=*/true, /*is_sync_signin=*/false,
      SigninInterceptionHeuristicOutcome::kInterceptEnterprise);
}

INSTANTIATE_TEST_SUITE_P(All,
                         DiceWebSigninInterceptorManagedAccountTest,
                         ::testing::Bool(),
                         &ParamToTestSuffixForInterceptionAndSyncPromo);

TEST_F(DiceWebSigninInterceptorTest, ShouldShowEnterpriseBubbleWithoutUPA) {
  AccountInfo account_info_1 =
      identity_test_env()->MakeAccountAvailable("bob@example.com");
  MakeValidAccountInfo(&account_info_1);
  account_info_1.hosted_domain = "example.com";
  identity_test_env()->UpdateAccountInfoForAccount(account_info_1);
  AccountInfo account_info_2 =
      identity_test_env()->MakeAccountAvailable("alice@example.com");
  MakeValidAccountInfo(&account_info_2);
  account_info_2.hosted_domain = "example.com";
  identity_test_env()->UpdateAccountInfoForAccount(account_info_2);

  // Primary account is not set.
  ASSERT_FALSE(identity_test_env()->identity_manager()->HasPrimaryAccount(
      signin::ConsentLevel::kSignin));
  EXPECT_FALSE(interceptor()->ShouldShowEnterpriseBubble(account_info_1));
}

TEST_F(DiceWebSigninInterceptorTest, ShouldShowMultiUserBubble) {
  // Setup two accounts in the profile.
  AccountInfo account_info_1 = identity_test_env()->MakePrimaryAccountAvailable(
      "bob@example.com", signin::ConsentLevel::kSignin);
  MakeValidAccountInfo(&account_info_1);
  account_info_1.given_name = "Bob";
  identity_test_env()->UpdateAccountInfoForAccount(account_info_1);
  AccountInfo account_info_2 =
      identity_test_env()->MakeAccountAvailable("alice@example.com");

  // The other account does not have full account info (empty name).
  ASSERT_TRUE(account_info_2.given_name.empty());
  EXPECT_TRUE(interceptor()->ShouldShowMultiUserBubble(account_info_1));

  // Accounts with different names.
  account_info_1.given_name = "Bob";
  identity_test_env()->UpdateAccountInfoForAccount(account_info_1);
  MakeValidAccountInfo(&account_info_2);
  account_info_2.given_name = "Alice";
  identity_test_env()->UpdateAccountInfoForAccount(account_info_2);
  EXPECT_TRUE(interceptor()->ShouldShowMultiUserBubble(account_info_1));

  // Accounts with same names.
  account_info_1.given_name = "Alice";
  identity_test_env()->UpdateAccountInfoForAccount(account_info_1);
  EXPECT_FALSE(interceptor()->ShouldShowMultiUserBubble(account_info_1));

  // Comparison is case insensitive.
  account_info_1.given_name = "alice";
  identity_test_env()->UpdateAccountInfoForAccount(account_info_1);
  EXPECT_FALSE(interceptor()->ShouldShowMultiUserBubble(account_info_1));
}

TEST_F(DiceWebSigninInterceptorTest,
       ShouldShowMultiUserBubbleNoPrimaryAccount) {
  // Setup two accounts in the profile.
  AccountInfo account_info_1 =
      identity_test_env()->MakeAccountAvailable("bob@example.com");
  MakeValidAccountInfo(&account_info_1);
  account_info_1.given_name = "Bob";
  identity_test_env()->UpdateAccountInfoForAccount(account_info_1);
  AccountInfo account_info_2 =
      identity_test_env()->MakeAccountAvailable("alice@example.com");
  account_info_2.given_name = "Alice";
  EXPECT_FALSE(interceptor()->ShouldShowMultiUserBubble(account_info_1));

  identity_test_env()->SetPrimaryAccount("bob@example.com",
                                         signin::ConsentLevel::kSignin);
  EXPECT_TRUE(interceptor()->ShouldShowMultiUserBubble(account_info_1));
}

TEST_F(DiceWebSigninInterceptorTest, NoInterception) {
  // Setup for profile switch interception.
  std::string email = "bob@example.com";
  AccountInfo account_info = identity_test_env()->MakeAccountAvailable(email);
  MakeValidAccountInfo(&account_info, "example.com");
  identity_test_env()->UpdateAccountInfoForAccount(account_info);
  Profile* profile_2 = CreateTestingProfile("Profile 2");
  ProfileAttributesEntry* entry =
      profile_attributes_storage()->GetProfileAttributesWithPath(
          profile_2->GetPath());
  ASSERT_NE(entry, nullptr);
  entry->SetAuthInfo(account_info.gaia, base::UTF8ToUTF16(email),
                     /*is_consented_primary_account=*/false);

  if (switches::IsExplicitBrowserSigninUIOnDesktopEnabled()) {
    // Suppress the signin bubble.
    SigninPrefs(*profile()->GetPrefs())
        .SetChromeSigninInterceptionUserChoice(
            account_info.gaia, ChromeSigninUserChoice::kDoNotSignin);
  }

  // Check that Sync signin is not intercepted.
  TestSynchronousInterception(
      account_info, /*is_new_account=*/true, /*is_sync_signin=*/true,
      SigninInterceptionHeuristicOutcome::kAbortSyncSignin);

  // Check that reauth is not intercepted.
  TestSynchronousInterception(
      account_info, /*is_new_account=*/false, /*is_sync_signin=*/false,
      SigninInterceptionHeuristicOutcome::kAbortAccountNotNew);

  // Check that interception works otherwise, as a sanity check.
  WebSigninInterceptor::Delegate::BubbleParameters expected_parameters(
      WebSigninInterceptor::SigninInterceptionType::kProfileSwitch,
      account_info, AccountInfo());
  EXPECT_CALL(*mock_delegate(),
              ShowSigninInterceptionBubble(
                  web_contents(), MatchBubbleParameters(expected_parameters),
                  testing::_));
  TestSynchronousInterception(
      account_info, /*is_new_account=*/true, /*is_sync_signin=*/false,
      SigninInterceptionHeuristicOutcome::kInterceptProfileSwitch);
}

// Checks that the heuristic still works if the account was not added to Chrome
// yet.
TEST_F(DiceWebSigninInterceptorTest, HeuristicAccountNotAdded) {
  // Setup for profile switch interception.
  std::string email = "bob@example.com";
  AccountInfo account_info = identity_test_env()->MakeAccountAvailable(email);
  MakeValidAccountInfo(&account_info, "example.com");
  identity_test_env()->UpdateAccountInfoForAccount(account_info);
  Profile* profile_2 = CreateTestingProfile("Profile 2");
  ProfileAttributesEntry* entry =
      profile_attributes_storage()->GetProfileAttributesWithPath(
          profile_2->GetPath());
  ASSERT_NE(entry, nullptr);
  entry->SetAuthInfo("dummy_gaia_id", base::UTF8ToUTF16(email),
                     /*is_consented_primary_account=*/false);
  EXPECT_EQ(interceptor()->GetHeuristicOutcome(
                /*is_new_account=*/true, /*is_sync_signin=*/false, email),
            SigninInterceptionHeuristicOutcome::kInterceptProfileSwitch);
}

// Checks that the heuristic defaults to gmail.com when no domain is specified.
TEST_F(DiceWebSigninInterceptorTest, HeuristicDefaultsToGmail) {
  // Setup for profile switch interception.
  std::string email = "bob@gmail.com";
  Profile* profile_2 = CreateTestingProfile("Profile 2");
  ProfileAttributesEntry* entry =
      profile_attributes_storage()->GetProfileAttributesWithPath(
          profile_2->GetPath());
  ASSERT_NE(entry, nullptr);
  entry->SetAuthInfo("dummy_gaia_id", base::UTF8ToUTF16(email),
                     /*is_consented_primary_account=*/false);
  // No domain defaults to gmail.com
  EXPECT_EQ(interceptor()->GetHeuristicOutcome(
                /*is_new_account=*/true, /*is_sync_signin=*/false, "bob"),
            SigninInterceptionHeuristicOutcome::kInterceptProfileSwitch);
}

// Checks that no heuristic is returned if signin interception is disabled.
TEST_F(DiceWebSigninInterceptorTest, InterceptionDisabled) {
  // Setup for profile switch interception.
  std::string email = "bob@gmail.com";
  Profile* profile_2 = CreateTestingProfile("Profile 2");
  profile()->GetPrefs()->SetBoolean(prefs::kSigninInterceptionEnabled, false);
  ProfileAttributesEntry* entry =
      profile_attributes_storage()->GetProfileAttributesWithPath(
          profile_2->GetPath());
  ASSERT_NE(entry, nullptr);
  entry->SetAuthInfo("dummy_gaia_id", base::UTF8ToUTF16(email),
                     /*is_consented_primary_account=*/false);
  EXPECT_EQ(interceptor()->GetHeuristicOutcome(
                /*is_new_account=*/true, /*is_sync_signin=*/false, "bob"),
            SigninInterceptionHeuristicOutcome::kAbortInterceptionDisabled);
  EXPECT_EQ(
      interceptor()->GetHeuristicOutcome(
          /*is_new_account=*/true, /*is_sync_signin=*/false, "bob@example.com"),
      std::nullopt);

  AccountInfo account_info =
      identity_test_env()->MakeAccountAvailable("bob@example.com");
  MakeValidAccountInfo(&account_info, "example.com");
  identity_test_env()->UpdateAccountInfoForAccount(account_info);
  EXPECT_EQ(
      interceptor()->GetHeuristicOutcome(
          /*is_new_account=*/true, /*is_sync_signin=*/false, "bob@example.com"),
      SigninInterceptionHeuristicOutcome::kAbortInterceptionDisabled);
}

TEST_F(DiceWebSigninInterceptorTest, TabClosed) {
  base::HistogramTester histogram_tester;
  interceptor()->MaybeInterceptWebSignin(
      /*web_contents=*/nullptr, CoreAccountId(),
      signin_metrics::AccessPoint::ACCESS_POINT_WEB_SIGNIN,
      /*is_new_account=*/true, /*is_sync_signin=*/false);
  histogram_tester.ExpectUniqueSample(
      "Signin.Intercept.HeuristicOutcome",
      SigninInterceptionHeuristicOutcome::kAbortTabClosed, 1);
}

TEST_F(DiceWebSigninInterceptorTest, InterceptionInProgress) {
  // Setup for profile switch interception.
  std::string email = "bob@example.com";
  AccountInfo account_info = identity_test_env()->MakeAccountAvailable(email);
  MakeValidAccountInfo(&account_info, "example.com");
  identity_test_env()->UpdateAccountInfoForAccount(account_info);
  Profile* profile_2 = CreateTestingProfile("Profile 2");
  ProfileAttributesEntry* entry =
      profile_attributes_storage()->GetProfileAttributesWithPath(
          profile_2->GetPath());
  ASSERT_NE(entry, nullptr);
  entry->SetAuthInfo(account_info.gaia, base::UTF8ToUTF16(email),
                     /*is_consented_primary_account=*/false);

  // Start an interception.
  WebSigninInterceptor::Delegate::BubbleParameters expected_parameters(
      WebSigninInterceptor::SigninInterceptionType::kProfileSwitch,
      account_info, AccountInfo());
  base::OnceCallback<void(SigninInterceptionResult)> delegate_callback;
  EXPECT_CALL(*mock_delegate(),
              ShowSigninInterceptionBubble(
                  web_contents(), MatchBubbleParameters(expected_parameters),
                  testing::_))
      .WillOnce(testing::WithArg<2>(testing::Invoke(
          [&delegate_callback](
              base::OnceCallback<void(SigninInterceptionResult)> callback) {
            delegate_callback = std::move(callback);
            return nullptr;
          })));
  MaybeIntercept(account_info.account_id);
  testing::Mock::VerifyAndClearExpectations(mock_delegate());
  EXPECT_TRUE(interceptor()->is_interception_in_progress());

  // Check that there is no interception while another one is in progress.
  base::HistogramTester histogram_tester;
  MaybeIntercept(account_info.account_id);
  testing::Mock::VerifyAndClearExpectations(mock_delegate());
  histogram_tester.ExpectUniqueSample(
      "Signin.Intercept.HeuristicOutcome",
      SigninInterceptionHeuristicOutcome::kAbortInterceptInProgress, 1);

  // Complete the interception that was in progress.
  std::move(delegate_callback).Run(SigninInterceptionResult::kDeclined);
  EXPECT_FALSE(interceptor()->is_interception_in_progress());

  // A new interception can now start.
  EXPECT_CALL(*mock_delegate(),
              ShowSigninInterceptionBubble(
                  web_contents(), MatchBubbleParameters(expected_parameters),
                  testing::_));
  MaybeIntercept(account_info.account_id);
}

TEST_F(DiceWebSigninInterceptorTest, DeclineCreationRepeatedly) {
  base::HistogramTester histogram_tester;
  AccountInfo primary_account_info =
      identity_test_env()->MakePrimaryAccountAvailable(
          "bob@example.com", signin::ConsentLevel::kSignin);
  AccountInfo account_info =
      identity_test_env()->MakeAccountAvailable("alice@example.com");
  MakeValidAccountInfo(&account_info, "example.com");
  identity_test_env()->UpdateAccountInfoForAccount(account_info);

  const int kMaxProfileCreationDeclinedCount = 2;
  // Decline the interception kMaxProfileCreationDeclinedCount times.
  WebSigninInterceptor::Delegate::BubbleParameters expected_parameters(
      WebSigninInterceptor::SigninInterceptionType::kEnterprise, account_info,
      primary_account_info, SkColor(),
      /*show_link_data_option=*/false, /*show_managed_disclaimer=*/true);
  for (int i = 0; i < kMaxProfileCreationDeclinedCount; ++i) {
    EXPECT_CALL(*mock_delegate(),
                ShowSigninInterceptionBubble(
                    web_contents(), MatchBubbleParameters(expected_parameters),
                    testing::_))
        .WillOnce(testing::WithArg<2>(testing::Invoke(
            [](base::OnceCallback<void(SigninInterceptionResult)> callback) {
              std::move(callback).Run(SigninInterceptionResult::kDeclined);
              return nullptr;
            })));
    MaybeIntercept(account_info.account_id);
    EXPECT_EQ(interceptor()->is_interception_in_progress(), false);
    histogram_tester.ExpectUniqueSample(
        "Signin.Intercept.HeuristicOutcome",
        SigninInterceptionHeuristicOutcome::kInterceptEnterprise, i + 1);
  }

  // Next time the interception is not shown again.
  MaybeIntercept(account_info.account_id);
  EXPECT_EQ(interceptor()->is_interception_in_progress(), false);
  histogram_tester.ExpectBucketCount(
      "Signin.Intercept.HeuristicOutcome",
      SigninInterceptionHeuristicOutcome::kAbortUserDeclinedProfileForAccount,
      1);

  // Another account can still be intercepted.
  account_info.email = "oscar@example.com";
  identity_test_env()->UpdateAccountInfoForAccount(account_info);
  expected_parameters.intercepted_account = account_info;
  EXPECT_CALL(*mock_delegate(),
              ShowSigninInterceptionBubble(
                  web_contents(), MatchBubbleParameters(expected_parameters),
                  testing::_));
  MaybeIntercept(account_info.account_id);
  histogram_tester.ExpectBucketCount(
      "Signin.Intercept.HeuristicOutcome",
      SigninInterceptionHeuristicOutcome::kInterceptEnterprise,
      kMaxProfileCreationDeclinedCount + 1);
  EXPECT_EQ(interceptor()->is_interception_in_progress(), true);
}

// Regression test for https://crbug.com/1309647
TEST_F(DiceWebSigninInterceptorTest,
       DeclineCreationRepeatedlyWithPolicyFetcher) {
  base::HistogramTester histogram_tester;
  AccountInfo primary_account_info =
      identity_test_env()->MakePrimaryAccountAvailable(
          "bob@example.com", signin::ConsentLevel::kSignin);
  AccountInfo account_info =
      identity_test_env()->MakeAccountAvailable("alice@example.com");
  MakeValidAccountInfo(&account_info, "example.com");
  identity_test_env()->UpdateAccountInfoForAccount(account_info);

  interceptor()->SetInterceptedAccountProfileSeparationPoliciesForTesting(
      policy::ProfileSeparationPolicies(""));

  const int kMaxProfileCreationDeclinedCount = 2;
  // Decline the interception kMaxProfileCreationDeclinedCount times.
  WebSigninInterceptor::Delegate::BubbleParameters expected_parameters(
      WebSigninInterceptor::SigninInterceptionType::kEnterprise, account_info,
      primary_account_info, SkColor(),
      /*show_link_data_option=*/false, /*show_managed_disclaimer=*/true);
  for (int i = 0; i < kMaxProfileCreationDeclinedCount; ++i) {
    EXPECT_CALL(*mock_delegate(),
                ShowSigninInterceptionBubble(
                    web_contents(), MatchBubbleParameters(expected_parameters),
                    testing::_))
        .WillOnce(testing::WithArg<2>(testing::Invoke(
            [](base::OnceCallback<void(SigninInterceptionResult)> callback) {
              std::move(callback).Run(SigninInterceptionResult::kDeclined);
              return nullptr;
            })));
    MaybeIntercept(account_info.account_id);
    EXPECT_EQ(interceptor()->is_interception_in_progress(), false);
    histogram_tester.ExpectUniqueSample(
        "Signin.Intercept.HeuristicOutcome",
        SigninInterceptionHeuristicOutcome::kInterceptEnterprise, i + 1);
  }

  // Next time the interception is not shown again.
  MaybeIntercept(account_info.account_id);
  EXPECT_EQ(interceptor()->is_interception_in_progress(), false);
  histogram_tester.ExpectBucketCount(
      "Signin.Intercept.HeuristicOutcome",
      SigninInterceptionHeuristicOutcome::kAbortUserDeclinedProfileForAccount,
      1);

  // Another account can still be intercepted.
  account_info.email = "oscar@example.com";
  identity_test_env()->UpdateAccountInfoForAccount(account_info);
  expected_parameters.intercepted_account = account_info;
  EXPECT_CALL(*mock_delegate(),
              ShowSigninInterceptionBubble(
                  web_contents(), MatchBubbleParameters(expected_parameters),
                  testing::_));
  MaybeIntercept(account_info.account_id);
  histogram_tester.ExpectBucketCount(
      "Signin.Intercept.HeuristicOutcome",
      SigninInterceptionHeuristicOutcome::kInterceptEnterprise,
      kMaxProfileCreationDeclinedCount + 1);
  EXPECT_EQ(interceptor()->is_interception_in_progress(), true);
}

TEST_F(DiceWebSigninInterceptorTest, DeclineSwitchRepeatedly_NoLimit) {
  base::HistogramTester histogram_tester;
  // Setup for profile switch interception.
  std::string email = "bob@example.com";
  AccountInfo account_info = identity_test_env()->MakeAccountAvailable(email);
  MakeValidAccountInfo(&account_info, "example.com");
  identity_test_env()->UpdateAccountInfoForAccount(account_info);
  Profile* profile_2 = CreateTestingProfile("Profile 2");
  ProfileAttributesEntry* entry =
      profile_attributes_storage()->GetProfileAttributesWithPath(
          profile_2->GetPath());
  ASSERT_NE(entry, nullptr);
  entry->SetAuthInfo(account_info.gaia, base::UTF8ToUTF16(email),
                     /*is_consented_primary_account=*/false);

  // Test that the profile switch can be declined multiple times.
  WebSigninInterceptor::Delegate::BubbleParameters expected_parameters(
      WebSigninInterceptor::SigninInterceptionType::kProfileSwitch,
      account_info, AccountInfo());
  for (int i = 0; i < 10; ++i) {
    EXPECT_CALL(*mock_delegate(),
                ShowSigninInterceptionBubble(
                    web_contents(), MatchBubbleParameters(expected_parameters),
                    testing::_))
        .WillOnce(testing::WithArg<2>(testing::Invoke(
            [](base::OnceCallback<void(SigninInterceptionResult)> callback) {
              std::move(callback).Run(SigninInterceptionResult::kDeclined);
              return nullptr;
            })));
    MaybeIntercept(account_info.account_id);
    EXPECT_EQ(interceptor()->is_interception_in_progress(), false);
    histogram_tester.ExpectUniqueSample(
        "Signin.Intercept.HeuristicOutcome",
        SigninInterceptionHeuristicOutcome::kInterceptProfileSwitch, i + 1);
  }
}

TEST_F(DiceWebSigninInterceptorTest, PersistentHash) {
  // The hash is persistent (the value should never change).
  EXPECT_EQ("email_174",
            interceptor()->GetPersistentEmailHash("alice@example.com"));
  // Different email get another hash.
  EXPECT_NE(interceptor()->GetPersistentEmailHash("bob@gmail.com"),
            interceptor()->GetPersistentEmailHash("alice@example.com"));
  // Equivalent emails get the same hash.
  EXPECT_EQ(interceptor()->GetPersistentEmailHash("bob"),
            interceptor()->GetPersistentEmailHash("bob@gmail.com"));
  EXPECT_EQ(interceptor()->GetPersistentEmailHash("bo.b@gmail.com"),
            interceptor()->GetPersistentEmailHash("bob@gmail.com"));
  // Dots are removed only for gmail accounts.
  EXPECT_NE(interceptor()->GetPersistentEmailHash("alice@example.com"),
            interceptor()->GetPersistentEmailHash("al.ice@example.com"));
}

// Interception other than the profile switch require at least 2 accounts.
TEST_F(DiceWebSigninInterceptorTest, NoInterceptionWithOneAccount) {
  base::HistogramTester histogram_tester;
  AccountInfo account_info =
      identity_test_env()->MakeAccountAvailable("bob@gmail.com");
  // Interception aborts even if the account info is not available.
  ASSERT_FALSE(identity_test_env()
                   ->identity_manager()
                   ->FindExtendedAccountInfoByAccountId(account_info.account_id)
                   .IsValid());

  if (switches::IsExplicitBrowserSigninUIOnDesktopEnabled()) {
    // Suppress the signin bubble.
    SigninPrefs(*profile()->GetPrefs())
        .SetChromeSigninInterceptionUserChoice(
            account_info.gaia, ChromeSigninUserChoice::kDoNotSignin);
  }

  TestSynchronousInterception(
      account_info, /*is_new_account=*/true, /*is_sync_signin=*/false,
      SigninInterceptionHeuristicOutcome::kAbortSingleAccount);
}

// When profile creation is disallowed, profile switch interception is still
// enabled, but others are disabled.
TEST_F(DiceWebSigninInterceptorTest, ProfileCreationDisallowed) {
  base::HistogramTester histogram_tester;
  g_browser_process->local_state()->SetBoolean(prefs::kBrowserAddPersonEnabled,
                                               false);
  // Setup for profile switch interception.
  std::string email = "bob@example.com";
  AccountInfo account_info = identity_test_env()->MakeAccountAvailable(email);
  MakeValidAccountInfo(&account_info, "example.com");
  identity_test_env()->UpdateAccountInfoForAccount(account_info);
  AccountInfo other_account_info =
      identity_test_env()->MakeAccountAvailable("alice@example.com");
  MakeValidAccountInfo(&other_account_info, "example.com");
  identity_test_env()->UpdateAccountInfoForAccount(other_account_info);
  Profile* profile_2 = CreateTestingProfile("Profile 2");
  ProfileAttributesEntry* entry =
      profile_attributes_storage()->GetProfileAttributesWithPath(
          profile_2->GetPath());
  ASSERT_NE(entry, nullptr);
  entry->SetAuthInfo(account_info.gaia, base::UTF8ToUTF16(email),
                     /*is_consented_primary_account=*/false);

  if (switches::IsExplicitBrowserSigninUIOnDesktopEnabled()) {
    // Suppress the signin bubble.
    SigninPrefs(*profile()->GetPrefs())
        .SetChromeSigninInterceptionUserChoice(
            other_account_info.gaia, ChromeSigninUserChoice::kDoNotSignin);
  }

  // Interception that would offer creating a new profile does not work.
  TestSynchronousInterception(
      other_account_info, /*is_new_account=*/true, /*is_sync_signin=*/false,
      SigninInterceptionHeuristicOutcome::kAbortProfileCreationDisallowed);

  // Profile switch interception still works.
  WebSigninInterceptor::Delegate::BubbleParameters expected_parameters(
      WebSigninInterceptor::SigninInterceptionType::kProfileSwitch,
      account_info, AccountInfo());
  EXPECT_CALL(*mock_delegate(),
              ShowSigninInterceptionBubble(
                  web_contents(), MatchBubbleParameters(expected_parameters),
                  testing::_));
  MaybeIntercept(account_info.account_id);
}

TEST_F(DiceWebSigninInterceptorTest, WaitForAccountInfoAvailable) {
  base::HistogramTester histogram_tester;
  AccountInfo primary_account_info =
      identity_test_env()->MakePrimaryAccountAvailable(
          "bob@example.com", signin::ConsentLevel::kSignin);
  AccountInfo account_info =
      identity_test_env()->MakeAccountAvailable("alice@example.com");
  EXPECT_FALSE(interceptor()
                   ->GetHeuristicOutcome(/*is_new_account=*/true,
                                         /*is_sync_signin=*/false,
                                         account_info.email)
                   .has_value());
  MaybeIntercept(account_info.account_id);
  // Delegate was not called yet.
  testing::Mock::VerifyAndClearExpectations(mock_delegate());

  // Account info becomes available, interception happens.
  WebSigninInterceptor::Delegate::BubbleParameters expected_parameters(
      WebSigninInterceptor::SigninInterceptionType::kEnterprise, account_info,
      primary_account_info, SkColor(),
      /*show_link_data_option=*/false, /*show_managed_disclaimer=*/true);
  EXPECT_CALL(*mock_delegate(),
              ShowSigninInterceptionBubble(
                  web_contents(), MatchBubbleParameters(expected_parameters),
                  testing::_));
  MakeValidAccountInfo(&account_info, "example.com");
  identity_test_env()->UpdateAccountInfoForAccount(account_info);
}

TEST_F(DiceWebSigninInterceptorTest, AccountInfoAlreadyAvailable) {
  base::HistogramTester histogram_tester;
  AccountInfo primary_account_info =
      identity_test_env()->MakePrimaryAccountAvailable(
          "bob@example.com", signin::ConsentLevel::kSignin);
  AccountInfo account_info =
      identity_test_env()->MakeAccountAvailable("alice@example.com");
  MakeValidAccountInfo(&account_info, "example.com");
  identity_test_env()->UpdateAccountInfoForAccount(account_info);

  // Account info is already available, interception happens immediately.
  WebSigninInterceptor::Delegate::BubbleParameters expected_parameters(
      WebSigninInterceptor::SigninInterceptionType::kEnterprise, account_info,
      primary_account_info, SkColor(),
      /*show_link_data_option=*/false, /*show_managed_disclaimer=*/true);
  EXPECT_CALL(*mock_delegate(),
              ShowSigninInterceptionBubble(
                  web_contents(), MatchBubbleParameters(expected_parameters),
                  testing::_));
  MaybeIntercept(account_info.account_id);
  histogram_tester.ExpectUniqueSample(
      "Signin.Intercept.HeuristicOutcome",
      SigninInterceptionHeuristicOutcome::kInterceptEnterprise, 1);
}

TEST_F(DiceWebSigninInterceptorTest, MultiUserInterception) {
  base::HistogramTester histogram_tester;
  AccountInfo primary_account_info =
      identity_test_env()->MakePrimaryAccountAvailable(
          "bob@example.com", signin::ConsentLevel::kSignin);
  AccountInfo account_info =
      identity_test_env()->MakeAccountAvailable("alice@example.com");
  MakeValidAccountInfo(&account_info);
  identity_test_env()->UpdateAccountInfoForAccount(account_info);

  // Account info is already available, interception happens immediately.
  WebSigninInterceptor::Delegate::BubbleParameters expected_parameters(
      WebSigninInterceptor::SigninInterceptionType::kMultiUser, account_info,
      primary_account_info);
  EXPECT_CALL(*mock_delegate(),
              ShowSigninInterceptionBubble(
                  web_contents(), MatchBubbleParameters(expected_parameters),
                  testing::_));
  MaybeIntercept(account_info.account_id);
  histogram_tester.ExpectUniqueSample(
      "Signin.Intercept.HeuristicOutcome",
      SigninInterceptionHeuristicOutcome::kInterceptMultiUser, 1);
}

TEST_F(DiceWebSigninInterceptorTest,
       AccountInfoAndCapabilitiesAlreadyAvailable) {
  base::HistogramTester histogram_tester;
  AccountInfo primary_account_info =
      identity_test_env()->MakePrimaryAccountAvailable(
          "bob@example.com", signin::ConsentLevel::kSignin);
  AccountInfo account_info =
      identity_test_env()->MakeAccountAvailable("alice@example.com");
  MakeValidAccountInfo(&account_info, "example.com");
  identity_test_env()->UpdateAccountInfoForAccount(account_info);

  // Account info is already available, interception happens immediately.
  WebSigninInterceptor::Delegate::BubbleParameters expected_parameters(
      WebSigninInterceptor::SigninInterceptionType::kEnterprise, account_info,
      primary_account_info, SkColor(),
      /*show_link_data_option=*/false, /*show_managed_disclaimer=*/true);
  EXPECT_CALL(*mock_delegate(),
              ShowSigninInterceptionBubble(
                  web_contents(), MatchBubbleParameters(expected_parameters),
                  testing::_));
  MaybeIntercept(account_info.account_id);
  histogram_tester.ExpectUniqueSample(
      "Signin.Intercept.HeuristicOutcome",
      SigninInterceptionHeuristicOutcome::kInterceptEnterprise, 1);
}

TEST_F(DiceWebSigninInterceptorTest,
       AccountInfoAlreadyAvailableWaitForCapabilities) {
  base::HistogramTester histogram_tester;
  AccountInfo primary_account_info =
      identity_test_env()->MakePrimaryAccountAvailable(
          "bob@example.com", signin::ConsentLevel::kSignin);
  AccountInfo account_info =
      identity_test_env()->MakeAccountAvailable("alice@example.com");
  MakeValidAccountInfoWithoutCapabilities(&account_info, "example.com");
  EXPECT_FALSE(interceptor()
                   ->GetHeuristicOutcome(/*is_new_account=*/true,
                                         /*is_sync_signin=*/false,
                                         account_info.email)
                   .has_value());
  MaybeIntercept(account_info.account_id);
  // Delegate was not called yet.
  testing::Mock::VerifyAndClearExpectations(mock_delegate());

  // Account capabilities become available, interception happens.
  WebSigninInterceptor::Delegate::BubbleParameters expected_parameters(
      WebSigninInterceptor::SigninInterceptionType::kEnterprise, account_info,
      primary_account_info, SkColor(),
      /*show_link_data_option=*/false, /*show_managed_disclaimer=*/true);
  EXPECT_CALL(*mock_delegate(),
              ShowSigninInterceptionBubble(
                  web_contents(), MatchBubbleParameters(expected_parameters),
                  testing::_));
  MakeValidAccountCapabilities(&account_info);
  identity_test_env()->UpdateAccountInfoForAccount(account_info);
}

TEST_F(DiceWebSigninInterceptorTest,
       AccountCapabilitiesAlreadyAvailableWaitForInfo) {
  base::HistogramTester histogram_tester;
  AccountInfo primary_account_info =
      identity_test_env()->MakePrimaryAccountAvailable(
          "bob@example.com", signin::ConsentLevel::kSignin);
  AccountInfo account_info =
      identity_test_env()->MakeAccountAvailable("alice@example.com");
  MakeValidAccountCapabilities(&account_info);
  EXPECT_FALSE(interceptor()
                   ->GetHeuristicOutcome(/*is_new_account=*/true,
                                         /*is_sync_signin=*/false,
                                         account_info.email)
                   .has_value());
  MaybeIntercept(account_info.account_id);
  // Delegate was not called yet.
  testing::Mock::VerifyAndClearExpectations(mock_delegate());

  // Account info becomes available, interception happens.
  WebSigninInterceptor::Delegate::BubbleParameters expected_parameters(
      WebSigninInterceptor::SigninInterceptionType::kEnterprise, account_info,
      primary_account_info, SkColor(),
      /*show_link_data_option=*/false, /*show_managed_disclaimer=*/true);
  EXPECT_CALL(*mock_delegate(),
              ShowSigninInterceptionBubble(
                  web_contents(), MatchBubbleParameters(expected_parameters),
                  testing::_));
  MakeValidAccountInfo(&account_info, "example.com");
  identity_test_env()->UpdateAccountInfoForAccount(account_info);
}

TEST_F(DiceWebSigninInterceptorTest, WaitForAccountInfoTimeout) {
  base::HistogramTester histogram_tester;
  AccountInfo primary_account_info =
      identity_test_env()->MakePrimaryAccountAvailable(
          "bob@example.com", signin::ConsentLevel::kSignin);
  AccountInfo account_info =
      identity_test_env()->MakeAccountAvailable("alice@example.com");
  EXPECT_FALSE(interceptor()
                   ->GetHeuristicOutcome(/*is_new_account=*/true,
                                         /*is_sync_signin=*/false,
                                         account_info.email)
                   .has_value());
  MaybeIntercept(account_info.account_id);
  // Delegate was not called yet.
  testing::Mock::VerifyAndClearExpectations(mock_delegate());

  // No interception happens, as we time out without the required info.
  testing::Mock::VerifyAndClearExpectations(mock_delegate());
  task_environment()->FastForwardBy(base::Seconds(5));
}

TEST_F(DiceWebSigninInterceptorTest, AccountInfoRemovedWhileWaiting) {
  base::HistogramTester histogram_tester;
  AccountInfo primary_account_info =
      identity_test_env()->MakePrimaryAccountAvailable(
          "bob@example.com", signin::ConsentLevel::kSignin);
  AccountInfo account_info =
      identity_test_env()->MakeAccountAvailable("alice@example.com");
  EXPECT_FALSE(interceptor()
                   ->GetHeuristicOutcome(/*is_new_account=*/true,
                                         /*is_sync_signin=*/false,
                                         account_info.email)
                   .has_value());
  MaybeIntercept(account_info.account_id);
  // Delegate was not called yet, interception is in progress.
  testing::Mock::VerifyAndClearExpectations(mock_delegate());
  EXPECT_TRUE(interceptor()->is_interception_in_progress());

  // Clear primary account.
  identity_test_env()->EnableRemovalOfExtendedAccountInfo();
  identity_test_env()->RemoveRefreshTokenForAccount(account_info.account_id);

  // Interception is cancelled.
  EXPECT_FALSE(interceptor()->is_interception_in_progress());
  histogram_tester.ExpectUniqueSample(
      "Signin.Intercept.HeuristicOutcome",
      SigninInterceptionHeuristicOutcome::kAbortSignedOut, 1);
}

TEST_F(DiceWebSigninInterceptorTest, WaitForAccountCapabilitiesTimeout) {
  base::HistogramTester histogram_tester;
  AccountInfo primary_account_info =
      identity_test_env()->MakePrimaryAccountAvailable(
          "bob@example.com", signin::ConsentLevel::kSignin);
  AccountInfo account_info =
      identity_test_env()->MakeAccountAvailable("alice@example.com");
  MakeValidAccountInfoWithoutCapabilities(&account_info, "example.com");
  identity_test_env()->UpdateAccountInfoForAccount(account_info);
  EXPECT_FALSE(interceptor()
                   ->GetHeuristicOutcome(/*is_new_account=*/true,
                                         /*is_sync_signin=*/false,
                                         account_info.email)
                   .has_value());
  MaybeIntercept(account_info.account_id);

  // Delegate was not called yet.
  testing::Mock::VerifyAndClearExpectations(mock_delegate());

  // Interception happens, as capabilities are not required.
  WebSigninInterceptor::Delegate::BubbleParameters expected_parameters(
      WebSigninInterceptor::SigninInterceptionType::kEnterprise, account_info,
      primary_account_info, SkColor(),
      /*show_link_data_option=*/false, /*show_managed_disclaimer=*/true);
  EXPECT_CALL(*mock_delegate(),
              ShowSigninInterceptionBubble(
                  web_contents(), MatchBubbleParameters(expected_parameters),
                  testing::_));
  task_environment()->FastForwardBy(base::Seconds(5));
}

TEST_F(DiceWebSigninInterceptorTest,
       ConsumerAccountForcedEnterpriseInterceptionOnEmptyProfile) {
  base::HistogramTester histogram_tester;
  AccountInfo account_info =
      identity_test_env()->MakeAccountAvailable("alice@gmail.com");
  MakeValidAccountInfo(&account_info);
  identity_test_env()->UpdateAccountInfoForAccount(account_info);

  base::Value::List profile_separation_exception_list;
  profile_separation_exception_list.Append(base::Value("notexample.com"));
  profile()->GetPrefs()->SetList(prefs::kProfileSeparationDomainExceptionList,
                                 std::move(profile_separation_exception_list));

  // Account info is already available, interception happens immediately.
  WebSigninInterceptor::Delegate::BubbleParameters expected_parameters(
      WebSigninInterceptor::SigninInterceptionType::kEnterpriseForced,
      account_info, AccountInfo(), SkColor(),
      /*show_link_data_option=*/true, /*show_managed_disclaimer=*/false);
  EXPECT_CALL(*mock_delegate(),
              ShowSigninInterceptionBubble(
                  web_contents(), MatchBubbleParameters(expected_parameters),
                  testing::_));
  MaybeIntercept(account_info.account_id);
  histogram_tester.ExpectUniqueSample(
      "Signin.Intercept.HeuristicOutcome",
      SigninInterceptionHeuristicOutcome::kInterceptEnterpriseForced, 1);
}

TEST_F(DiceWebSigninInterceptorTest, ConsumerAccountAllowedOnEmptyProfile) {
  base::HistogramTester histogram_tester;
  AccountInfo account_info =
      identity_test_env()->MakeAccountAvailable("alice@gmail.com");
  MakeValidAccountInfo(&account_info);
  identity_test_env()->UpdateAccountInfoForAccount(account_info);

  if (switches::IsExplicitBrowserSigninUIOnDesktopEnabled()) {
    // Suppress the signin bubble.
    SigninPrefs(*profile()->GetPrefs())
        .SetChromeSigninInterceptionUserChoice(
            account_info.gaia, ChromeSigninUserChoice::kDoNotSignin);
  }

  base::Value::List profile_separation_exception_list;
  profile_separation_exception_list.Append(base::Value("gmail.com"));
  profile()->GetPrefs()->SetList(prefs::kProfileSeparationDomainExceptionList,
                                 std::move(profile_separation_exception_list));

  MaybeIntercept(account_info.account_id);
  histogram_tester.ExpectUniqueSample(
      "Signin.Intercept.HeuristicOutcome",
      SigninInterceptionHeuristicOutcome::kAbortSingleAccount, 1);
}

TEST_F(DiceWebSigninInterceptorTest,
       ConsumerAccountForcedEnterpriseInterceptionOnManagedProfile) {
  base::HistogramTester histogram_tester;
  AccountInfo primary_account_info =
      identity_test_env()->MakePrimaryAccountAvailable(
          "bob@example.com", signin::ConsentLevel::kSignin);
  primary_account_info.hosted_domain = "example.com";
  identity_test_env()->UpdateAccountInfoForAccount(primary_account_info);

  AccountInfo account_info =
      identity_test_env()->MakeAccountAvailable("alice@gmail.com");
  MakeValidAccountInfo(&account_info);
  identity_test_env()->UpdateAccountInfoForAccount(account_info);

  base::Value::List profile_separation_exception_list;
  profile_separation_exception_list.Append(base::Value("notexample.com"));
  profile()->GetPrefs()->SetList(prefs::kProfileSeparationDomainExceptionList,
                                 std::move(profile_separation_exception_list));

  // Account info is already available, interception happens immediately.
  WebSigninInterceptor::Delegate::BubbleParameters expected_parameters(
      WebSigninInterceptor::SigninInterceptionType::kEnterpriseForced,
      account_info, primary_account_info, SkColor(),
      /*show_link_data_option=*/false, /*show_managed_disclaimer=*/false);
  EXPECT_CALL(*mock_delegate(),
              ShowSigninInterceptionBubble(
                  web_contents(), MatchBubbleParameters(expected_parameters),
                  testing::_));
  MaybeIntercept(account_info.account_id);
  histogram_tester.ExpectUniqueSample(
      "Signin.Intercept.HeuristicOutcome",
      SigninInterceptionHeuristicOutcome::kInterceptEnterpriseForced, 1);
}

TEST_F(DiceWebSigninInterceptorTest, StateResetTest) {
  // This is a simplification of the equality check. There is no need to
  // implement a full exhaustive check for the test.
  auto AreStatesEqual =
      [](const DiceWebSigninInterceptor::ResetableState* state1,
         const DiceWebSigninInterceptor::ResetableState* state2) {
        return state1->is_interception_in_progress_ ==
               state2->is_interception_in_progress_;
      };

  // Create the default values to be compared to.
  DiceWebSigninInterceptor::ResetableState default_values;

  DiceWebSigninInterceptor::ResetableState* state_ =
      interceptor()->state_.get();
  // Ensure initial default values.
  EXPECT_TRUE(AreStatesEqual(state_, &default_values));

  // Simulate default state value modifications
  state_->is_interception_in_progress_ = true;

  ASSERT_FALSE(AreStatesEqual(state_, &default_values));

  // Reset and check the default values equality.
  interceptor()->Reset();

  // Values should be properly reset to default values.
  EXPECT_TRUE(AreStatesEqual(interceptor()->state_.get(), &default_values));
}

// Tests the recording of metrics relating to the supervised user capability.
class DiceWebSigninInterceptorTestSupervisionMetrics
    : public DiceWebSigninInterceptorTest,
      public testing::WithParamInterface<
          std::tuple<signin::Tribool,
                     WebSigninInterceptor::SigninInterceptionType>> {
 public:
  DiceWebSigninInterceptorTestSupervisionMetrics() {
    feature_list_.InitAndEnableFeature(
        switches::kExplicitBrowserSigninUIOnDesktop);
  }

  signin::Tribool IsSupervisedUser() { return std::get<0>(GetParam()); }
  WebSigninInterceptor::SigninInterceptionType GetInterceptionType() {
    return std::get<1>(GetParam());
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

// helper
std::string InterceptionTypeString(
    WebSigninInterceptor::SigninInterceptionType interception_type) {
  switch (interception_type) {
    case WebSigninInterceptor::SigninInterceptionType::kChromeSignin:
      return "ChromeSignin";
    case WebSigninInterceptor::SigninInterceptionType::kMultiUser:
      return "MultiUser";
    case WebSigninInterceptor::SigninInterceptionType::kProfileSwitch:
      return "ProfileSwitch";
    default:
      return "";
  }
}

INSTANTIATE_TEST_SUITE_P(
    All,
    DiceWebSigninInterceptorTestSupervisionMetrics,
    testing::Combine(
        testing::Values(signin::Tribool::kTrue,
                        signin::Tribool::kFalse,
                        signin::Tribool::kUnknown),
        testing::Values(
            WebSigninInterceptor::SigninInterceptionType::kChromeSignin,
            WebSigninInterceptor::SigninInterceptionType::kMultiUser,
            WebSigninInterceptor::SigninInterceptionType::kProfileSwitch)),
    [](const auto& info) {
      std::string name = "";
      switch (std::get<0>(info.param)) {
        case signin::Tribool::kTrue:
          name += "ForSupervisedUser";
          break;
        case signin::Tribool::kFalse:
          name += "ForRegularUser";
          break;
        case signin::Tribool::kUnknown:
          name += "ForUnknownSupervision";
          break;
      }
      name += InterceptionTypeString(std::get<1>(info.param));
      return name;
    });

TEST_P(DiceWebSigninInterceptorTestSupervisionMetrics, RecordMetrics) {
  base::HistogramTester histogram_tester;

  std::string intercepted_account_email = "alice@example.com";
  std::string other_account_email = "bob@example.com";

  AccountInfo other_account_info;
  if (GetInterceptionType() ==
      WebSigninInterceptor::SigninInterceptionType::kMultiUser) {
    // For the multi-use case, set the other account as the primary account.
    other_account_info = identity_test_env()->MakePrimaryAccountAvailable(
        other_account_email, signin::ConsentLevel::kSignin);
  }

  AccountInfo intercepted_account_info =
      identity_test_env()->MakeAccountAvailable(intercepted_account_email);
  MakeValidAccountInfoWithoutCapabilities(&intercepted_account_info);

  // Set supervised user capabilities and expectations.
  AccountCapabilitiesTestMutator mutator(
      &intercepted_account_info.capabilities);
  SinginInterceptSupervisionState expected_state;
  switch (IsSupervisedUser()) {
    case (signin::Tribool::kTrue):
      mutator.set_is_subject_to_parental_controls(true);
      expected_state = SinginInterceptSupervisionState::kSupervisedUser;
      break;
    case (signin::Tribool::kFalse):
      mutator.set_is_subject_to_parental_controls(false);
      expected_state = SinginInterceptSupervisionState::kRegularUser;
      break;
    case (signin::Tribool::kUnknown):
      expected_state = SinginInterceptSupervisionState::kUnknownSupervision;
      break;
  }
  identity_test_env()->UpdateAccountInfoForAccount(intercepted_account_info);

  if (GetInterceptionType() ==
      WebSigninInterceptor::SigninInterceptionType::kProfileSwitch) {
    // For the profile switch case, create an existing profile for the account
    // to be intercepted.
    Profile* profile_2 = CreateTestingProfile("Profile 2");
    ProfileAttributesEntry* entry =
        profile_attributes_storage()->GetProfileAttributesWithPath(
            profile_2->GetPath());
    ASSERT_NE(entry, nullptr);
    entry->SetAuthInfo(intercepted_account_info.gaia,
                       base::UTF8ToUTF16(intercepted_account_email),
                       /*is_consented_primary_account=*/false);
  }

  WebSigninInterceptor::Delegate::BubbleParameters expected_parameters(
      GetInterceptionType(), intercepted_account_info, other_account_info);
  EXPECT_CALL(*mock_delegate(),
              ShowSigninInterceptionBubble(
                  web_contents(), MatchBubbleParameters(expected_parameters),
                  testing::_));
  MaybeIntercept(intercepted_account_info.account_id);

  if (IsSupervisedUser() == signin::Tribool::kUnknown) {
    // Timeout the capabilities and account info fetching, as this is the case
    // the supervised user capability is still unknown.
    task_environment()->FastForwardBy(base::Seconds(5));
  }

  int expected_count_multiuser =
      GetInterceptionType() ==
              WebSigninInterceptor::SigninInterceptionType::kMultiUser
          ? 1
          : 0;
  int expected_count_signin =
      GetInterceptionType() ==
              WebSigninInterceptor::SigninInterceptionType::kChromeSignin
          ? 1
          : 0;
  int expected_count_switch =
      GetInterceptionType() ==
              WebSigninInterceptor::SigninInterceptionType::kProfileSwitch
          ? 1
          : 0;
  histogram_tester.ExpectBucketCount(
      "Signin.Intercept.Heuristic.SupervisionState.ChromeSignin",
      expected_state, expected_count_signin);
  histogram_tester.ExpectBucketCount(
      "Signin.Intercept.Heuristic.SupervisionState.MultiUser", expected_state,
      expected_count_multiuser);
  histogram_tester.ExpectBucketCount(
      "Signin.Intercept.Heuristic.SupervisionState.Switch", expected_state,
      expected_count_switch);
}

class DiceWebSigninInterceptorTestWithUnoEnabled
    : public DiceWebSigninInterceptorTest {
 private:
  base::test::ScopedFeatureList feature_list_{
      switches::kExplicitBrowserSigninUIOnDesktop};
};

TEST_F(DiceWebSigninInterceptorTestWithUnoEnabled,
       InterceptShouldShowChromeSigninBubbleOnAccountSigninAndChromeSignOut) {
  AccountInfo account_info =
      identity_test_env()->MakeAccountAvailable("alice@example.com");
  MakeValidAccountInfo(&account_info);
  identity_test_env()->UpdateAccountInfoForAccount(account_info);

  // Account is valid.
  ASSERT_TRUE(account_info.IsValid());
  // Primary account is not set, Chrome is not signed in.
  ASSERT_FALSE(identity_test_env()->identity_manager()->HasPrimaryAccount(
      signin::ConsentLevel::kSignin));

  WebSigninInterceptor::Delegate::BubbleParameters expected_parameters(
      WebSigninInterceptor::SigninInterceptionType::kChromeSignin,
      /*intercepted_account=*/account_info,
      /*primary_account=*/AccountInfo());
  EXPECT_CALL(*mock_delegate(),
              ShowSigninInterceptionBubble(
                  web_contents(), MatchBubbleParameters(expected_parameters),
                  testing::_));

  auto expected_outcome =
      SigninInterceptionHeuristicOutcome::kInterceptChromeSignin;
  base::HistogramTester histogram_tester;
  interceptor()->MaybeInterceptWebSignin(
      web_contents(), account_info.account_id,
      signin_metrics::AccessPoint::ACCESS_POINT_WEB_SIGNIN,
      /*is_new_account=*/true, /*is_sync_signin=*/false);
  EXPECT_EQ(
      interceptor()->GetHeuristicOutcome(/*is_new_account=*/true,
                                         /*is_sync_signin=*/false,
                                         account_info.email, account_info.gaia),
      expected_outcome);
  testing::Mock::VerifyAndClearExpectations(mock_delegate());
  histogram_tester.ExpectUniqueSample("Signin.Intercept.HeuristicOutcome",
                                      expected_outcome, 1);
  histogram_tester.ExpectUniqueTimeSample("Signin.Intercept.HeuristicLatency",
                                          base::Milliseconds(0), 1);

  EXPECT_EQ(interceptor()->is_interception_in_progress(),
            SigninInterceptionHeuristicOutcomeIsSuccess(expected_outcome));

  histogram_tester.ExpectUniqueSample(
      "Signin.Intercept.Heuristic.ShouldShowChromeSigninBubbleWithReason",
      ShouldShowChromeSigninBubbleWithReason::kShouldShow, 1);
}

TEST_F(DiceWebSigninInterceptorTestWithUnoEnabled,
       InterceptShouldShowChromeSigninReauthAccountInfoAvailable) {
  AccountInfo account_info =
      identity_test_env()->MakeAccountAvailable("alice@example.com");
  MakeValidAccountInfo(&account_info);
  identity_test_env()->UpdateAccountInfoForAccount(account_info);

  // Account is valid.
  ASSERT_TRUE(account_info.IsValid());
  // Primary account is not set, Chrome is not signed in.
  ASSERT_FALSE(identity_test_env()->identity_manager()->HasPrimaryAccount(
      signin::ConsentLevel::kSignin));

  WebSigninInterceptor::Delegate::BubbleParameters expected_parameters(
      WebSigninInterceptor::SigninInterceptionType::kChromeSignin,
      /*intercepted_account=*/account_info,
      /*primary_account=*/AccountInfo());
  EXPECT_CALL(*mock_delegate(),
              ShowSigninInterceptionBubble(
                  web_contents(), MatchBubbleParameters(expected_parameters),
                  testing::_));

  auto expected_outcome =
      SigninInterceptionHeuristicOutcome::kInterceptChromeSignin;
  base::HistogramTester histogram_tester;
  interceptor()->MaybeInterceptWebSignin(
      web_contents(), account_info.account_id,
      signin_metrics::AccessPoint::ACCESS_POINT_WEB_SIGNIN,
      /*is_new_account=*/false, /*is_sync_signin=*/false);
  EXPECT_EQ(
      interceptor()->GetHeuristicOutcome(/*is_new_account=*/true,
                                         /*is_sync_signin=*/false,
                                         account_info.email, account_info.gaia),
      expected_outcome);
  testing::Mock::VerifyAndClearExpectations(mock_delegate());
  histogram_tester.ExpectUniqueSample("Signin.Intercept.HeuristicOutcome",
                                      expected_outcome, 1);
  histogram_tester.ExpectUniqueTimeSample("Signin.Intercept.HeuristicLatency",
                                          base::Milliseconds(0), 1);

  EXPECT_EQ(interceptor()->is_interception_in_progress(),
            SigninInterceptionHeuristicOutcomeIsSuccess(expected_outcome));

  histogram_tester.ExpectUniqueSample(
      "Signin.Intercept.Heuristic.ShouldShowChromeSigninBubbleWithReason",
      ShouldShowChromeSigninBubbleWithReason::kShouldShow, 1);
}

TEST_F(DiceWebSigninInterceptorTestWithUnoEnabled,
       InterceptShouldShowChromeSigninReauthWaitOnAccountInfo) {
  AccountInfo account_info =
      identity_test_env()->MakeAccountAvailable("alice@example.com");
  // Primary account is not set, Chrome is not signed in.
  ASSERT_FALSE(identity_test_env()->identity_manager()->HasPrimaryAccount(
      signin::ConsentLevel::kSignin));

  auto expected_outcome =
      SigninInterceptionHeuristicOutcome::kInterceptChromeSignin;
  base::HistogramTester histogram_tester;
  interceptor()->MaybeInterceptWebSignin(
      web_contents(), account_info.account_id,
      signin_metrics::AccessPoint::ACCESS_POINT_WEB_SIGNIN,
      /*is_new_account=*/true, /*is_sync_signin=*/false);
  EXPECT_EQ(interceptor()->is_interception_in_progress(), true);
  testing::Mock::VerifyAndClearExpectations(mock_delegate());

  WebSigninInterceptor::Delegate::BubbleParameters expected_parameters(
      WebSigninInterceptor::SigninInterceptionType::kChromeSignin,
      /*intercepted_account=*/account_info,
      /*primary_account=*/AccountInfo());
  EXPECT_CALL(*mock_delegate(),
              ShowSigninInterceptionBubble(
                  web_contents(), MatchBubbleParameters(expected_parameters),
                  testing::_));
  MakeValidAccountInfo(&account_info);
  identity_test_env()->UpdateAccountInfoForAccount(account_info);
  testing::Mock::VerifyAndClearExpectations(mock_delegate());

  histogram_tester.ExpectUniqueSample("Signin.Intercept.HeuristicOutcome",
                                      expected_outcome, 1);
  histogram_tester.ExpectUniqueTimeSample("Signin.Intercept.HeuristicLatency",
                                          base::Milliseconds(0), 1);

  EXPECT_EQ(interceptor()->is_interception_in_progress(),
            SigninInterceptionHeuristicOutcomeIsSuccess(expected_outcome));

  histogram_tester.ExpectUniqueSample(
      "Signin.Intercept.Heuristic.ShouldShowChromeSigninBubbleWithReason",
      ShouldShowChromeSigninBubbleWithReason::kShouldShow, 1);
}

TEST_F(DiceWebSigninInterceptorTestWithUnoEnabled,
       InterceptShouldShowChromeSigninBubbleSecondaryAccount) {
  AccountInfo account_info =
      identity_test_env()->MakeAccountAvailable("alice@example.com");
  MakeValidAccountInfo(&account_info);
  identity_test_env()->UpdateAccountInfoForAccount(account_info);

  // Account is valid.
  ASSERT_TRUE(account_info.IsValid());
  // Primary account is not set, Chrome is not signed in.
  ASSERT_FALSE(identity_test_env()->identity_manager()->HasPrimaryAccount(
      signin::ConsentLevel::kSignin));

  WebSigninInterceptor::Delegate::BubbleParameters expected_parameters(
      WebSigninInterceptor::SigninInterceptionType::kChromeSignin,
      /*intercepted_account=*/account_info,
      /*primary_account=*/AccountInfo());
  EXPECT_CALL(*mock_delegate(),
              ShowSigninInterceptionBubble(
                  web_contents(), MatchBubbleParameters(expected_parameters),
                  testing::_));

  auto expected_outcome =
      SigninInterceptionHeuristicOutcome::kInterceptChromeSignin;
  base::HistogramTester histogram_tester;
  interceptor()->MaybeInterceptWebSignin(
      web_contents(), account_info.account_id,
      signin_metrics::AccessPoint::ACCESS_POINT_WEB_SIGNIN,
      /*is_new_account=*/true, /*is_sync_signin=*/false);
  EXPECT_EQ(
      interceptor()->GetHeuristicOutcome(/*is_new_account=*/true,
                                         /*is_sync_signin=*/false,
                                         account_info.email, account_info.gaia),
      expected_outcome);
  testing::Mock::VerifyAndClearExpectations(mock_delegate());
  histogram_tester.ExpectUniqueSample("Signin.Intercept.HeuristicOutcome",
                                      expected_outcome, 1);
  histogram_tester.ExpectUniqueTimeSample("Signin.Intercept.HeuristicLatency",
                                          base::Milliseconds(0), 1);

  EXPECT_EQ(interceptor()->is_interception_in_progress(),
            SigninInterceptionHeuristicOutcomeIsSuccess(expected_outcome));

  histogram_tester.ExpectUniqueSample(
      "Signin.Intercept.Heuristic.ShouldShowChromeSigninBubbleWithReason",
      ShouldShowChromeSigninBubbleWithReason::kShouldShow, 1);
}

TEST_F(DiceWebSigninInterceptorTest,
       InterceptShouldNotShowWaitForAccountInfoAvailableMetricRecorded) {
  base::HistogramTester histogram_tester;
  AccountInfo account_info =
      identity_test_env()->MakeAccountAvailable("alice@example.com");
  EXPECT_FALSE(interceptor()
                   ->GetHeuristicOutcome(/*is_new_account=*/true,
                                         /*is_sync_signin=*/false,
                                         account_info.email)
                   .has_value());
  EXPECT_CALL(*mock_delegate(), ShowSigninInterceptionBubble(
                                    web_contents(), testing::_, testing::_))
      .Times(0);
  interceptor()->MaybeInterceptWebSignin(
      web_contents(), account_info.account_id,
      signin_metrics::AccessPoint::ACCESS_POINT_UNKNOWN,
      /*is_new_account=*/true,
      /*is_sync_signin=*/false);
  // Delegate was not called yet.
  testing::Mock::VerifyAndClearExpectations(mock_delegate());

  MakeValidAccountInfo(&account_info, "example.com");
  identity_test_env()->UpdateAccountInfoForAccount(account_info);
  auto expected_outcome =
      SigninInterceptionHeuristicOutcome::kAbortAccountInfoNotCompatible;
  EXPECT_EQ(interceptor()->is_interception_in_progress(),
            SigninInterceptionHeuristicOutcomeIsSuccess(expected_outcome));

  histogram_tester.ExpectUniqueSample(
      "Signin.Intercept.Heuristic.ShouldShowChromeSigninBubbleWithReason",
      ShouldShowChromeSigninBubbleWithReason::kShouldNotShowUnknownAccessPoint,
      1);
}

TEST_F(DiceWebSigninInterceptorTestWithUnoEnabled,
       NoInterceptionIfPrimaryAccountAlreadySet) {
  // Set up first account.
  const std::string primary_email = "alice@example.com";
  AccountInfo first_account_info =
      identity_test_env()->MakeAccountAvailable(primary_email);
  MakeValidAccountInfo(&first_account_info);
  identity_test_env()->UpdateAccountInfoForAccount(first_account_info);

  // Set up second account.
  AccountInfo second_account_info =
      identity_test_env()->MakeAccountAvailable("bob@example.com");
  MakeValidAccountInfo(&second_account_info);
  identity_test_env()->UpdateAccountInfoForAccount(second_account_info);

  // Accounts are valid.
  ASSERT_TRUE(first_account_info.IsValid());
  ASSERT_TRUE(second_account_info.IsValid());

  // Set the primary account.
  identity_test_env()->SetPrimaryAccount(primary_email,
                                         signin::ConsentLevel::kSignin);
  ASSERT_TRUE(identity_test_env()->identity_manager()->HasPrimaryAccount(
      signin::ConsentLevel::kSignin));

  // Sign in interception bubble should not be shown because this is not the
  // first account but there is no primary account.
  EXPECT_CALL(*mock_delegate(), ShowSigninInterceptionBubble(
                                    web_contents(), testing::_, testing::_))
      .Times(0);

  auto expected_outcome =
      SigninInterceptionHeuristicOutcome::kAbortAccountInfoNotCompatible;
  base::HistogramTester histogram_tester;
  interceptor()->MaybeInterceptWebSignin(
      web_contents(), second_account_info.account_id,
      signin_metrics::AccessPoint::ACCESS_POINT_WEB_SIGNIN,
      /*is_new_account=*/true, /*is_sync_signin=*/false);
  EXPECT_EQ(interceptor()->GetHeuristicOutcome(/*is_new_account=*/true,
                                               /*is_sync_signin=*/false,
                                               second_account_info.email),
            std::nullopt);
  testing::Mock::VerifyAndClearExpectations(mock_delegate());
  histogram_tester.ExpectUniqueSample("Signin.Intercept.HeuristicOutcome",
                                      expected_outcome, 1);
  histogram_tester.ExpectUniqueTimeSample("Signin.Intercept.HeuristicLatency",
                                          base::Milliseconds(0), 1);

  EXPECT_EQ(interceptor()->is_interception_in_progress(),
            SigninInterceptionHeuristicOutcomeIsSuccess(expected_outcome));

  histogram_tester.ExpectUniqueSample(
      "Signin.Intercept.Heuristic.ShouldShowChromeSigninBubbleWithReason",
      ShouldShowChromeSigninBubbleWithReason::kShouldNotShowAlreadySignedIn, 1);
}

class DiceWebSigninInterceptorTestWithUnoDisabled
    : public DiceWebSigninInterceptorTest {
 public:
  DiceWebSigninInterceptorTestWithUnoDisabled() {
    feature_list_.InitAndDisableFeature(
        switches::kExplicitBrowserSigninUIOnDesktop);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

TEST_F(DiceWebSigninInterceptorTestWithUnoDisabled,
       InterceptShouldLogChromeSigninBubbleOfferedForControlGroup) {
  AccountInfo account_info =
      identity_test_env()->MakeAccountAvailable("alice@example.com");
  MakeValidAccountInfo(&account_info);
  identity_test_env()->UpdateAccountInfoForAccount(account_info);

  // Account is valid.
  ASSERT_TRUE(account_info.IsValid());
  // Primary account is not set, Chrome is not signed in.
  ASSERT_FALSE(identity_test_env()->identity_manager()->HasPrimaryAccount(
      signin::ConsentLevel::kSignin));

  EXPECT_CALL(*mock_delegate(), ShowSigninInterceptionBubble(
                                    web_contents(), testing::_, testing::_))
      .Times(0);
  base::HistogramTester histogram_tester;
  interceptor()->MaybeInterceptWebSignin(
      web_contents(), account_info.account_id,
      signin_metrics::AccessPoint::ACCESS_POINT_WEB_SIGNIN,
      /*is_new_account=*/true, /*is_sync_signin=*/false);
  EXPECT_EQ(interceptor()->GetHeuristicOutcome(/*is_new_account=*/true,
                                               /*is_sync_signin=*/false,
                                               account_info.email),
            SigninInterceptionHeuristicOutcome::kAbortSingleAccount);

  histogram_tester.ExpectUniqueSample(
      "Signin.Intercept.Heuristic.ShouldShowChromeSigninBubbleWithReason",
      ShouldShowChromeSigninBubbleWithReason::kShouldShow, 1);
}
