// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/dice_web_signin_interceptor.h"

#include <memory>
#include <optional>
#include <tuple>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "build/buildflag.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/enterprise/browser_management/management_service_factory.h"
#include "chrome/browser/enterprise/util/managed_browser_utils.h"
#include "chrome/browser/profiles/profile_attributes_entry.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/signin/account_preview_data_service_factory.h"
#include "chrome/browser/signin/chrome_signin_client_factory.h"
#include "chrome/browser/signin/chrome_signin_client_test_util.h"
#include "chrome/browser/signin/chrome_signin_pref_names.h"
#include "chrome/browser/signin/dice_web_signin_interceptor_factory.h"
#include "chrome/browser/signin/identity_test_environment_profile_adaptor.h"
#include "chrome/browser/signin/web_signin_interceptor.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/metrics/profile_metrics_service.h"
#include "components/policy/core/browser/signin/profile_separation_policies.h"
#include "components/policy/core/common/management/scoped_management_service_override_for_testing.h"
#include "components/prefs/pref_service.h"
#include "components/signin/core/browser/account_preview_data_service.h"
#include "components/signin/core/browser/test_account_preview_data_service.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/base/signin_metrics.h"
#include "components/signin/public/base/signin_pref_names.h"
#include "components/signin/public/base/signin_prefs.h"
#include "components/signin/public/base/signin_switches.h"
#include "components/signin/public/identity_manager/account_capabilities_test_mutator.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/signin/public/identity_manager/tribool.h"
#include "components/sync/protocol/sync_enums.pb.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"
#include "google_apis/gaia/gaia_id.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace {

class MockDiceWebSigninInterceptorDelegate
    : public WebSigninInterceptor::Delegate {
 public:
  base::WeakPtr<MockDiceWebSigninInterceptorDelegate> GetWeakPtr() {
    return weak_factory_.GetWeakPtr();
  }

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
               signin::SigninChoiceWithConfirmAndRetryCallback,
               base::OnceClosure,
               base::RepeatingClosure),
              (override));
  void ShowFirstRunExperienceInNewProfile(
      BrowserWindowInterface* browser,
      const CoreAccountId& account_id,
      WebSigninInterceptor::SigninInterceptionType interception_type) override {
  }
  void ShowSigninError(content::WebContents* web_contents,
                       const SigninUIError& error) override {}

 private:
  base::WeakPtrFactory<MockDiceWebSigninInterceptorDelegate> weak_factory_{
      this};
};

MATCHER_P(HasSameAccountIdAs, other, "") {
  return arg.GetAccountId() == other.GetAccountId();
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
                     parameters.show_managed_disclaimer),
      testing::Field("account_preview_preference",
                     &WebSigninInterceptor::Delegate::BubbleParameters::
                         account_preview_preference,
                     parameters.account_preview_preference));
}

void MakeValidAccountCapabilities(AccountInfo* info) {
  AccountCapabilitiesTestMutator mutator(info);
  mutator.set_is_subject_to_parental_controls(false);
  bool is_managed = info->IsManaged() == signin::Tribool::kTrue;
  mutator.set_is_subject_to_enterprise_features(is_managed);
  mutator.set_is_subject_to_account_level_enterprise_policies(is_managed);
}

void MakeValidAccountInfoWithoutCapabilities(
    AccountInfo* info,
    const std::string& hosted_domain = std::string()) {
  if (info->IsValid()) {
    return;
  }
  *info = AccountInfo::Builder(*info)
              .SetFullName("fullname")
              .SetGivenName("givenname")
              .SetHostedDomain(hosted_domain)
              .SetLocale("en")
              .SetAvatarUrl("https://example.com")
              .Build();
  DCHECK(info->IsValid());
}

// If the account info is valid, does nothing. Otherwise fills the extended
// fields with default values.
void MakeValidAccountInfo(AccountInfo* info,
                          const std::string& hosted_domain = std::string()) {
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

class TestScopedWebSigninInterceptionBubbleHandle
    : public ScopedWebSigninInterceptionBubbleHandle {
 public:
  ~TestScopedWebSigninInterceptionBubbleHandle() override = default;
};

}  // namespace

class DiceWebSigninInterceptorTest : public testing::Test {
 public:
  DiceWebSigninInterceptorTest() = default;
  ~DiceWebSigninInterceptorTest() override = default;

  DiceWebSigninInterceptor* interceptor() {
    return DiceWebSigninInterceptorFactory::GetForProfile(profile());
  }

  MockDiceWebSigninInterceptorDelegate* mock_delegate() {
    return mock_delegate_.get();
  }

  TestingProfile* profile() { return profile_; }
  TestingProfileManager* profile_manager() { return &profile_manager_; }
  content::BrowserTaskEnvironment* task_environment() {
    return &task_environment_;
  }

  content::WebContents* web_contents() { return web_contents_.get(); }

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
  void MaybeIntercept(
      CoreAccountId account_id,
      signin::Tribool primary_is_connected = signin::Tribool::kUnknown) {
    interceptor()->MaybeInterceptWebSignin(
        web_contents(), account_id, signin_metrics::AccessPoint::kWebSignin,
        /*is_new_account=*/true, /*is_sync_signin=*/false,
        primary_is_connected);
  }

  // Calls MaybeInterceptWebSignin and verifies the heuristic outcome, the
  // histograms and whether the interception is in progress.
  // This function only works if the interception decision can be made
  // synchronously (GetHeuristicOutcome() returns a value).
  void TestSingleAccountSynchronousInterception(
      AccountInfo account_info,
      bool is_new_account,
      bool is_sync_signin,
      SigninInterceptionHeuristicOutcome expected_outcome) {
    TestSynchronousInterceptionImpl(
        account_info, is_new_account, is_sync_signin,
        /*primary_is_connected=*/signin::Tribool::kUnknown, expected_outcome);
  }

  void TestLinkedAccountsSynchronousInterception(
      AccountInfo account_info,
      bool is_new_account,
      bool is_sync_signin,
      signin::Tribool primary_is_connected,
      SigninInterceptionHeuristicOutcome expected_outcome) {
    TestSynchronousInterceptionImpl(account_info, is_new_account,
                                    is_sync_signin, primary_is_connected,
                                    expected_outcome);
  }

  void TestSingleAccountAsynchronousInterception(
      AccountInfo account_info,
      bool is_new_account,
      bool is_sync_signin,
      SigninInterceptionHeuristicOutcome expected_outcome) {
    TestAsynchronousInterceptionImpl(
        account_info, is_new_account, is_sync_signin,
        /*primary_is_connected=*/signin::Tribool::kUnknown, expected_outcome);
  }

  void TestLinkedAccountsAsynchronousInterception(
      AccountInfo account_info,
      bool is_new_account,
      bool is_sync_signin,
      signin::Tribool primary_is_connected,
      SigninInterceptionHeuristicOutcome expected_outcome) {
    TestAsynchronousInterceptionImpl(account_info, is_new_account,
                                     is_sync_signin, primary_is_connected,
                                     expected_outcome);
  }

 private:
  void TestSynchronousInterceptionImpl(
      AccountInfo account_info,
      bool is_new_account,
      bool is_sync_signin,
      signin::Tribool primary_is_connected,
      SigninInterceptionHeuristicOutcome expected_outcome) {
    ASSERT_EQ(interceptor()->GetHeuristicOutcome(
                  is_new_account, is_sync_signin, account_info.GetEmail(),
                  account_info.GetGaiaId(), nullptr, primary_is_connected),
              expected_outcome);
    base::HistogramTester histogram_tester;
    interceptor()->MaybeInterceptWebSignin(
        web_contents(), account_info.GetAccountId(),
        signin_metrics::AccessPoint::kWebSignin, is_new_account, is_sync_signin,
        primary_is_connected);
    testing::Mock::VerifyAndClearExpectations(mock_delegate());
    histogram_tester.ExpectUniqueSample("Signin.Intercept.HeuristicOutcome",
                                        expected_outcome, 1);

    EXPECT_EQ(interceptor()->is_interception_in_progress(),
              SigninInterceptionHeuristicOutcomeIsSuccess(expected_outcome));
  }

  void TestAsynchronousInterceptionImpl(
      AccountInfo account_info,
      bool is_new_account,
      bool is_sync_signin,
      signin::Tribool primary_is_connected,
      SigninInterceptionHeuristicOutcome expected_outcome) {
    ASSERT_EQ(interceptor()->GetHeuristicOutcome(
                  is_new_account, is_sync_signin, account_info.GetEmail(),
                  account_info.GetGaiaId(), nullptr, primary_is_connected),
              std::nullopt);
    base::HistogramTester histogram_tester;
    interceptor()->MaybeInterceptWebSignin(
        web_contents(), account_info.GetAccountId(),
        signin_metrics::AccessPoint::kWebSignin, is_new_account, is_sync_signin,
        primary_is_connected);
    testing::Mock::VerifyAndClearExpectations(mock_delegate());
    histogram_tester.ExpectUniqueSample("Signin.Intercept.HeuristicOutcome",
                                        expected_outcome, 1);
    EXPECT_EQ(interceptor()->is_interception_in_progress(),
              SigninInterceptionHeuristicOutcomeIsSuccess(expected_outcome));
  }

 protected:
  // testing::Test:
  void SetUp() override {
    ASSERT_TRUE(profile_manager_.SetUp());
    profile_ =
        profile_manager_.CreateTestingProfile("Default", GetTestingFactories());

    web_contents_ =
        content::WebContentsTester::CreateTestWebContents(profile(), nullptr);

    identity_test_env_profile_adaptor_ =
        std::make_unique<IdentityTestEnvironmentProfileAdaptor>(profile());
    identity_test_env_profile_adaptor_->identity_test_env()
        ->SetTestURLLoaderFactory(&test_url_loader_factory_);

    disclaimer_service_resetter_ =
        enterprise_util::DisableAutomaticManagementDisclaimerUntilReset(
            profile());

    // Creating the interceptor is necessary for setting the
    // mock_delegate_, so we can't rely on it being lazily initialized.
    DiceWebSigninInterceptorFactory::GetForProfile(profile());
  }

 private:
  void TearDown() override {
    identity_test_env_profile_adaptor_.reset();
    web_contents_.reset();
    profile_ = nullptr;
    profile_manager_.DeleteAllTestingProfiles();
  }

  std::unique_ptr<KeyedService> BuildDiceWebSigninInterceptor(
      content::BrowserContext* browser_context) {
    Profile* input_profile = Profile::FromBrowserContext(browser_context);
    CHECK_EQ(input_profile, profile());
    auto delegate = std::make_unique<
        testing::StrictMock<MockDiceWebSigninInterceptorDelegate>>();
    mock_delegate_ = delegate->GetWeakPtr();
    return std::make_unique<DiceWebSigninInterceptor>(
        profile(), std::move(delegate), &profile_metrics_service_);
  }

  TestingProfile::TestingFactories GetTestingFactories() {
    TestingProfile::TestingFactories factories =
        IdentityTestEnvironmentProfileAdaptor::
            GetIdentityTestEnvironmentFactories();
    factories.push_back(
        {ChromeSigninClientFactory::GetInstance(),
         base::BindRepeating(&BuildChromeSigninClientWithURLLoader,
                             &test_url_loader_factory_)});

    // TemplateURLService is required by ProceedWithProfileCreation which copies
    // search engine choice presets, otherwise it crashes in unit tests.
    factories.push_back(
        {TemplateURLServiceFactory::GetInstance(),
         base::BindRepeating(&TemplateURLServiceFactory::BuildInstanceFor)});

    factories.push_back(
        {DiceWebSigninInterceptorFactory::GetInstance(),
         base::BindRepeating(
             &DiceWebSigninInterceptorTest::BuildDiceWebSigninInterceptor,
             base::Unretained(this))});

    return factories;
  }

  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  content::RenderViewHostTestEnabler rvh_test_enabler_;
  TestingProfileManager profile_manager_{TestingBrowserProcess::GetGlobal()};
  raw_ptr<TestingProfile> profile_ = nullptr;
  std::unique_ptr<content::WebContents> web_contents_;

  // Force local machine to be unmanaged, so that variations in try bots and
  // developer machines don't affect the tests. See https://crbug.com/40268091.
  policy::ScopedManagementServiceOverrideForTesting platform_browser_mgmt_ = {
      policy::ManagementServiceFactory::GetForPlatform(),
      policy::EnterpriseManagementAuthority::NONE};
  network::TestURLLoaderFactory test_url_loader_factory_;
  std::unique_ptr<IdentityTestEnvironmentProfileAdaptor>
      identity_test_env_profile_adaptor_;
  base::WeakPtr<MockDiceWebSigninInterceptorDelegate> mock_delegate_;
  base::ScopedClosureRunner disclaimer_service_resetter_;
  metrics::ProfileMetricsService profile_metrics_service_;
};

TEST_F(DiceWebSigninInterceptorTest, ShouldShowProfileSwitchBubble) {
  AccountInfo account_info =
      identity_test_env()->MakeAccountAvailable("bob@example.com");
  const GaiaId& gaia = account_info.GetGaiaId();
  std::string_view email = account_info.GetEmail();
  EXPECT_FALSE(interceptor()->ShouldShowProfileSwitchBubble(
      gaia, email, profile_attributes_storage()));

  // Add another profile with no account.
  CreateTestingProfile("Profile 1");
  EXPECT_FALSE(interceptor()->ShouldShowProfileSwitchBubble(
      gaia, email, profile_attributes_storage()));

  // Add another profile with a different account.
  Profile* profile_2 = CreateTestingProfile("Profile 2");
  ProfileAttributesEntry* entry =
      profile_attributes_storage()->GetProfileAttributesWithPath(
          profile_2->GetPath());
  ASSERT_NE(entry, nullptr);
  const GaiaId kOtherGaiaID("SomeOtherGaiaID");
  ASSERT_NE(kOtherGaiaID, gaia);
  entry->SetAuthInfo(kOtherGaiaID, u"alice@gmail.com",
                     /*is_consented_primary_account=*/true);
  EXPECT_FALSE(interceptor()->ShouldShowProfileSwitchBubble(
      gaia, email, profile_attributes_storage()));

  // Change email to match.
  entry->SetAuthInfo(kOtherGaiaID, base::UTF8ToUTF16(email),
                     /*is_consented_primary_account=*/false);
  // With empty GaiaID, fall back to email: this is a match.
  EXPECT_EQ(entry, interceptor()->ShouldShowProfileSwitchBubble(
                       GaiaId(), email, profile_attributes_storage()));
  // When passing the GaiaID, it does not match.
  EXPECT_FALSE(interceptor()->ShouldShowProfileSwitchBubble(
      gaia, email, profile_attributes_storage()));

  // Change the gaia ID to match.
  entry->SetAuthInfo(gaia, base::UTF8ToUTF16(email),
                     /*is_consented_primary_account=*/false);
  EXPECT_EQ(entry, interceptor()->ShouldShowProfileSwitchBubble(
                       gaia, email, profile_attributes_storage()));
  // Email is ignored when the GaiaId is here. This is a match even if the email
  // is different.
  EXPECT_EQ(entry, interceptor()->ShouldShowProfileSwitchBubble(
                       gaia, "alice@gmail.com", profile_attributes_storage()));
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
  other_account_info = AccountInfo::Builder(other_account_info)
                           .SetHostedDomain("example.com")
                           .Build();
  AccountCapabilitiesTestMutator(&other_account_info)
      .set_is_subject_to_account_level_enterprise_policies(true);
  identity_test_env()->UpdateAccountInfoForAccount(other_account_info);
  AccountInfo account_info =
      identity_test_env()->MakeAccountAvailable("bob@example.com");
  MakeValidAccountInfo(&account_info);
  identity_test_env()->UpdateAccountInfoForAccount(account_info);
  ASSERT_EQ(identity_test_env()->identity_manager()->GetPrimaryAccountId(
                signin::ConsentLevel::kSignin),
            primary_account_info.GetAccountId());

  // The primary account does not have full account info (empty domain).
  ASSERT_EQ(identity_test_env()
                ->identity_manager()
                ->FindExtendedAccountInfo(primary_account_info)
                .GetHostedDomain(),
            std::nullopt);
  EXPECT_FALSE(interceptor()->ShouldShowEnterpriseBubble(account_info));
  account_info =
      AccountInfo::Builder(account_info).SetHostedDomain("example.com").Build();
  AccountCapabilitiesTestMutator(&account_info)
      .set_is_subject_to_account_level_enterprise_policies(true);
  identity_test_env()->UpdateAccountInfoForAccount(account_info);
  EXPECT_TRUE(interceptor()->ShouldShowEnterpriseBubble(account_info));

  // The primary account has full info.
  MakeValidAccountInfo(&primary_account_info);
  identity_test_env()->UpdateAccountInfoForAccount(primary_account_info);
  // The intercepted account is enterprise.
  EXPECT_TRUE(interceptor()->ShouldShowEnterpriseBubble(account_info));
  // Two consumer accounts.
  account_info =
      AccountInfo::Builder(account_info).SetHostedDomain(std::string()).Build();
  AccountCapabilitiesTestMutator(&account_info)
      .set_is_subject_to_account_level_enterprise_policies(false);
  identity_test_env()->UpdateAccountInfoForAccount(account_info);
  EXPECT_FALSE(interceptor()->ShouldShowEnterpriseBubble(account_info));
  // The primary account is enterprise.
  primary_account_info = AccountInfo::Builder(primary_account_info)
                             .SetHostedDomain("example.com")
                             .Build();
  AccountCapabilitiesTestMutator(&account_info)
      .set_is_subject_to_account_level_enterprise_policies(true);
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
  other_account_info = AccountInfo::Builder(other_account_info)
                           .SetHostedDomain("example.com")
                           .Build();
  AccountCapabilitiesTestMutator(&other_account_info)
      .set_is_subject_to_account_level_enterprise_policies(true);
  identity_test_env()->UpdateAccountInfoForAccount(other_account_info);
  AccountInfo account_info =
      identity_test_env()->MakeAccountAvailable("bob@example.com");
  MakeValidAccountInfo(&account_info);
  identity_test_env()->UpdateAccountInfoForAccount(account_info);
  ASSERT_EQ(identity_test_env()->identity_manager()->GetPrimaryAccountId(
                signin::ConsentLevel::kSignin),
            primary_account_info.GetAccountId());
  interceptor()->state_->new_account_interception_ = true;
  // Consumer account not intercepted.
  EXPECT_FALSE(
      interceptor()->ShouldEnforceEnterpriseProfileSeparation(account_info));
  account_info =
      AccountInfo::Builder(account_info).SetHostedDomain("example.com").Build();
  AccountCapabilitiesTestMutator(&account_info)
      .set_is_subject_to_account_level_enterprise_policies(true);
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
  MakeValidAccountInfo(&account_info_1, "example.com");
  AccountCapabilitiesTestMutator(&account_info_1)
      .set_is_subject_to_account_level_enterprise_policies(true);
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
  MakeValidAccountInfo(&primary_account_info, "example.com");
  AccountCapabilitiesTestMutator(&primary_account_info)
      .set_is_subject_to_account_level_enterprise_policies(true);
  identity_test_env()->UpdateAccountInfoForAccount(primary_account_info);

  // Primary account is set.
  ASSERT_TRUE(identity_test_env()->identity_manager()->HasPrimaryAccount(
      signin::ConsentLevel::kSignin));
  ASSERT_EQ(primary_account_info.CanApplyAccountLevelEnterprisePolicies(),
            signin::Tribool::kTrue);
  EXPECT_TRUE(interceptor()->ShouldEnforceEnterpriseProfileSeparation(
      primary_account_info));

  ProfileAttributesEntry* entry =
      profile_attributes_storage()->GetProfileAttributesWithPath(
          profile()->GetPath());
  entry->SetUserAcceptedAccountManagement(true);

  EXPECT_FALSE(interceptor()->ShouldEnforceEnterpriseProfileSeparation(
      primary_account_info));
}

TEST_F(DiceWebSigninInterceptorTest, ShouldShowEnterpriseDialog_AlwaysAsk) {
  // The enterprise dialog should be shown for a managed account when no account
  // is in the profile, even if the user previously declined.
  AccountInfo account_info =
      identity_test_env()->MakeAccountAvailable("alice@example.com");
  MakeValidAccountInfo(&account_info, "example.com");
  identity_test_env()->UpdateAccountInfoForAccount(account_info);
  ASSERT_FALSE(identity_test_env()->identity_manager()->HasPrimaryAccount(
      signin::ConsentLevel::kSignin));
  ASSERT_EQ(account_info.CanApplyAccountLevelEnterprisePolicies(),
            signin::Tribool::kTrue);

  // Simulate that the user declined profile creation twice.
  const int kMaxProfileCreationDeclinedCount = 2;
  for (int i = 0; i < kMaxProfileCreationDeclinedCount; ++i) {
    interceptor()->IncrementEmailToCountDictionaryPref(
        prefs::kProfileCreationInterceptionDeclined, account_info.GetEmail());
  }
  ASSERT_TRUE(
      interceptor()->HasUserDeclinedProfileCreation(account_info.GetEmail()));

  // The dialog is not shown by default after being declined.
  EXPECT_FALSE(interceptor()->ShouldShowEnterpriseDialog(account_info));

  // The dialog is shown if the user choice is `kAlwaysAsk`.
  SigninPrefs(*profile()->GetPrefs())
      .SetChromeSigninInterceptionUserChoice(
          account_info.GetGaiaId(), ChromeSigninUserChoice::kAlwaysAsk);
  EXPECT_TRUE(interceptor()->ShouldShowEnterpriseDialog(account_info));
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
       NoForcedInterceptionShowsDialog) {
  // Reauth intercepted if enterprise confirmation not shown yet for forced
  // managed separation.
  AccountInfo account_info =
      identity_test_env()->MakeAccountAvailable("alice@example.com");
  MakeValidAccountInfo(&account_info, "example.com");
  identity_test_env()->UpdateAccountInfoForAccount(account_info);
  interceptor()->SetInterceptedAccountProfileSeparationPoliciesForTesting(
      policy::ProfileSeparationPolicies(""));

  WebSigninInterceptor::Delegate::BubbleParameters expected_parameters(
      WebSigninInterceptor::SigninInterceptionType::kEnterpriseAcceptManagement,
      account_info, AccountInfo(), SkColor(),
      /*show_link_data_option=*/true, /*show_managed_disclaimer=*/true);
  EXPECT_CALL(*mock_delegate(),
              ShowSigninInterceptionBubble(
                  web_contents(), MatchBubbleParameters(expected_parameters),
                  testing::_));
  TestSingleAccountAsynchronousInterception(
      account_info, /*is_new_account=*/true, /*is_sync_signin=*/false,
      SigninInterceptionHeuristicOutcome::kInterceptEnterprise);
}

TEST_P(DiceWebSigninInterceptorManagedAccountTest,
       NoForcedInterceptionShowsDialogForReauth) {
  // Reauth intercepted if enterprise confirmation not shown yet for forced
  // managed separation.
  AccountInfo account_info = identity_test_env()->MakePrimaryAccountAvailable(
      "alice@example.com", signin::ConsentLevel::kSignin);
  MakeValidAccountInfo(&account_info, "example.com");
  identity_test_env()->UpdateAccountInfoForAccount(account_info);
  interceptor()->SetInterceptedAccountProfileSeparationPoliciesForTesting(
      policy::ProfileSeparationPolicies(""));

  EXPECT_CALL(*mock_delegate(),
              ShowSigninInterceptionBubble(testing::_, testing::_, testing::_))
      .Times(0);
  TestSingleAccountAsynchronousInterception(
      account_info, /*is_new_account=*/false, /*is_sync_signin=*/false,
      signin_interception_enabled_
          ? SigninInterceptionHeuristicOutcome::kAbortAccountNotNew
          : SigninInterceptionHeuristicOutcome::kAbortInterceptionDisabled);
}

TEST_P(
    DiceWebSigninInterceptorManagedAccountTest,
    NoForcedInterceptionShowsNoDialogIfFeatureEnabledButDisabledDialogByPolicy) {
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
    TestSingleAccountAsynchronousInterception(
        account_info, /*is_new_account=*/true, /*is_sync_signin=*/false,
        SigninInterceptionHeuristicOutcome::kAbortAccountInfoNotCompatible);
  } else {
    TestSingleAccountAsynchronousInterception(
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
    TestSingleAccountAsynchronousInterception(
        account_info, /*is_new_account=*/true, /*is_sync_signin=*/false,
        SigninInterceptionHeuristicOutcome::kAbortAccountInfoNotCompatible);
  } else {
    TestSingleAccountAsynchronousInterception(
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

  TestSingleAccountSynchronousInterception(
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
  TestSingleAccountSynchronousInterception(
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
  TestSingleAccountAsynchronousInterception(
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
  TestSingleAccountSynchronousInterception(
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
  TestSingleAccountSynchronousInterception(
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
  TestSingleAccountSynchronousInterception(
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
  entry->SetAuthInfo(account_info.GetGaiaId(),
                     base::UTF8ToUTF16(account_info.GetEmail()),
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
  TestSingleAccountSynchronousInterception(
      account_info, /*is_new_account=*/true,
      /*is_sync_signin=*/false,
      SigninInterceptionHeuristicOutcome::
          kInterceptEnterpriseForcedProfileSwitch);
}

TEST_P(DiceWebSigninInterceptorManagedAccountTest,
       EnforceManagedAccountSecondaryAccountNotAllowed) {
  base::ListValue profile_separation_exception_list;
  profile_separation_exception_list.Append(base::Value("notexample.com"));
  profile()->GetPrefs()->SetList(prefs::kProfileSeparationDomainExceptionList,
                                 std::move(profile_separation_exception_list));

  AccountInfo primary_account_info =
      identity_test_env()->MakePrimaryAccountAvailable(
          "alice@example.com", signin::ConsentLevel::kSignin);
  MakeValidAccountInfo(&primary_account_info, "example.com");
  identity_test_env()->UpdateAccountInfoForAccount(primary_account_info);

  std::string email = "bob@example.com";
  AccountInfo account_info = identity_test_env()->MakeAccountAvailable(email);
  MakeValidAccountInfo(&account_info, "example.com");
  identity_test_env()->UpdateAccountInfoForAccount(account_info);

  // Check that interception works otherwise, as a sanity check.
  WebSigninInterceptor::Delegate::BubbleParameters expected_parameters(
      WebSigninInterceptor::SigninInterceptionType::kEnterpriseForced,
      account_info, primary_account_info, SkColor(),
      /*show_link_data_option=*/false, /*show_managed_disclaimer=*/true);
  EXPECT_CALL(*mock_delegate(),
              ShowSigninInterceptionBubble(
                  web_contents(), MatchBubbleParameters(expected_parameters),
                  testing::_));
  TestSingleAccountSynchronousInterception(
      account_info, /*is_new_account=*/true, /*is_sync_signin=*/false,
      SigninInterceptionHeuristicOutcome::kInterceptEnterpriseForced);
}

TEST_P(DiceWebSigninInterceptorManagedAccountTest,
       EnforceManagedAccountSecondaryAccountAllowedReauth) {
  base::ListValue profile_separation_exception_list;
  profile_separation_exception_list.Append(base::Value("notexample.com"));
  profile()->GetPrefs()->SetList(prefs::kProfileSeparationDomainExceptionList,
                                 std::move(profile_separation_exception_list));

  AccountInfo primary_account_info =
      identity_test_env()->MakePrimaryAccountAvailable(
          "alice@example.com", signin::ConsentLevel::kSignin);
  MakeValidAccountInfo(&primary_account_info, "example.com");
  identity_test_env()->UpdateAccountInfoForAccount(primary_account_info);

  TestSingleAccountSynchronousInterception(
      primary_account_info, /*is_new_account=*/false, /*is_sync_signin=*/false,
      profile()->GetPrefs()->GetBoolean(prefs::kSigninInterceptionEnabled)
          ? SigninInterceptionHeuristicOutcome::kAbortAccountNotNew
          : SigninInterceptionHeuristicOutcome::kAbortInterceptionDisabled);
}

TEST_P(DiceWebSigninInterceptorManagedAccountTest,
       EnforceManagedAccountSecondaryAccountNotAllowedReauth) {
  base::ListValue profile_separation_exception_list;
  profile_separation_exception_list.Append(base::Value("notexample.com"));
  profile()->GetPrefs()->SetList(prefs::kProfileSeparationDomainExceptionList,
                                 std::move(profile_separation_exception_list));

  AccountInfo primary_account_info =
      identity_test_env()->MakePrimaryAccountAvailable(
          "alice@example.com", signin::ConsentLevel::kSignin);
  MakeValidAccountInfo(&primary_account_info, "example.com");
  identity_test_env()->UpdateAccountInfoForAccount(primary_account_info);

  std::string email = "bob@example.com";
  AccountInfo account_info = identity_test_env()->MakeAccountAvailable(email);
  MakeValidAccountInfo(&account_info, "example.com");
  identity_test_env()->UpdateAccountInfoForAccount(account_info);

  // Check that interception works otherwise, as a sanity check.
  WebSigninInterceptor::Delegate::BubbleParameters expected_parameters(
      WebSigninInterceptor::SigninInterceptionType::kEnterpriseForced,
      account_info, primary_account_info, SkColor(),
      /*show_link_data_option=*/false, /*show_managed_disclaimer=*/true);
  EXPECT_CALL(*mock_delegate(),
              ShowSigninInterceptionBubble(
                  web_contents(), MatchBubbleParameters(expected_parameters),
                  testing::_));
  TestSingleAccountSynchronousInterception(
      account_info, /*is_new_account=*/false, /*is_sync_signin=*/false,
      SigninInterceptionHeuristicOutcome::kInterceptEnterpriseForced);
}

TEST_P(DiceWebSigninInterceptorManagedAccountTest,
       EnforceManagedAccountSecondaryConsumerAccountNotAllowed) {
  base::ListValue profile_separation_exception_list;
  profile_separation_exception_list.Append(base::Value("example.com"));
  profile()->GetPrefs()->SetList(prefs::kProfileSeparationDomainExceptionList,
                                 std::move(profile_separation_exception_list));

  AccountInfo primary_account_info =
      identity_test_env()->MakePrimaryAccountAvailable(
          "alice@example.com", signin::ConsentLevel::kSignin);
  MakeValidAccountInfo(&primary_account_info, "example.com");
  identity_test_env()->UpdateAccountInfoForAccount(primary_account_info);

  std::string email = "bob@gmail.com";
  AccountInfo account_info = identity_test_env()->MakeAccountAvailable(email);
  MakeValidAccountInfo(&account_info);
  identity_test_env()->UpdateAccountInfoForAccount(account_info);

  // Check that interception works otherwise, as a sanity check.
  WebSigninInterceptor::Delegate::BubbleParameters expected_parameters(
      WebSigninInterceptor::SigninInterceptionType::kEnterpriseForced,
      account_info, primary_account_info, SkColor(),
      /*show_link_data_option=*/false, /*show_managed_disclaimer=*/false);
  EXPECT_CALL(*mock_delegate(),
              ShowSigninInterceptionBubble(
                  web_contents(), MatchBubbleParameters(expected_parameters),
                  testing::_));
  TestSingleAccountSynchronousInterception(
      account_info, /*is_new_account=*/true, /*is_sync_signin=*/false,
      SigninInterceptionHeuristicOutcome::kInterceptEnterpriseForced);
}

TEST_P(DiceWebSigninInterceptorManagedAccountTest,
       EnforceManagedAccountSecondaryAccountAllowed) {
  base::ListValue profile_separation_exception_list;
  profile_separation_exception_list.Append(base::Value("gmail.com"));
  profile()->GetPrefs()->SetList(prefs::kProfileSeparationDomainExceptionList,
                                 std::move(profile_separation_exception_list));

  AccountInfo primary_account_info =
      identity_test_env()->MakePrimaryAccountAvailable(
          "alice@example.com", signin::ConsentLevel::kSignin);
  MakeValidAccountInfo(&primary_account_info, "example.com");
  identity_test_env()->UpdateAccountInfoForAccount(primary_account_info);

  std::string email = "bob@gmail.com";
  AccountInfo account_info = identity_test_env()->MakeAccountAvailable(email);
  MakeValidAccountInfo(&account_info);
  identity_test_env()->UpdateAccountInfoForAccount(account_info);

  if (!profile()->GetPrefs()->GetBoolean(prefs::kSigninInterceptionEnabled)) {
    TestSingleAccountSynchronousInterception(
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
  TestSingleAccountAsynchronousInterception(
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
  account_info_1 = AccountInfo::Builder(account_info_1)
                       .SetHostedDomain("example.com")
                       .Build();
  {
    AccountCapabilitiesTestMutator(&account_info_1)
        .set_is_subject_to_account_level_enterprise_policies(true);
  }
  identity_test_env()->UpdateAccountInfoForAccount(account_info_1);
  AccountInfo account_info_2 =
      identity_test_env()->MakeAccountAvailable("alice@example.com");
  MakeValidAccountInfo(&account_info_2);
  account_info_2 = AccountInfo::Builder(account_info_2)
                       .SetHostedDomain("example.com")
                       .Build();
  AccountCapabilitiesTestMutator(&account_info_2)
      .set_is_subject_to_account_level_enterprise_policies(true);
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
  account_info_1 =
      AccountInfo::Builder(account_info_1).SetGivenName("Bob").Build();
  identity_test_env()->UpdateAccountInfoForAccount(account_info_1);
  AccountInfo account_info_2 =
      identity_test_env()->MakeAccountAvailable("alice@example.com");

  // The other account does not have full account info (empty name).
  ASSERT_FALSE(account_info_2.GetGivenName().has_value());
  EXPECT_TRUE(interceptor()->ShouldShowMultiUserBubble(account_info_1));

  // Accounts with different names.
  account_info_1 =
      AccountInfo::Builder(account_info_1).SetGivenName("Bob").Build();
  identity_test_env()->UpdateAccountInfoForAccount(account_info_1);
  MakeValidAccountInfo(&account_info_2);
  account_info_2 =
      AccountInfo::Builder(account_info_2).SetGivenName("Alice").Build();
  identity_test_env()->UpdateAccountInfoForAccount(account_info_2);
  EXPECT_TRUE(interceptor()->ShouldShowMultiUserBubble(account_info_1));

  // Accounts with same names.
  account_info_1 =
      AccountInfo::Builder(account_info_1).SetGivenName("Alice").Build();
  identity_test_env()->UpdateAccountInfoForAccount(account_info_1);
  EXPECT_FALSE(interceptor()->ShouldShowMultiUserBubble(account_info_1));

  // Comparison is case insensitive.
  account_info_1 =
      AccountInfo::Builder(account_info_1).SetGivenName("alice").Build();
  identity_test_env()->UpdateAccountInfoForAccount(account_info_1);
  EXPECT_FALSE(interceptor()->ShouldShowMultiUserBubble(account_info_1));
}

TEST_F(DiceWebSigninInterceptorTest,
       ShouldShowMultiUserBubbleNoPrimaryAccount) {
  // Setup two accounts in the profile.
  AccountInfo account_info_1 =
      identity_test_env()->MakeAccountAvailable("bob@example.com");
  MakeValidAccountInfo(&account_info_1);
  account_info_1 =
      AccountInfo::Builder(account_info_1).SetGivenName("Bob").Build();
  identity_test_env()->UpdateAccountInfoForAccount(account_info_1);
  AccountInfo account_info_2 =
      identity_test_env()->MakeAccountAvailable("alice@example.com");
  account_info_2 =
      AccountInfo::Builder(account_info_2).SetGivenName("Alice").Build();
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
  entry->SetAuthInfo(account_info.GetGaiaId(), base::UTF8ToUTF16(email),
                     /*is_consented_primary_account=*/false);

  // Suppress the signin bubble.
  SigninPrefs(*profile()->GetPrefs())
      .SetChromeSigninInterceptionUserChoice(
          account_info.GetGaiaId(), ChromeSigninUserChoice::kDoNotSignin);

  // Check that Sync signin is not intercepted.
  {
    SCOPED_TRACE("Sync signin is not intercepted");
    TestSingleAccountSynchronousInterception(
        account_info, /*is_new_account=*/true, /*is_sync_signin=*/true,
        SigninInterceptionHeuristicOutcome::kAbortSyncSignin);
  }

  // Check that reauth is not intercepted.
  {
    SCOPED_TRACE("Reauth is not intercepted");
    TestSingleAccountSynchronousInterception(
        account_info, /*is_new_account=*/false, /*is_sync_signin=*/false,
        SigninInterceptionHeuristicOutcome::kAbortAccountNotNew);
  }

  // Check that primary_is_connected == kTrue is not intercepted.
  {
    SCOPED_TRACE(
        "Connected account (primary_is_connected == kTrue) is not intercepted");
    TestLinkedAccountsSynchronousInterception(
        account_info, /*is_new_account=*/true, /*is_sync_signin=*/false,
        /*primary_is_connected=*/signin::Tribool::kTrue,
        SigninInterceptionHeuristicOutcome::kAbortAccountConnected);
  }

  // Check that interception works otherwise, as a sanity check.
  {
    SCOPED_TRACE("Sanity check: normal profile switch interception");
    WebSigninInterceptor::Delegate::BubbleParameters expected_parameters(
        WebSigninInterceptor::SigninInterceptionType::kProfileSwitch,
        account_info, AccountInfo());
    EXPECT_CALL(*mock_delegate(),
                ShowSigninInterceptionBubble(
                    web_contents(), MatchBubbleParameters(expected_parameters),
                    testing::_));
    TestSingleAccountSynchronousInterception(
        account_info, /*is_new_account=*/true, /*is_sync_signin=*/false,
        SigninInterceptionHeuristicOutcome::kInterceptProfileSwitch);
  }
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
  entry->SetAuthInfo(GaiaId("dummy_gaia_id"), base::UTF8ToUTF16(email),
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
  entry->SetAuthInfo(GaiaId("dummy_gaia_id"), base::UTF8ToUTF16(email),
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
  entry->SetAuthInfo(GaiaId("dummy_gaia_id"), base::UTF8ToUTF16(email),
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
          /*is_new_account=*/true, /*is_sync_signin=*/false, "bob@example.com",
          GaiaId(), nullptr, /*primary_is_connected=*/signin::Tribool::kFalse),
      SigninInterceptionHeuristicOutcome::kAbortInterceptionDisabled);
}

TEST_F(DiceWebSigninInterceptorTest, TabClosed) {
  base::HistogramTester histogram_tester;
  interceptor()->MaybeInterceptWebSignin(
      /*web_contents=*/nullptr, CoreAccountId(),
      signin_metrics::AccessPoint::kWebSignin,
      /*is_new_account=*/true, /*is_sync_signin=*/false,
      /*primary_is_connected=*/signin::Tribool::kUnknown);
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
  entry->SetAuthInfo(account_info.GetGaiaId(), base::UTF8ToUTF16(email),
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
      .WillOnce(testing::WithArg<2>(
          [&delegate_callback](
              base::OnceCallback<void(SigninInterceptionResult)> callback) {
            delegate_callback = std::move(callback);
            return nullptr;
          }));
  MaybeIntercept(account_info.GetAccountId());
  testing::Mock::VerifyAndClearExpectations(mock_delegate());
  EXPECT_TRUE(interceptor()->is_interception_in_progress());

  // Check that there is no interception while another one is in progress.
  base::HistogramTester histogram_tester;
  MaybeIntercept(account_info.GetAccountId());
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
  MaybeIntercept(account_info.GetAccountId());
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
        .WillOnce(testing::WithArg<2>(
            [](base::OnceCallback<void(SigninInterceptionResult)> callback) {
              std::move(callback).Run(SigninInterceptionResult::kDeclined);
              return nullptr;
            }));
    MaybeIntercept(account_info.GetAccountId());
    EXPECT_EQ(interceptor()->is_interception_in_progress(), false);
    histogram_tester.ExpectUniqueSample(
        "Signin.Intercept.HeuristicOutcome",
        SigninInterceptionHeuristicOutcome::kInterceptEnterprise, i + 1);
  }

  // Next time the interception is not shown again.
  MaybeIntercept(account_info.GetAccountId());
  EXPECT_EQ(interceptor()->is_interception_in_progress(), false);
  histogram_tester.ExpectBucketCount(
      "Signin.Intercept.HeuristicOutcome",
      SigninInterceptionHeuristicOutcome::kAbortUserDeclinedProfileForAccount,
      1);
  EXPECT_EQ(
      interceptor()->GetHeuristicOutcome(
          /*is_new_account=*/true, /*is_sync_signin=*/false,
          account_info.GetEmail(), account_info.GetGaiaId(), nullptr,
          /*primary_is_connected=*/signin::Tribool::kFalse),
      SigninInterceptionHeuristicOutcome::kAbortUserDeclinedProfileForAccount);

  // Another account can still be intercepted.
  account_info =
      AccountInfo::Builder(account_info).SetEmail("oscar@example.com").Build();
  identity_test_env()->UpdateAccountInfoForAccount(account_info);
  expected_parameters.intercepted_account = account_info;
  EXPECT_CALL(*mock_delegate(),
              ShowSigninInterceptionBubble(
                  web_contents(), MatchBubbleParameters(expected_parameters),
                  testing::_));
  MaybeIntercept(account_info.GetAccountId());
  histogram_tester.ExpectBucketCount(
      "Signin.Intercept.HeuristicOutcome",
      SigninInterceptionHeuristicOutcome::kInterceptEnterprise,
      kMaxProfileCreationDeclinedCount + 1);
  EXPECT_EQ(interceptor()->is_interception_in_progress(), true);
}

// Regression test for https://crbug.com/40829908
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
        .WillOnce(testing::WithArg<2>(
            [](base::OnceCallback<void(SigninInterceptionResult)> callback) {
              std::move(callback).Run(SigninInterceptionResult::kDeclined);
              return nullptr;
            }));
    MaybeIntercept(account_info.GetAccountId());
    EXPECT_EQ(interceptor()->is_interception_in_progress(), false);
    histogram_tester.ExpectUniqueSample(
        "Signin.Intercept.HeuristicOutcome",
        SigninInterceptionHeuristicOutcome::kInterceptEnterprise, i + 1);
  }

  // Next time the interception is not shown again.
  MaybeIntercept(account_info.GetAccountId());
  EXPECT_EQ(interceptor()->is_interception_in_progress(), false);
  histogram_tester.ExpectBucketCount(
      "Signin.Intercept.HeuristicOutcome",
      SigninInterceptionHeuristicOutcome::kAbortUserDeclinedProfileForAccount,
      1);
  EXPECT_EQ(
      interceptor()->GetHeuristicOutcome(
          /*is_new_account=*/true, /*is_sync_signin=*/false,
          account_info.GetEmail(), account_info.GetGaiaId(), nullptr,
          /*primary_is_connected=*/signin::Tribool::kFalse),
      SigninInterceptionHeuristicOutcome::kAbortUserDeclinedProfileForAccount);

  // Another account can still be intercepted.
  account_info =
      AccountInfo::Builder(account_info).SetEmail("oscar@example.com").Build();
  identity_test_env()->UpdateAccountInfoForAccount(account_info);
  expected_parameters.intercepted_account = account_info;
  EXPECT_CALL(*mock_delegate(),
              ShowSigninInterceptionBubble(
                  web_contents(), MatchBubbleParameters(expected_parameters),
                  testing::_));
  MaybeIntercept(account_info.GetAccountId());
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
  entry->SetAuthInfo(account_info.GetGaiaId(), base::UTF8ToUTF16(email),
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
        .WillOnce(testing::WithArg<2>(
            [](base::OnceCallback<void(SigninInterceptionResult)> callback) {
              std::move(callback).Run(SigninInterceptionResult::kDeclined);
              return nullptr;
            }));
    MaybeIntercept(account_info.GetAccountId());
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
  ASSERT_FALSE(
      identity_test_env()
          ->identity_manager()
          ->FindExtendedAccountInfoByAccountId(account_info.GetAccountId())
          .IsValid());
  // Suppress the signin bubble.
  SigninPrefs(*profile()->GetPrefs())
      .SetChromeSigninInterceptionUserChoice(
          account_info.GetGaiaId(), ChromeSigninUserChoice::kDoNotSignin);

  TestSingleAccountSynchronousInterception(
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
  entry->SetAuthInfo(account_info.GetGaiaId(), base::UTF8ToUTF16(email),
                     /*is_consented_primary_account=*/false);

  // Suppress the signin bubble.
  SigninPrefs(*profile()->GetPrefs())
      .SetChromeSigninInterceptionUserChoice(
          other_account_info.GetGaiaId(), ChromeSigninUserChoice::kDoNotSignin);

  // Interception that would offer creating a new profile does not work,
  // even when primary_is_connected == kFalse explicitly demands separation.
  TestLinkedAccountsSynchronousInterception(
      other_account_info, /*is_new_account=*/true, /*is_sync_signin=*/false,
      /*primary_is_connected=*/signin::Tribool::kFalse,
      SigninInterceptionHeuristicOutcome::kAbortProfileCreationDisallowed);

  // Profile switch interception still works.
  WebSigninInterceptor::Delegate::BubbleParameters expected_parameters(
      WebSigninInterceptor::SigninInterceptionType::kProfileSwitch,
      account_info, AccountInfo());
  EXPECT_CALL(*mock_delegate(),
              ShowSigninInterceptionBubble(
                  web_contents(), MatchBubbleParameters(expected_parameters),
                  testing::_));
  MaybeIntercept(account_info.GetAccountId());
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
                                         account_info.GetEmail())
                   .has_value());
  MaybeIntercept(account_info.GetAccountId());
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
  MaybeIntercept(account_info.GetAccountId());
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
  MaybeIntercept(account_info.GetAccountId());
  histogram_tester.ExpectUniqueSample(
      "Signin.Intercept.HeuristicOutcome",
      SigninInterceptionHeuristicOutcome::kInterceptMultiUser, 1);
}

TEST_F(DiceWebSigninInterceptorTest,
       MultiUserInterceptionPrimaryNotConnectedSameName) {
  base::HistogramTester histogram_tester;
  AccountInfo primary_account_info =
      identity_test_env()->MakePrimaryAccountAvailable(
          "bob@example.com", signin::ConsentLevel::kSignin);
  MakeValidAccountInfo(&primary_account_info);
  primary_account_info =
      AccountInfo::Builder(primary_account_info).SetGivenName("Bob").Build();
  identity_test_env()->UpdateAccountInfoForAccount(primary_account_info);

  AccountInfo account_info =
      identity_test_env()->MakeAccountAvailable("bob.work@example.com");
  MakeValidAccountInfo(&account_info);
  account_info = AccountInfo::Builder(account_info).SetGivenName("Bob").Build();
  identity_test_env()->UpdateAccountInfoForAccount(account_info);

  // When primary_is_connected is kUnknown (default), same given names abort
  // interception with kAbortAccountInfoNotCompatible after waiting for info.
  {
    SCOPED_TRACE("kUnknown aborts when given names match");
    TestLinkedAccountsAsynchronousInterception(
        account_info, /*is_new_account=*/true, /*is_sync_signin=*/false,
        /*primary_is_connected=*/signin::Tribool::kUnknown,
        SigninInterceptionHeuristicOutcome::kAbortAccountInfoNotCompatible);
  }

  interceptor()->Reset();

  // When primary_is_connected is kFalse, GetHeuristicOutcome synchronously
  // returns kInterceptMultiUser and interception is enforced despite same given
  // name.
  // Note: Asynchronous heuristic evaluation cannot occur for
  // signin::Tribool::kFalse because GetHeuristicOutcome() synchronously returns
  // kInterceptMultiUser (bypassing given-name check).
  {
    SCOPED_TRACE(
        "kFalse enforces multi-user interception despite matching given names");
    WebSigninInterceptor::Delegate::BubbleParameters expected_parameters(
        WebSigninInterceptor::SigninInterceptionType::kMultiUser, account_info,
        primary_account_info);
    EXPECT_CALL(*mock_delegate(),
                ShowSigninInterceptionBubble(
                    web_contents(), MatchBubbleParameters(expected_parameters),
                    testing::_));
    TestLinkedAccountsSynchronousInterception(
        account_info, /*is_new_account=*/true, /*is_sync_signin=*/false,
        /*primary_is_connected=*/signin::Tribool::kFalse,
        SigninInterceptionHeuristicOutcome::kInterceptMultiUser);
  }
}

TEST_F(DiceWebSigninInterceptorTest,
       InterceptionPrimaryNotConnectedNoPrimaryAccount) {
  AccountInfo account_info =
      identity_test_env()->MakeAccountAvailable("alice@example.com");
  MakeValidAccountInfo(&account_info);
  identity_test_env()->UpdateAccountInfoForAccount(account_info);

  // Account is valid and primary account is not set (Chrome is unsigned in).
  ASSERT_TRUE(account_info.IsValid());
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
      web_contents(), account_info.GetAccountId(),
      signin_metrics::AccessPoint::kWebSignin,
      /*is_new_account=*/true, /*is_sync_signin=*/false,
      /*primary_is_connected=*/signin::Tribool::kFalse);
  EXPECT_EQ(interceptor()->GetHeuristicOutcome(
                /*is_new_account=*/true, /*is_sync_signin=*/false,
                account_info.GetEmail(), account_info.GetGaiaId(), nullptr,
                /*primary_is_connected=*/signin::Tribool::kFalse),
            expected_outcome);
  testing::Mock::VerifyAndClearExpectations(mock_delegate());
  histogram_tester.ExpectUniqueSample("Signin.Intercept.HeuristicOutcome",
                                      expected_outcome, 1);

  EXPECT_EQ(interceptor()->is_interception_in_progress(),
            SigninInterceptionHeuristicOutcomeIsSuccess(expected_outcome));
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
  MaybeIntercept(account_info.GetAccountId());
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
                                         account_info.GetEmail())
                   .has_value());
  MaybeIntercept(account_info.GetAccountId());
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
                                         account_info.GetEmail())
                   .has_value());
  MaybeIntercept(account_info.GetAccountId());
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
                                         account_info.GetEmail())
                   .has_value());
  MaybeIntercept(account_info.GetAccountId());
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
                                         account_info.GetEmail())
                   .has_value());
  MaybeIntercept(account_info.GetAccountId());
  // Delegate was not called yet, interception is in progress.
  testing::Mock::VerifyAndClearExpectations(mock_delegate());
  EXPECT_TRUE(interceptor()->is_interception_in_progress());

  // Clear primary account.
  identity_test_env()->EnableRemovalOfExtendedAccountInfo();
  identity_test_env()->RemoveRefreshTokenForAccount(
      account_info.GetAccountId());

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
                                         account_info.GetEmail())
                   .has_value());
  MaybeIntercept(account_info.GetAccountId());

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
  base::ListValue profile_separation_exception_list;
  profile_separation_exception_list.Append(base::Value("notexample.com"));
  profile()->GetPrefs()->SetList(prefs::kProfileSeparationDomainExceptionList,
                                 std::move(profile_separation_exception_list));

  base::HistogramTester histogram_tester;
  AccountInfo account_info =
      identity_test_env()->MakeAccountAvailable("alice@gmail.com");
  MakeValidAccountInfo(&account_info);
  identity_test_env()->UpdateAccountInfoForAccount(account_info);

  // Account info is already available, interception happens immediately.
  WebSigninInterceptor::Delegate::BubbleParameters expected_parameters(
      WebSigninInterceptor::SigninInterceptionType::kEnterpriseForced,
      account_info, AccountInfo(), SkColor(),
      /*show_link_data_option=*/true, /*show_managed_disclaimer=*/false);
  base::OnceCallback<void(SigninInterceptionResult)> bubble_callback;
  EXPECT_CALL(*mock_delegate(),
              ShowSigninInterceptionBubble(
                  web_contents(), MatchBubbleParameters(expected_parameters),
                  testing::_))
      .WillOnce(testing::WithArg<2>(
          [&bubble_callback](
              base::OnceCallback<void(SigninInterceptionResult)> callback) {
            bubble_callback = std::move(callback);
            return std::make_unique<
                TestScopedWebSigninInterceptionBubbleHandle>();
          }));
  MaybeIntercept(account_info.GetAccountId());
  histogram_tester.ExpectUniqueSample(
      "Signin.Intercept.HeuristicOutcome",
      SigninInterceptionHeuristicOutcome::kInterceptEnterpriseForced, 1);

  // Signin.SignIn.Offered should be recorded for enterprise.
  histogram_tester.ExpectUniqueSample(
      "Signin.SignIn.Offered",
      signin_metrics::AccessPoint::kEnterpriseDialogAfterSigninInterception, 1);

  ASSERT_TRUE(bubble_callback);
  // Signin.SignIn.Started should not be recorded yet.
  histogram_tester.ExpectTotalCount("Signin.SignIn.Started", 0);

  // Simulate user accepting the bubble.
  std::move(bubble_callback).Run(SigninInterceptionResult::kAccepted);

  // Signin.SignIn.Started should now be recorded.
  histogram_tester.ExpectUniqueSample(
      "Signin.SignIn.Started",
      signin_metrics::AccessPoint::kEnterpriseDialogAfterSigninInterception, 1);
}

TEST_F(
    DiceWebSigninInterceptorTest,
    ConsumerAccountForcedEnterpriseInterceptionOnEmptyProfile_AcceptWithExistingProfile) {
  base::ListValue profile_separation_exception_list;
  profile_separation_exception_list.Append(base::Value("notexample.com"));
  profile()->GetPrefs()->SetList(prefs::kProfileSeparationDomainExceptionList,
                                 std::move(profile_separation_exception_list));

  base::HistogramTester histogram_tester;
  AccountInfo account_info =
      identity_test_env()->MakeAccountAvailable("alice@gmail.com");
  MakeValidAccountInfo(&account_info);
  identity_test_env()->UpdateAccountInfoForAccount(account_info);

  // Account info is already available, interception happens immediately.
  WebSigninInterceptor::Delegate::BubbleParameters expected_parameters(
      WebSigninInterceptor::SigninInterceptionType::kEnterpriseForced,
      account_info, AccountInfo(), SkColor(),
      /*show_link_data_option=*/true, /*show_managed_disclaimer=*/false);
  base::OnceCallback<void(SigninInterceptionResult)> bubble_callback;
  EXPECT_CALL(*mock_delegate(),
              ShowSigninInterceptionBubble(
                  web_contents(), MatchBubbleParameters(expected_parameters),
                  testing::_))
      .WillOnce(testing::WithArg<2>(
          [&bubble_callback](
              base::OnceCallback<void(SigninInterceptionResult)> callback) {
            bubble_callback = std::move(callback);
            return std::make_unique<
                TestScopedWebSigninInterceptionBubbleHandle>();
          }));
  MaybeIntercept(account_info.GetAccountId());
  histogram_tester.ExpectUniqueSample(
      "Signin.Intercept.HeuristicOutcome",
      SigninInterceptionHeuristicOutcome::kInterceptEnterpriseForced, 1);

  // Signin.SignIn.Offered should be recorded for enterprise.
  histogram_tester.ExpectUniqueSample(
      "Signin.SignIn.Offered",
      signin_metrics::AccessPoint::kEnterpriseDialogAfterSigninInterception, 1);

  ASSERT_TRUE(bubble_callback);
  // Signin.SignIn.Started should not be recorded yet.
  histogram_tester.ExpectTotalCount("Signin.SignIn.Started", 0);

  // Simulate user accepting the bubble with existing profile.
  std::move(bubble_callback)
      .Run(SigninInterceptionResult::kAcceptedWithExistingProfile);

  // Signin.SignIn.Started should now be recorded under
  // kEnterpriseDialogAfterSigninInterception.
  histogram_tester.ExpectUniqueSample(
      "Signin.SignIn.Started",
      signin_metrics::AccessPoint::kEnterpriseDialogAfterSigninInterception, 1);
}

TEST_F(DiceWebSigninInterceptorTest, ConsumerAccountAllowedOnEmptyProfile) {
  base::ListValue profile_separation_exception_list;
  profile_separation_exception_list.Append(base::Value("gmail.com"));
  profile()->GetPrefs()->SetList(prefs::kProfileSeparationDomainExceptionList,
                                 std::move(profile_separation_exception_list));

  base::HistogramTester histogram_tester;
  AccountInfo account_info =
      identity_test_env()->MakeAccountAvailable("alice@gmail.com");
  MakeValidAccountInfo(&account_info);
  identity_test_env()->UpdateAccountInfoForAccount(account_info);

  // Suppress the signin bubble.
  SigninPrefs(*profile()->GetPrefs())
      .SetChromeSigninInterceptionUserChoice(
          account_info.GetGaiaId(), ChromeSigninUserChoice::kDoNotSignin);

  MaybeIntercept(account_info.GetAccountId());
  histogram_tester.ExpectUniqueSample(
      "Signin.Intercept.HeuristicOutcome",
      SigninInterceptionHeuristicOutcome::kAbortSingleAccount, 1);
}

TEST_F(DiceWebSigninInterceptorTest,
       ConsumerAccountForcedEnterpriseInterceptionOnManagedProfile) {
  base::ListValue profile_separation_exception_list;
  profile_separation_exception_list.Append(base::Value("notexample.com"));
  profile()->GetPrefs()->SetList(prefs::kProfileSeparationDomainExceptionList,
                                 std::move(profile_separation_exception_list));

  base::HistogramTester histogram_tester;
  AccountInfo primary_account_info =
      identity_test_env()->MakePrimaryAccountAvailable(
          "bob@example.com", signin::ConsentLevel::kSignin);
  primary_account_info = AccountInfo::Builder(primary_account_info)
                             .SetHostedDomain("example.com")
                             .Build();
  AccountCapabilitiesTestMutator(&primary_account_info)
      .set_is_subject_to_account_level_enterprise_policies(true);
  identity_test_env()->UpdateAccountInfoForAccount(primary_account_info);

  AccountInfo account_info =
      identity_test_env()->MakeAccountAvailable("alice@gmail.com");
  MakeValidAccountInfo(&account_info);
  identity_test_env()->UpdateAccountInfoForAccount(account_info);

  // Account info is already available, interception happens immediately.
  WebSigninInterceptor::Delegate::BubbleParameters expected_parameters(
      WebSigninInterceptor::SigninInterceptionType::kEnterpriseForced,
      account_info, primary_account_info, SkColor(),
      /*show_link_data_option=*/false, /*show_managed_disclaimer=*/false);
  EXPECT_CALL(*mock_delegate(),
              ShowSigninInterceptionBubble(
                  web_contents(), MatchBubbleParameters(expected_parameters),
                  testing::_));
  MaybeIntercept(account_info.GetAccountId());
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
  signin::Tribool IsSupervisedUser() { return std::get<0>(GetParam()); }
  WebSigninInterceptor::SigninInterceptionType GetInterceptionType() {
    return std::get<1>(GetParam());
  }
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
  AccountCapabilitiesTestMutator mutator(&intercepted_account_info);
  mutator.set_is_subject_to_account_level_enterprise_policies(false);
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
    entry->SetAuthInfo(intercepted_account_info.GetGaiaId(),
                       base::UTF8ToUTF16(intercepted_account_email),
                       /*is_consented_primary_account=*/false);
  }

  WebSigninInterceptor::Delegate::BubbleParameters expected_parameters(
      GetInterceptionType(), intercepted_account_info, other_account_info);
  EXPECT_CALL(*mock_delegate(),
              ShowSigninInterceptionBubble(
                  web_contents(), MatchBubbleParameters(expected_parameters),
                  testing::_));
  MaybeIntercept(intercepted_account_info.GetAccountId());

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

TEST_F(DiceWebSigninInterceptorTest,
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
      web_contents(), account_info.GetAccountId(),
      signin_metrics::AccessPoint::kWebSignin,
      /*is_new_account=*/true, /*is_sync_signin=*/false,
      /*primary_is_connected=*/signin::Tribool::kUnknown);
  EXPECT_EQ(interceptor()->GetHeuristicOutcome(
                /*is_new_account=*/true,
                /*is_sync_signin=*/false, account_info.GetEmail(),
                account_info.GetGaiaId()),
            expected_outcome);
  testing::Mock::VerifyAndClearExpectations(mock_delegate());
  histogram_tester.ExpectUniqueSample("Signin.Intercept.HeuristicOutcome",
                                      expected_outcome, 1);

  EXPECT_EQ(interceptor()->is_interception_in_progress(),
            SigninInterceptionHeuristicOutcomeIsSuccess(expected_outcome));
}

TEST_F(DiceWebSigninInterceptorTest,
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
      web_contents(), account_info.GetAccountId(),
      signin_metrics::AccessPoint::kWebSignin,
      /*is_new_account=*/false, /*is_sync_signin=*/false,
      /*primary_is_connected=*/signin::Tribool::kUnknown);
  EXPECT_EQ(interceptor()->GetHeuristicOutcome(
                /*is_new_account=*/true,
                /*is_sync_signin=*/false, account_info.GetEmail(),
                account_info.GetGaiaId()),
            expected_outcome);
  testing::Mock::VerifyAndClearExpectations(mock_delegate());
  histogram_tester.ExpectUniqueSample("Signin.Intercept.HeuristicOutcome",
                                      expected_outcome, 1);

  EXPECT_EQ(interceptor()->is_interception_in_progress(),
            SigninInterceptionHeuristicOutcomeIsSuccess(expected_outcome));
}

TEST_F(DiceWebSigninInterceptorTest, EnforceManagedAccountAsPrimaryReauth) {
  interceptor()->SetInterceptedAccountProfileSeparationPoliciesForTesting(
      policy::ProfileSeparationPolicies(
          policy::ProfileSeparationSettings::ENFORCED, std::nullopt));

  // Reauth intercepted if enterprise confirmation not shown yet for forced
  // managed separation.
  AccountInfo account_info = identity_test_env()->MakePrimaryAccountAvailable(
      "alice@example.com", signin::ConsentLevel::kSignin);
  MakeValidAccountInfo(&account_info, "example.com");
  identity_test_env()->UpdateAccountInfoForAccount(account_info);

  // Check that interception works otherwise, as a sanity check.
  WebSigninInterceptor::Delegate::BubbleParameters expected_parameters(
      WebSigninInterceptor::SigninInterceptionType::kEnterpriseForced,
      account_info, account_info, SkColor(),
      /*show_link_data_option=*/true, /*show_managed_disclaimer=*/true);
  EXPECT_CALL(*mock_delegate(),
              ShowSigninInterceptionBubble(
                  web_contents(), MatchBubbleParameters(expected_parameters),
                  testing::_));

  TestSingleAccountAsynchronousInterception(
      account_info, /*is_new_account=*/false, /*is_sync_signin=*/false,
      SigninInterceptionHeuristicOutcome::kInterceptEnterpriseForced);
}

TEST_F(DiceWebSigninInterceptorTest,
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
      web_contents(), account_info.GetAccountId(),
      signin_metrics::AccessPoint::kWebSignin,
      /*is_new_account=*/true, /*is_sync_signin=*/false,
      /*primary_is_connected=*/signin::Tribool::kUnknown);
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

  EXPECT_EQ(interceptor()->is_interception_in_progress(),
            SigninInterceptionHeuristicOutcomeIsSuccess(expected_outcome));
}

TEST_F(DiceWebSigninInterceptorTest,
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
      web_contents(), account_info.GetAccountId(),
      signin_metrics::AccessPoint::kWebSignin,
      /*is_new_account=*/true, /*is_sync_signin=*/false,
      /*primary_is_connected=*/signin::Tribool::kUnknown);
  EXPECT_EQ(interceptor()->GetHeuristicOutcome(
                /*is_new_account=*/true,
                /*is_sync_signin=*/false, account_info.GetEmail(),
                account_info.GetGaiaId()),
            expected_outcome);
  testing::Mock::VerifyAndClearExpectations(mock_delegate());
  histogram_tester.ExpectUniqueSample("Signin.Intercept.HeuristicOutcome",
                                      expected_outcome, 1);

  EXPECT_EQ(interceptor()->is_interception_in_progress(),
            SigninInterceptionHeuristicOutcomeIsSuccess(expected_outcome));
}

class DiceWebSigninInterceptorTestWithAccountPreview
    : public DiceWebSigninInterceptorTest,
      public testing::WithParamInterface<
          WebSigninInterceptor::SigninInterceptionType> {
 public:
  DiceWebSigninInterceptorTestWithAccountPreview() {
    feature_list_.InitAndEnableFeature(
        switches::kEnableAccountPreviewPreferredAccount);
  }

  WebSigninInterceptor::SigninInterceptionType GetInterceptionType() const {
    return GetParam();
  }

  AccountInfo SetUpPrimaryAccountIfNeeded() {
    if (GetInterceptionType() ==
        WebSigninInterceptor::SigninInterceptionType::kMultiUser) {
      return identity_test_env()->MakePrimaryAccountAvailable(
          "primary@example.com", signin::ConsentLevel::kSignin);
    }
    return AccountInfo();
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

TEST_P(DiceWebSigninInterceptorTestWithAccountPreview,
       InterceptBubbleWithAccountPreviewData) {
  base::HistogramTester histogram_tester;
  auto fake_service = std::make_unique<signin::TestAccountPreviewDataService>();
  fake_service->set_defer_callbacks(true);
  signin::TestAccountPreviewDataService* raw_fake_service = fake_service.get();
  AccountPreviewDataServiceFactory::GetInstance()->SetTestingFactory(
      profile(),
      base::BindLambdaForTesting(
          [&](content::BrowserContext*) -> std::unique_ptr<KeyedService> {
            return std::move(fake_service);
          }));

  AccountInfo primary_account_info = SetUpPrimaryAccountIfNeeded();
  AccountInfo account_info =
      identity_test_env()->MakeAccountAvailable("alice@example.com");
  MakeValidAccountInfo(&account_info);
  identity_test_env()->UpdateAccountInfoForAccount(account_info);

  signin::AccountPreviewDataService::AccountPreviewPreference pref;
  pref.preferred_data_types.push_back(
      {syncer::BOOKMARKS, signin::SyncDataQuartile::kAboveQ3});
  pref.other_device_form_factor =
      sync_pb::SyncEnums_DeviceFormFactor_DEVICE_FORM_FACTOR_PHONE;

  WebSigninInterceptor::Delegate::BubbleParameters expected_parameters(
      GetInterceptionType(),
      /*intercepted_account=*/account_info,
      /*primary_account=*/primary_account_info,
      /*profile_highlight_color=*/SkColor(),
      /*show_link_data_option=*/false,
      /*show_managed_disclaimer=*/false,
      /*account_preview_preference=*/pref);

  EXPECT_CALL(*mock_delegate(),
              ShowSigninInterceptionBubble(
                  web_contents(), MatchBubbleParameters(expected_parameters),
                  testing::_));

  interceptor()->MaybeInterceptWebSignin(
      web_contents(), account_info.GetAccountId(),
      signin_metrics::AccessPoint::kWebSignin,
      /*is_new_account=*/true, /*is_sync_signin=*/false,
      /*primary_is_connected=*/signin::Tribool::kUnknown);

  EXPECT_TRUE(raw_fake_service->has_pending_callback());
  raw_fake_service->TriggerCallback(pref);

  testing::Mock::VerifyAndClearExpectations(mock_delegate());
  histogram_tester.ExpectUniqueSample(
      "Signin.Intercept.AccountPreview.TimedOut", false, 1);
  histogram_tester.ExpectTotalCount(
      "Signin.Intercept.AccountPreview.ResponseTime", 1);
}

TEST_P(DiceWebSigninInterceptorTestWithAccountPreview,
       InterceptBubbleWithAccountPreviewDataSupervisedAccount) {
  base::HistogramTester histogram_tester;
  auto fake_service = std::make_unique<signin::TestAccountPreviewDataService>();
  fake_service->set_defer_callbacks(true);
  signin::TestAccountPreviewDataService* raw_fake_service = fake_service.get();
  AccountPreviewDataServiceFactory::GetInstance()->SetTestingFactory(
      profile(),
      base::BindLambdaForTesting(
          [&](content::BrowserContext*) -> std::unique_ptr<KeyedService> {
            return std::move(fake_service);
          }));

  AccountInfo primary_account_info = SetUpPrimaryAccountIfNeeded();
  AccountInfo account_info =
      identity_test_env()->MakeAccountAvailable("alice@example.com");
  MakeValidAccountInfoWithoutCapabilities(&account_info);
  AccountCapabilitiesTestMutator mutator(&account_info);
  mutator.set_is_subject_to_parental_controls(true);
  mutator.set_is_subject_to_account_level_enterprise_policies(false);
  identity_test_env()->UpdateAccountInfoForAccount(account_info);

  WebSigninInterceptor::Delegate::BubbleParameters expected_parameters(
      GetInterceptionType(),
      /*intercepted_account=*/account_info,
      /*primary_account=*/primary_account_info,
      /*profile_highlight_color=*/SkColor(),
      /*show_link_data_option=*/false,
      /*show_managed_disclaimer=*/false,
      /*account_preview_preference=*/std::nullopt);

  EXPECT_CALL(*mock_delegate(),
              ShowSigninInterceptionBubble(
                  web_contents(), MatchBubbleParameters(expected_parameters),
                  testing::_));

  interceptor()->MaybeInterceptWebSignin(
      web_contents(), account_info.GetAccountId(),
      signin_metrics::AccessPoint::kWebSignin,
      /*is_new_account=*/true, /*is_sync_signin=*/false,
      /*primary_is_connected=*/signin::Tribool::kUnknown);

  // For supervised accounts, account preview data is not fetched, so no
  // callback should be pending, and the bubble is shown immediately.
  EXPECT_FALSE(raw_fake_service->has_pending_callback());

  testing::Mock::VerifyAndClearExpectations(mock_delegate());
  histogram_tester.ExpectTotalCount("Signin.Intercept.AccountPreview.TimedOut",
                                    0);
  histogram_tester.ExpectTotalCount(
      "Signin.Intercept.AccountPreview.ResponseTime", 0);
}

TEST_P(DiceWebSigninInterceptorTestWithAccountPreview,
       InterceptBubbleWithAccountPreviewDataTimeout) {
  base::HistogramTester histogram_tester;
  auto fake_service = std::make_unique<signin::TestAccountPreviewDataService>();
  fake_service->set_defer_callbacks(true);
  signin::TestAccountPreviewDataService* raw_fake_service = fake_service.get();
  AccountPreviewDataServiceFactory::GetInstance()->SetTestingFactory(
      profile(),
      base::BindLambdaForTesting(
          [&](content::BrowserContext*) -> std::unique_ptr<KeyedService> {
            return std::move(fake_service);
          }));

  AccountInfo primary_account_info = SetUpPrimaryAccountIfNeeded();
  AccountInfo account_info =
      identity_test_env()->MakeAccountAvailable("alice@example.com");
  MakeValidAccountInfo(&account_info);
  identity_test_env()->UpdateAccountInfoForAccount(account_info);

  WebSigninInterceptor::Delegate::BubbleParameters expected_parameters(
      GetInterceptionType(),
      /*intercepted_account=*/account_info,
      /*primary_account=*/primary_account_info,
      /*profile_highlight_color=*/SkColor(),
      /*show_link_data_option=*/false,
      /*show_managed_disclaimer=*/false,
      /*account_preview_preference=*/std::nullopt);

  EXPECT_CALL(*mock_delegate(),
              ShowSigninInterceptionBubble(
                  web_contents(), MatchBubbleParameters(expected_parameters),
                  testing::_));

  interceptor()->MaybeInterceptWebSignin(
      web_contents(), account_info.GetAccountId(),
      signin_metrics::AccessPoint::kWebSignin,
      /*is_new_account=*/true, /*is_sync_signin=*/false,
      /*primary_is_connected=*/signin::Tribool::kUnknown);

  EXPECT_TRUE(raw_fake_service->has_pending_callback());
  task_environment()->FastForwardBy(
      switches::kAccountPreviewPreferredAccountSingleAccountPromoFetchTimeout
          .Get());

  testing::Mock::VerifyAndClearExpectations(mock_delegate());
  histogram_tester.ExpectUniqueSample(
      "Signin.Intercept.AccountPreview.TimedOut", true, 1);
  histogram_tester.ExpectTotalCount(
      "Signin.Intercept.AccountPreview.ResponseTime", 1);
}

TEST_P(DiceWebSigninInterceptorTestWithAccountPreview,
       InterceptBubbleWithAccountPreviewDataDelayedCallbackAfterReset) {
  auto fake_service = std::make_unique<signin::TestAccountPreviewDataService>();
  fake_service->set_defer_callbacks(true);
  signin::TestAccountPreviewDataService* raw_fake_service = fake_service.get();
  AccountPreviewDataServiceFactory::GetInstance()->SetTestingFactory(
      profile(),
      base::BindLambdaForTesting(
          [&](content::BrowserContext*) -> std::unique_ptr<KeyedService> {
            return std::move(fake_service);
          }));

  AccountInfo primary_account_info = SetUpPrimaryAccountIfNeeded();
  AccountInfo account_info_a =
      identity_test_env()->MakeAccountAvailable("alice@example.com");
  MakeValidAccountInfo(&account_info_a);
  account_info_a =
      AccountInfo::Builder(account_info_a).SetGivenName("Alice").Build();
  identity_test_env()->UpdateAccountInfoForAccount(account_info_a);

  AccountInfo account_info_b =
      identity_test_env()->MakeAccountAvailable("bob@example.com");
  MakeValidAccountInfo(&account_info_b);
  account_info_b =
      AccountInfo::Builder(account_info_b).SetGivenName("Bob").Build();
  identity_test_env()->UpdateAccountInfoForAccount(account_info_b);

  signin::AccountPreviewDataService::AccountPreviewPreference pref_a;
  pref_a.preferred_data_types.push_back(
      {syncer::BOOKMARKS, signin::SyncDataQuartile::kAboveQ3});
  pref_a.other_device_form_factor =
      sync_pb::SyncEnums_DeviceFormFactor_DEVICE_FORM_FACTOR_PHONE;

  signin::AccountPreviewDataService::AccountPreviewPreference pref_b;
  pref_b.preferred_data_types.push_back(
      {syncer::PASSWORDS, signin::SyncDataQuartile::kAboveQ3});
  pref_b.other_device_form_factor =
      sync_pb::SyncEnums_DeviceFormFactor_DEVICE_FORM_FACTOR_DESKTOP;

  WebSigninInterceptor::Delegate::BubbleParameters expected_parameters_b(
      GetInterceptionType(),
      /*intercepted_account=*/account_info_b,
      /*primary_account=*/primary_account_info,
      /*profile_highlight_color=*/SkColor(),
      /*show_link_data_option=*/false,
      /*show_managed_disclaimer=*/false,
      /*account_preview_preference=*/pref_b);

  // Interception 1 starts for Account A.
  interceptor()->MaybeInterceptWebSignin(
      web_contents(), account_info_a.GetAccountId(),
      signin_metrics::AccessPoint::kWebSignin,
      /*is_new_account=*/true, /*is_sync_signin=*/false,
      /*primary_is_connected=*/signin::Tribool::kUnknown);

  EXPECT_TRUE(raw_fake_service->has_pending_callback());
  auto delayed_callback_a = raw_fake_service->TakePendingCallback();

  // Interception 1 is reset while the fetch is in progress.
  interceptor()->Reset();

  // Interception 2 starts for Account B.
  interceptor()->MaybeInterceptWebSignin(
      web_contents(), account_info_b.GetAccountId(),
      signin_metrics::AccessPoint::kWebSignin,
      /*is_new_account=*/true, /*is_sync_signin=*/false,
      /*primary_is_connected=*/signin::Tribool::kUnknown);

  EXPECT_TRUE(raw_fake_service->has_pending_callback());

  // Bubble should only be shown for Account B, not Account A.
  EXPECT_CALL(*mock_delegate(),
              ShowSigninInterceptionBubble(
                  web_contents(), MatchBubbleParameters(expected_parameters_b),
                  testing::_));

  // The delayed callback for Account A finally fires. Since the weak pointer
  // was invalidated during Reset(), this should NOT show the bubble for Account
  // A.
  std::move(delayed_callback_a).Run(pref_a);

  // The callback for Account B fires and displays the bubble for Account B.
  raw_fake_service->TriggerCallback(pref_b);

  testing::Mock::VerifyAndClearExpectations(mock_delegate());
}

INSTANTIATE_TEST_SUITE_P(
    ,
    DiceWebSigninInterceptorTestWithAccountPreview,
    testing::Values(WebSigninInterceptor::SigninInterceptionType::kChromeSignin,
                    WebSigninInterceptor::SigninInterceptionType::kMultiUser),
    [](const testing::TestParamInfo<
        WebSigninInterceptor::SigninInterceptionType>& info) {
      switch (info.param) {
        case WebSigninInterceptor::SigninInterceptionType::kChromeSignin:
          return "ChromeSignin";
        case WebSigninInterceptor::SigninInterceptionType::kMultiUser:
          return "MultiUser";
        default:
          NOTREACHED();
      }
    });

TEST_F(DiceWebSigninInterceptorTest,
       InterceptShouldNotShowWaitForAccountInfoAvailableMetricRecorded) {
  base::HistogramTester histogram_tester;
  AccountInfo account_info =
      identity_test_env()->MakeAccountAvailable("alice@example.com");
  EXPECT_FALSE(interceptor()
                   ->GetHeuristicOutcome(/*is_new_account=*/true,
                                         /*is_sync_signin=*/false,
                                         account_info.GetEmail())
                   .has_value());
  EXPECT_CALL(*mock_delegate(), ShowSigninInterceptionBubble(
                                    web_contents(), testing::_, testing::_))
      .Times(0);
  interceptor()->MaybeInterceptWebSignin(
      web_contents(), account_info.GetAccountId(),
      signin_metrics::AccessPoint::kSettings,
      /*is_new_account=*/true,
      /*is_sync_signin=*/false,
      /*primary_is_connected=*/signin::Tribool::kUnknown);
  // Delegate was not called yet.
  testing::Mock::VerifyAndClearExpectations(mock_delegate());

  MakeValidAccountInfo(&account_info);
  identity_test_env()->UpdateAccountInfoForAccount(account_info);
  auto expected_outcome =
      SigninInterceptionHeuristicOutcome::kAbortAccountInfoNotCompatible;
  EXPECT_EQ(interceptor()->is_interception_in_progress(),
            SigninInterceptionHeuristicOutcomeIsSuccess(expected_outcome));
  histogram_tester.ExpectUniqueSample("Signin.Intercept.HeuristicOutcome",
                                      expected_outcome, 1);
}

TEST_F(DiceWebSigninInterceptorTest, NoInterceptionIfPrimaryAccountAlreadySet) {
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
      web_contents(), second_account_info.GetAccountId(),
      signin_metrics::AccessPoint::kWebSignin,
      /*is_new_account=*/true, /*is_sync_signin=*/false,
      /*primary_is_connected=*/signin::Tribool::kUnknown);
  EXPECT_EQ(interceptor()->GetHeuristicOutcome(
                /*is_new_account=*/true,
                /*is_sync_signin=*/false, second_account_info.GetEmail()),
            std::nullopt);
  testing::Mock::VerifyAndClearExpectations(mock_delegate());
  histogram_tester.ExpectUniqueSample("Signin.Intercept.HeuristicOutcome",
                                      expected_outcome, 1);

  EXPECT_EQ(interceptor()->is_interception_in_progress(),
            SigninInterceptionHeuristicOutcomeIsSuccess(expected_outcome));
}

TEST_F(DiceWebSigninInterceptorTest,
       MultiAccountInterception_UserAcceptsBeforeCompletion) {
  base::HistogramTester histogram_tester;
  AccountInfo primary_account_info =
      identity_test_env()->MakePrimaryAccountAvailable(
          "primary@example.com", signin::ConsentLevel::kSignin);
  AccountInfo initiator_info =
      identity_test_env()->MakeAccountAvailable("alice@example.com");
  MakeValidAccountInfo(&initiator_info);
  initiator_info =
      AccountInfo::Builder(initiator_info).SetGivenName("Alice").Build();
  identity_test_env()->UpdateAccountInfoForAccount(initiator_info);

  AccountInfo secondary_info =
      identity_test_env()->MakeAccountAvailable("bob@example.com");
  MakeValidAccountInfo(&secondary_info);
  secondary_info =
      AccountInfo::Builder(secondary_info).SetGivenName("Bob").Build();
  identity_test_env()->UpdateAccountInfoForAccount(secondary_info);

  // Setup mock delegate to capture the bubble callback.
  base::OnceCallback<void(SigninInterceptionResult)> bubble_callback;
  EXPECT_CALL(*mock_delegate(), ShowSigninInterceptionBubble(
                                    web_contents(), testing::_, testing::_))
      .WillOnce(testing::WithArg<2>(
          [&bubble_callback](
              base::OnceCallback<void(SigninInterceptionResult)> callback) {
            bubble_callback = std::move(callback);
            return std::make_unique<
                TestScopedWebSigninInterceptionBubbleHandle>();
          }));

  // Start interception.
  interceptor()->MaybeInterceptWebSignin(
      web_contents(), initiator_info.GetAccountId(),
      signin_metrics::AccessPoint::kWebSignin, /*is_new_account=*/true,
      /*is_sync_signin=*/false,
      /*primary_is_connected=*/signin::Tribool::kUnknown);

  ASSERT_TRUE(bubble_callback);
  ASSERT_TRUE(interceptor()->is_interception_in_progress());

  // Simulate user accepting the bubble.
  // Since the background multi-account token fetches (DICE session) are not yet
  // complete, this should not trigger the profile creator yet.
  std::move(bubble_callback).Run(SigninInterceptionResult::kAccepted);
  EXPECT_FALSE(interceptor()->has_dice_signed_in_profile_creator_for_testing());
  EXPECT_TRUE(interceptor()->is_interception_in_progress());

  // Now simulate session completion.
  // This should trigger ProceedWithProfileCreation and create the creator.
  std::vector<CoreAccountId> secondary_ids = {secondary_info.GetAccountId()};
  interceptor()->OnDiceSigninSessionComplete(initiator_info.GetAccountId(),
                                             secondary_ids);
  EXPECT_TRUE(interceptor()->has_dice_signed_in_profile_creator_for_testing());
  EXPECT_THAT(
      interceptor()->dice_signed_in_profile_creator_accounts_for_testing(),
      testing::ElementsAre(initiator_info.GetAccountId(),
                           secondary_info.GetAccountId()));
  EXPECT_TRUE(interceptor()->is_interception_in_progress());
}

TEST_F(DiceWebSigninInterceptorTest,
       MultiAccountInterception_UserAcceptsAfterCompletion) {
  base::HistogramTester histogram_tester;
  AccountInfo primary_account_info =
      identity_test_env()->MakePrimaryAccountAvailable(
          "primary@example.com", signin::ConsentLevel::kSignin);
  AccountInfo initiator_info =
      identity_test_env()->MakeAccountAvailable("alice@example.com");
  MakeValidAccountInfo(&initiator_info);
  initiator_info =
      AccountInfo::Builder(initiator_info).SetGivenName("Alice").Build();
  identity_test_env()->UpdateAccountInfoForAccount(initiator_info);

  AccountInfo secondary_info =
      identity_test_env()->MakeAccountAvailable("bob@example.com");
  MakeValidAccountInfo(&secondary_info);
  secondary_info =
      AccountInfo::Builder(secondary_info).SetGivenName("Bob").Build();
  identity_test_env()->UpdateAccountInfoForAccount(secondary_info);

  base::OnceCallback<void(SigninInterceptionResult)> bubble_callback;
  EXPECT_CALL(*mock_delegate(), ShowSigninInterceptionBubble(
                                    web_contents(), testing::_, testing::_))
      .WillOnce(testing::WithArg<2>(
          [&bubble_callback](
              base::OnceCallback<void(SigninInterceptionResult)> callback) {
            bubble_callback = std::move(callback);
            return std::make_unique<
                TestScopedWebSigninInterceptionBubbleHandle>();
          }));

  interceptor()->MaybeInterceptWebSignin(
      web_contents(), initiator_info.GetAccountId(),
      signin_metrics::AccessPoint::kWebSignin, /*is_new_account=*/true,
      /*is_sync_signin=*/false,
      /*primary_is_connected=*/signin::Tribool::kUnknown);

  ASSERT_TRUE(bubble_callback);
  ASSERT_TRUE(interceptor()->is_interception_in_progress());

  // Simulate session completion first.
  // This should NOT trigger profile creator because user hasn't accepted yet.
  std::vector<CoreAccountId> secondary_ids = {secondary_info.GetAccountId()};
  interceptor()->OnDiceSigninSessionComplete(initiator_info.GetAccountId(),
                                             secondary_ids);
  EXPECT_FALSE(interceptor()->has_dice_signed_in_profile_creator_for_testing());
  EXPECT_TRUE(interceptor()->is_interception_in_progress());

  // Now simulate user accepting.
  // This should immediately trigger ProceedWithProfileCreation.
  std::move(bubble_callback).Run(SigninInterceptionResult::kAccepted);
  EXPECT_TRUE(interceptor()->has_dice_signed_in_profile_creator_for_testing());

  EXPECT_THAT(
      interceptor()->dice_signed_in_profile_creator_accounts_for_testing(),
      testing::ElementsAre(initiator_info.GetAccountId(),
                           secondary_info.GetAccountId()));
  EXPECT_TRUE(interceptor()->is_interception_in_progress());
}

TEST_F(DiceWebSigninInterceptorTest,
       MultiAccountInterception_SecondaryFailure) {
  base::HistogramTester histogram_tester;
  AccountInfo primary_account_info =
      identity_test_env()->MakePrimaryAccountAvailable(
          "primary@example.com", signin::ConsentLevel::kSignin);
  AccountInfo initiator_info =
      identity_test_env()->MakeAccountAvailable("alice@example.com");
  MakeValidAccountInfo(&initiator_info);
  initiator_info =
      AccountInfo::Builder(initiator_info).SetGivenName("Alice").Build();
  identity_test_env()->UpdateAccountInfoForAccount(initiator_info);

  // Secondary 1: Bob has no refresh token in the source profile (simulates a
  // failed background fetch of a new account).
  // We only need a valid CoreAccountId for him, no extended setup needed.
  CoreAccountId secondary_bob_id = CoreAccountId::FromString("bob_id");

  // Secondary 2: Charlie has a valid refresh token in the source profile
  // (simulates an account with a pre-existing token).
  AccountInfo secondary_charlie =
      identity_test_env()->MakeAccountAvailable("charlie@example.com");
  MakeValidAccountInfo(&secondary_charlie);
  secondary_charlie =
      AccountInfo::Builder(secondary_charlie).SetGivenName("Charlie").Build();
  identity_test_env()->UpdateAccountInfoForAccount(secondary_charlie);
  identity_test_env()->SetRefreshTokenForAccount(
      secondary_charlie.GetAccountId());

  // Setup mock delegate.
  base::OnceCallback<void(SigninInterceptionResult)> bubble_callback;
  EXPECT_CALL(*mock_delegate(), ShowSigninInterceptionBubble(
                                    web_contents(), testing::_, testing::_))
      .WillOnce(testing::WithArg<2>(
          [&bubble_callback](
              base::OnceCallback<void(SigninInterceptionResult)> callback) {
            bubble_callback = std::move(callback);
            return std::make_unique<
                TestScopedWebSigninInterceptionBubbleHandle>();
          }));

  // Start interception.
  interceptor()->MaybeInterceptWebSignin(
      web_contents(), initiator_info.GetAccountId(),
      signin_metrics::AccessPoint::kWebSignin, /*is_new_account=*/true,
      /*is_sync_signin=*/false,
      /*primary_is_connected=*/signin::Tribool::kUnknown);

  ASSERT_TRUE(bubble_callback);
  ASSERT_TRUE(interceptor()->is_interception_in_progress());

  // User accepts immediately.
  std::move(bubble_callback).Run(SigninInterceptionResult::kAccepted);
  EXPECT_FALSE(interceptor()->has_dice_signed_in_profile_creator_for_testing());

  // Simulate session completion.
  // Bob has no refresh token, while Charlie has a valid token.
  // Expected: Both are passed to the creator, which will filter out Bob.
  std::vector<CoreAccountId> secondary_ids = {secondary_bob_id,
                                              secondary_charlie.GetAccountId()};

  interceptor()->OnDiceSigninSessionComplete(initiator_info.GetAccountId(),
                                             secondary_ids);

  EXPECT_TRUE(interceptor()->has_dice_signed_in_profile_creator_for_testing());

  // Alice, Bob, and Charlie are passed to the creator. Only Alice and Charlie
  // are actually moved, while Bob is skipped inside the creator.
  // Note: The actual moving of tokens and verification of the target profile's
  // database is tested in `dice_signed_in_profile_creator_unittest.cc`. In this
  // file, we verify that the interceptor correctly gathers and delegates the
  // expected list of accounts to the creator.
  EXPECT_THAT(
      interceptor()->dice_signed_in_profile_creator_accounts_for_testing(),
      testing::ElementsAre(initiator_info.GetAccountId(), secondary_bob_id,
                           secondary_charlie.GetAccountId()));
  EXPECT_TRUE(interceptor()->is_interception_in_progress());
}

TEST_F(DiceWebSigninInterceptorTest,
       MultiAccountInterception_LateSignalFromDifferentAccountIsIgnored) {
  base::HistogramTester histogram_tester;
  AccountInfo primary_account_info =
      identity_test_env()->MakePrimaryAccountAvailable(
          "primary@example.com", signin::ConsentLevel::kSignin);

  // Account 1: Alice (Initiator 1)
  AccountInfo initiator_alice =
      identity_test_env()->MakeAccountAvailable("alice@example.com");
  MakeValidAccountInfo(&initiator_alice);
  initiator_alice =
      AccountInfo::Builder(initiator_alice).SetGivenName("Alice").Build();
  identity_test_env()->UpdateAccountInfoForAccount(initiator_alice);

  // Account 2: Bob (Initiator 2)
  AccountInfo initiator_bob =
      identity_test_env()->MakeAccountAvailable("bob@example.com");
  MakeValidAccountInfo(&initiator_bob);
  initiator_bob =
      AccountInfo::Builder(initiator_bob).SetGivenName("Bob").Build();
  identity_test_env()->UpdateAccountInfoForAccount(initiator_bob);

  // 1. Start Alice's interception.
  base::OnceCallback<void(SigninInterceptionResult)> alice_bubble_callback;
  EXPECT_CALL(*mock_delegate(), ShowSigninInterceptionBubble(
                                    web_contents(), testing::_, testing::_))
      .WillOnce(testing::WithArg<2>(
          [&alice_bubble_callback](
              base::OnceCallback<void(SigninInterceptionResult)> callback) {
            alice_bubble_callback = std::move(callback);
            return std::make_unique<
                TestScopedWebSigninInterceptionBubbleHandle>();
          }));

  interceptor()->MaybeInterceptWebSignin(
      web_contents(), initiator_alice.GetAccountId(),
      signin_metrics::AccessPoint::kWebSignin, /*is_new_account=*/true,
      /*is_sync_signin=*/false,
      /*primary_is_connected=*/signin::Tribool::kUnknown);

  ASSERT_TRUE(alice_bubble_callback);
  ASSERT_TRUE(interceptor()->is_interception_in_progress());

  // 2. Alice's interception is declined (Reset is called).
  std::move(alice_bubble_callback).Run(SigninInterceptionResult::kDeclined);
  EXPECT_FALSE(interceptor()->is_interception_in_progress());
  EXPECT_FALSE(interceptor()->has_dice_signed_in_profile_creator_for_testing());

  // 3. Start Bob's interception.
  base::OnceCallback<void(SigninInterceptionResult)> bob_bubble_callback;
  EXPECT_CALL(*mock_delegate(), ShowSigninInterceptionBubble(
                                    web_contents(), testing::_, testing::_))
      .WillOnce(testing::WithArg<2>(
          [&bob_bubble_callback](
              base::OnceCallback<void(SigninInterceptionResult)> callback) {
            bob_bubble_callback = std::move(callback);
            return std::make_unique<
                TestScopedWebSigninInterceptionBubbleHandle>();
          }));

  interceptor()->MaybeInterceptWebSignin(
      web_contents(), initiator_bob.GetAccountId(),
      signin_metrics::AccessPoint::kWebSignin, /*is_new_account=*/true,
      /*is_sync_signin=*/false,
      /*primary_is_connected=*/signin::Tribool::kUnknown);

  ASSERT_TRUE(bob_bubble_callback);
  ASSERT_TRUE(interceptor()->is_interception_in_progress());

  // 4. Now, Alice's late session completion signal arrives!
  // This must be safely ignored because the active interception is for Bob.
  std::vector<CoreAccountId> alice_secondaries = {};
  interceptor()->OnDiceSigninSessionComplete(initiator_alice.GetAccountId(),
                                             alice_secondaries);

  // Verify that Alice's late signal did NOT trigger profile creation or change
  // the active state.
  EXPECT_FALSE(interceptor()->has_dice_signed_in_profile_creator_for_testing());
  EXPECT_TRUE(interceptor()->is_interception_in_progress());

  // 5. Bob's session completion signal arrives.
  std::vector<CoreAccountId> bob_secondaries = {};
  interceptor()->OnDiceSigninSessionComplete(initiator_bob.GetAccountId(),
                                             bob_secondaries);

  // User accepts Bob's bubble.
  std::move(bob_bubble_callback).Run(SigninInterceptionResult::kAccepted);

  // Verify profile creator was successfully started for Bob!
  EXPECT_TRUE(interceptor()->has_dice_signed_in_profile_creator_for_testing());
  EXPECT_THAT(
      interceptor()->dice_signed_in_profile_creator_accounts_for_testing(),
      testing::ElementsAre(initiator_bob.GetAccountId()));
}

TEST_F(DiceWebSigninInterceptorTest,
       MultiAccountInterception_DeferralLatency_UserWaits) {
  base::HistogramTester histogram_tester;
  AccountInfo primary_account_info =
      identity_test_env()->MakePrimaryAccountAvailable(
          "primary@example.com", signin::ConsentLevel::kSignin);
  AccountInfo initiator_info =
      identity_test_env()->MakeAccountAvailable("alice@example.com");
  MakeValidAccountInfo(&initiator_info);
  initiator_info =
      AccountInfo::Builder(initiator_info).SetGivenName("Alice").Build();
  identity_test_env()->UpdateAccountInfoForAccount(initiator_info);

  AccountInfo secondary_info =
      identity_test_env()->MakeAccountAvailable("bob@example.com");
  MakeValidAccountInfo(&secondary_info);
  secondary_info =
      AccountInfo::Builder(secondary_info).SetGivenName("Bob").Build();
  identity_test_env()->UpdateAccountInfoForAccount(secondary_info);

  base::OnceCallback<void(SigninInterceptionResult)> bubble_callback;
  EXPECT_CALL(*mock_delegate(), ShowSigninInterceptionBubble(
                                    web_contents(), testing::_, testing::_))
      .WillOnce(testing::WithArg<2>(
          [&bubble_callback](
              base::OnceCallback<void(SigninInterceptionResult)> callback) {
            bubble_callback = std::move(callback);
            return std::make_unique<
                TestScopedWebSigninInterceptionBubbleHandle>();
          }));

  interceptor()->MaybeInterceptWebSignin(
      web_contents(), initiator_info.GetAccountId(),
      signin_metrics::AccessPoint::kWebSignin, /*is_new_account=*/true,
      /*is_sync_signin=*/false,
      /*primary_is_connected=*/signin::Tribool::kFalse);

  ASSERT_TRUE(bubble_callback);
  ASSERT_TRUE(interceptor()->is_interception_in_progress());

  // User accepts bubble before session completion.
  std::move(bubble_callback).Run(SigninInterceptionResult::kAccepted);
  EXPECT_FALSE(interceptor()->has_dice_signed_in_profile_creator_for_testing());

  // Fast forward time by 200 ms while waiting.
  task_environment()->FastForwardBy(base::Milliseconds(200));

  // Session completion arrives.
  std::vector<CoreAccountId> secondary_ids = {secondary_info.GetAccountId()};
  interceptor()->OnDiceSigninSessionComplete(initiator_info.GetAccountId(),
                                             secondary_ids);

  EXPECT_TRUE(interceptor()->has_dice_signed_in_profile_creator_for_testing());
  histogram_tester.ExpectTimeBucketCount(
      "Signin.Dice.LinkedAccounts.Latency.InterceptionDeferral",
      base::Milliseconds(200), 1);
}

TEST_F(DiceWebSigninInterceptorTest,
       MultiAccountInterception_DeferralLatency_ZeroWait) {
  base::HistogramTester histogram_tester;
  AccountInfo primary_account_info =
      identity_test_env()->MakePrimaryAccountAvailable(
          "primary@example.com", signin::ConsentLevel::kSignin);
  AccountInfo initiator_info =
      identity_test_env()->MakeAccountAvailable("alice@example.com");
  MakeValidAccountInfo(&initiator_info);
  initiator_info =
      AccountInfo::Builder(initiator_info).SetGivenName("Alice").Build();
  identity_test_env()->UpdateAccountInfoForAccount(initiator_info);

  AccountInfo secondary_info =
      identity_test_env()->MakeAccountAvailable("bob@example.com");
  MakeValidAccountInfo(&secondary_info);
  secondary_info =
      AccountInfo::Builder(secondary_info).SetGivenName("Bob").Build();
  identity_test_env()->UpdateAccountInfoForAccount(secondary_info);

  base::OnceCallback<void(SigninInterceptionResult)> bubble_callback;
  EXPECT_CALL(*mock_delegate(), ShowSigninInterceptionBubble(
                                    web_contents(), testing::_, testing::_))
      .WillOnce(testing::WithArg<2>(
          [&bubble_callback](
              base::OnceCallback<void(SigninInterceptionResult)> callback) {
            bubble_callback = std::move(callback);
            return std::make_unique<
                TestScopedWebSigninInterceptionBubbleHandle>();
          }));

  interceptor()->MaybeInterceptWebSignin(
      web_contents(), initiator_info.GetAccountId(),
      signin_metrics::AccessPoint::kWebSignin, /*is_new_account=*/true,
      /*is_sync_signin=*/false,
      /*primary_is_connected=*/signin::Tribool::kFalse);

  ASSERT_TRUE(bubble_callback);
  ASSERT_TRUE(interceptor()->is_interception_in_progress());

  // Session completion arrives before user acceptance.
  std::vector<CoreAccountId> secondary_ids = {secondary_info.GetAccountId()};
  interceptor()->OnDiceSigninSessionComplete(initiator_info.GetAccountId(),
                                             secondary_ids);

  // User accepts bubble.
  std::move(bubble_callback).Run(SigninInterceptionResult::kAccepted);
  EXPECT_TRUE(interceptor()->has_dice_signed_in_profile_creator_for_testing());

  // 0 ms should be recorded for immediate/frictionless profile creation.
  histogram_tester.ExpectTimeBucketCount(
      "Signin.Dice.LinkedAccounts.Latency.InterceptionDeferral",
      base::Milliseconds(0), 1);
}

TEST_F(DiceWebSigninInterceptorTest,
       MultiAccountInterception_DeferralLatency_SingleAccountNoOp) {
  base::HistogramTester histogram_tester;
  AccountInfo primary_account_info =
      identity_test_env()->MakePrimaryAccountAvailable(
          "primary@example.com", signin::ConsentLevel::kSignin);
  AccountInfo initiator_info =
      identity_test_env()->MakeAccountAvailable("alice@example.com");
  MakeValidAccountInfo(&initiator_info);
  initiator_info =
      AccountInfo::Builder(initiator_info).SetGivenName("Alice").Build();
  identity_test_env()->UpdateAccountInfoForAccount(initiator_info);

  base::OnceCallback<void(SigninInterceptionResult)> bubble_callback;
  EXPECT_CALL(*mock_delegate(), ShowSigninInterceptionBubble(
                                    web_contents(), testing::_, testing::_))
      .WillOnce(testing::WithArg<2>(
          [&bubble_callback](
              base::OnceCallback<void(SigninInterceptionResult)> callback) {
            bubble_callback = std::move(callback);
            return std::make_unique<
                TestScopedWebSigninInterceptionBubbleHandle>();
          }));

  interceptor()->MaybeInterceptWebSignin(
      web_contents(), initiator_info.GetAccountId(),
      signin_metrics::AccessPoint::kWebSignin, /*is_new_account=*/true,
      /*is_sync_signin=*/false,
      /*primary_is_connected=*/signin::Tribool::kUnknown);

  ASSERT_TRUE(bubble_callback);

  // Single-account completion arrives with empty secondaries.
  interceptor()->OnDiceSigninSessionComplete(initiator_info.GetAccountId(),
                                             /*secondary_accounts=*/{});

  // User accepts bubble.
  std::move(bubble_callback).Run(SigninInterceptionResult::kAccepted);
  EXPECT_TRUE(interceptor()->has_dice_signed_in_profile_creator_for_testing());

  // No InterceptionDeferral metric should be recorded for single account
  // sign-in.
  histogram_tester.ExpectTotalCount(
      "Signin.Dice.LinkedAccounts.Latency.InterceptionDeferral", 0);
}
