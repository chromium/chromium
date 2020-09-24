// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/dice_web_signin_interceptor.h"

#include <memory>

#include "base/callback.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile_attributes_entry.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/signin/chrome_signin_client_factory.h"
#include "chrome/browser/signin/chrome_signin_client_test_util.h"
#include "chrome/browser/signin/identity_test_environment_profile_adaptor.h"
#include "chrome/browser/signin/signin_features.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_web_contents_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class MockDiceWebSigninInterceptorDelegate
    : public DiceWebSigninInterceptor::Delegate {
 public:
  MOCK_METHOD(void,
              ShowSigninInterceptionBubble,
              (content::WebContents * web_contents,
               const BubbleParameters& bubble_parameters,
               base::OnceCallback<void(bool)> callback),
              (override));
  void ShowProfileCustomizationBubble(Browser* browser) override {}
};

// Matches BubbleParameters fields excepting the color. This is useful in the
// test because the color is randomly generated.
testing::Matcher<const DiceWebSigninInterceptor::Delegate::BubbleParameters&>
MatchBubbleParameters(
    const DiceWebSigninInterceptor::Delegate::BubbleParameters& parameters) {
  return testing::AllOf(
      testing::Field("interception_type",
                     &DiceWebSigninInterceptor::Delegate::BubbleParameters::
                         interception_type,
                     parameters.interception_type),
      testing::Field("intercepted_account",
                     &DiceWebSigninInterceptor::Delegate::BubbleParameters::
                         intercepted_account,
                     parameters.intercepted_account),
      testing::Field("primary_account",
                     &DiceWebSigninInterceptor::Delegate::BubbleParameters::
                         primary_account,
                     parameters.primary_account));
}

// If the account info is valid, does nothing. Otherwise fills the extended
// fields with default values.
void MakeValidAccountInfo(AccountInfo* info) {
  if (info->IsValid())
    return;
  info->full_name = "fullname";
  info->given_name = "givenname";
  info->hosted_domain = kNoHostedDomainFound;
  info->locale = "en";
  info->picture_url = "https://example.com";
  info->is_child_account = false;
  DCHECK(info->IsValid());
}

}  // namespace

class DiceWebSigninInterceptorTest : public testing::Test {
 public:
  DiceWebSigninInterceptorTest() = default;
  ~DiceWebSigninInterceptorTest() override = default;

  DiceWebSigninInterceptor* interceptor() {
    return dice_web_signin_interceptor_.get();
  }

  MockDiceWebSigninInterceptorDelegate* mock_delegate() {
    return mock_delegate_;
  }

  Profile* profile() { return profile_; }

  content::WebContents* web_contents() { return web_contents_; }

  ProfileAttributesStorage* profile_attributes_storage() {
    return profile_manager_->profile_attributes_storage();
  }

  signin::IdentityTestEnvironment* identity_test_env() {
    return identity_test_env_profile_adaptor_->identity_test_env();
  }

  Profile* CreateTestingProfile(const std::string& name) {
    return profile_manager_->CreateTestingProfile(name);
  }

  // Helper function that calls MaybeInterceptWebSignin with parameters
  // compatible with interception.
  void MaybeIntercept(CoreAccountId account_id) {
    interceptor()->MaybeInterceptWebSignin(web_contents(), account_id,
                                           /*is_new_account=*/true,
                                           /*is_sync_signin=*/false);
  }

 private:
  // testing::Test:
  void SetUp() override {
    feature_list_.InitAndEnableFeature(kDiceWebSigninInterceptionFeature);
    // Create a testing profile registered in the profile manager.
    profile_manager_ = std::make_unique<TestingProfileManager>(
        TestingBrowserProcess::GetGlobal());
    ASSERT_TRUE(profile_manager_->SetUp());
    TestingProfile::TestingFactories factories =
        IdentityTestEnvironmentProfileAdaptor::
            GetIdentityTestEnvironmentFactories();
    factories.push_back(
        {ChromeSigninClientFactory::GetInstance(),
         base::BindRepeating(&BuildChromeSigninClientWithURLLoader,
                             &test_url_loader_factory_)});
    profile_ = profile_manager_->CreateTestingProfile(
        chrome::kInitialProfile,
        std::unique_ptr<sync_preferences::PrefServiceSyncable>(),
        base::UTF8ToUTF16(""), 0, std::string(), std::move(factories));
    identity_test_env_profile_adaptor_ =
        std::make_unique<IdentityTestEnvironmentProfileAdaptor>(profile_);
    identity_test_env_profile_adaptor_->identity_test_env()
        ->SetTestURLLoaderFactory(&test_url_loader_factory_);

    auto delegate = std::make_unique<
        testing::StrictMock<MockDiceWebSigninInterceptorDelegate>>();
    mock_delegate_ = delegate.get();
    dice_web_signin_interceptor_ = std::make_unique<DiceWebSigninInterceptor>(
        profile_, std::move(delegate));

    web_contents_ = test_web_contents_factory_.CreateWebContents(profile_);
  }

  void TearDown() override {
    test_web_contents_factory_.DestroyWebContents(web_contents_);
    dice_web_signin_interceptor_->Shutdown();
    identity_test_env_profile_adaptor_.reset();
    profile_manager_->DeleteTestingProfile(chrome::kInitialProfile);
  }

  base::test::ScopedFeatureList feature_list_;
  content::BrowserTaskEnvironment task_environment_;
  network::TestURLLoaderFactory test_url_loader_factory_;
  content::TestWebContentsFactory test_web_contents_factory_;
  std::unique_ptr<TestingProfileManager> profile_manager_;
  std::unique_ptr<IdentityTestEnvironmentProfileAdaptor>
      identity_test_env_profile_adaptor_;
  std::unique_ptr<DiceWebSigninInterceptor> dice_web_signin_interceptor_;

  // Owned by profile_manager_
  TestingProfile* profile_ = nullptr;
  // Owned by dice_web_signin_interceptor_
  MockDiceWebSigninInterceptorDelegate* mock_delegate_ = nullptr;
  // Owned by test_web_contents_factory_
  content::WebContents* web_contents_ = nullptr;
};

TEST_F(DiceWebSigninInterceptorTest, ShouldShowProfileSwitchBubble) {
  AccountInfo account_info =
      identity_test_env()->MakeAccountAvailable("bob@example.com");
  EXPECT_FALSE(interceptor()->ShouldShowProfileSwitchBubble(
      account_info, profile_attributes_storage()));

  // Add another profile with no account.
  CreateTestingProfile("Profile 1");
  EXPECT_FALSE(interceptor()->ShouldShowProfileSwitchBubble(
      account_info, profile_attributes_storage()));

  // Add another profile with a different account.
  Profile* profile_2 = CreateTestingProfile("Profile 2");
  ProfileAttributesEntry* entry;
  ASSERT_TRUE(profile_attributes_storage()->GetProfileAttributesWithPath(
      profile_2->GetPath(), &entry));
  std::string kOtherGaiaID = "SomeOtherGaiaID";
  ASSERT_NE(kOtherGaiaID, account_info.gaia);
  entry->SetAuthInfo(kOtherGaiaID, base::UTF8ToUTF16("Bob"),
                     /*is_consented_primary_account=*/true);
  EXPECT_FALSE(interceptor()->ShouldShowProfileSwitchBubble(
      account_info, profile_attributes_storage()));

  // Change the account to match.
  entry->SetAuthInfo(account_info.gaia, base::UTF8ToUTF16("Bob"),
                     /*is_consented_primary_account=*/false);
  const ProfileAttributesEntry* switch_to_entry =
      interceptor()->ShouldShowProfileSwitchBubble(
          account_info, profile_attributes_storage());
  EXPECT_EQ(entry, switch_to_entry);
}

TEST_F(DiceWebSigninInterceptorTest, NoBubbleWithSingleAccount) {
  AccountInfo account_info =
      identity_test_env()->MakeAccountAvailable("bob@example.com");
  MakeValidAccountInfo(&account_info);
  account_info.hosted_domain = "example.com";
  identity_test_env()->UpdateAccountInfoForAccount(account_info);

  // Without UPA.
  EXPECT_FALSE(interceptor()->ShouldShowEnterpriseBubble(account_info));
  EXPECT_FALSE(interceptor()->ShouldShowMultiUserBubble(account_info));

  // With UPA.
  identity_test_env()->SetUnconsentedPrimaryAccount("bob@example.com");
  EXPECT_FALSE(interceptor()->ShouldShowEnterpriseBubble(account_info));
}

TEST_F(DiceWebSigninInterceptorTest, ShouldShowEnterpriseBubble) {
  // Setup 3 accounts in the profile:
  // - primary account
  // - other enterprise account that is not primary (should be ignored)
  // - intercepted account.
  AccountInfo primary_account_info =
      identity_test_env()->MakeUnconsentedPrimaryAccountAvailable(
          "alice@example.com");
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
                signin::ConsentLevel::kNotRequired),
            primary_account_info.account_id);

  // The primary account does not have full account info (empty domain).
  ASSERT_TRUE(identity_test_env()
                  ->identity_manager()
                  ->FindExtendedAccountInfoForAccountWithRefreshToken(
                      primary_account_info)
                  ->hosted_domain.empty());
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
      signin::ConsentLevel::kNotRequired));
  EXPECT_FALSE(interceptor()->ShouldShowEnterpriseBubble(account_info_1));
}

TEST_F(DiceWebSigninInterceptorTest, ShouldShowMultiUserBubble) {
  // Setup two accounts in the profile.
  AccountInfo account_info_1 =
      identity_test_env()->MakeAccountAvailable("bob@example.com");
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

TEST_F(DiceWebSigninInterceptorTest, NoInterception) {
  // Setup for profile switch interception.
  AccountInfo account_info =
      identity_test_env()->MakeAccountAvailable("bob@example.com");
  Profile* profile_2 = CreateTestingProfile("Profile 2");
  ProfileAttributesEntry* entry;
  ASSERT_TRUE(profile_attributes_storage()->GetProfileAttributesWithPath(
      profile_2->GetPath(), &entry));
  entry->SetAuthInfo(account_info.gaia, base::UTF8ToUTF16("Bob"),
                     /*is_consented_primary_account=*/false);

  std::unique_ptr<base::HistogramTester> histogram_tester =
      std::make_unique<base::HistogramTester>();
  // Check that Sync signin is not intercepted.
  interceptor()->MaybeInterceptWebSignin(web_contents(),
                                         account_info.account_id,
                                         /*is_new_account=*/true,
                                         /*is_sync_signin=*/true);
  testing::Mock::VerifyAndClearExpectations(mock_delegate());
  histogram_tester->ExpectUniqueSample(
      "Signin.Intercept.HeuristicOutcome",
      SigninInterceptionHeuristicOutcome::kAbortSyncSignin, 1);

  // Check that reauth is not intercepted.
  histogram_tester = std::make_unique<base::HistogramTester>();
  interceptor()->MaybeInterceptWebSignin(web_contents(),
                                         account_info.account_id,
                                         /*is_new_account=*/false,
                                         /*is_sync_signin=*/false);
  testing::Mock::VerifyAndClearExpectations(mock_delegate());
  histogram_tester->ExpectUniqueSample(
      "Signin.Intercept.HeuristicOutcome",
      SigninInterceptionHeuristicOutcome::kAbortAccountNotNew, 1);

  // Check that interception works otherwise, as a sanity check.
  histogram_tester = std::make_unique<base::HistogramTester>();
  DiceWebSigninInterceptor::Delegate::BubbleParameters expected_parameters = {
      DiceWebSigninInterceptor::SigninInterceptionType::kProfileSwitch,
      account_info, AccountInfo(), SkColor()};
  EXPECT_CALL(*mock_delegate(),
              ShowSigninInterceptionBubble(
                  web_contents(), MatchBubbleParameters(expected_parameters),
                  testing::_));
  interceptor()->MaybeInterceptWebSignin(web_contents(),
                                         account_info.account_id,
                                         /*is_new_account=*/true,
                                         /*is_sync_signin=*/false);
  histogram_tester->ExpectUniqueSample(
      "Signin.Intercept.HeuristicOutcome",
      SigninInterceptionHeuristicOutcome::kInterceptProfileSwitch, 1);
}

TEST_F(DiceWebSigninInterceptorTest, InterceptionInProgress) {
  // Setup for profile switch interception.
  AccountInfo account_info =
      identity_test_env()->MakeAccountAvailable("bob@example.com");
  Profile* profile_2 = CreateTestingProfile("Profile 2");
  ProfileAttributesEntry* entry;
  ASSERT_TRUE(profile_attributes_storage()->GetProfileAttributesWithPath(
      profile_2->GetPath(), &entry));
  entry->SetAuthInfo(account_info.gaia, base::UTF8ToUTF16("Bob"),
                     /*is_consented_primary_account=*/false);

  // Start an interception.
  DiceWebSigninInterceptor::Delegate::BubbleParameters expected_parameters = {
      DiceWebSigninInterceptor::SigninInterceptionType::kProfileSwitch,
      account_info, AccountInfo(), SkColor()};
  base::OnceCallback<void(bool)> delegate_callback;
  EXPECT_CALL(*mock_delegate(),
              ShowSigninInterceptionBubble(
                  web_contents(), MatchBubbleParameters(expected_parameters),
                  testing::_))
      .WillOnce(testing::WithArg<2>(testing::Invoke(
          [&delegate_callback](base::OnceCallback<void(bool)> callback) {
            delegate_callback = std::move(callback);
          })));
  MaybeIntercept(account_info.account_id);
  testing::Mock::VerifyAndClearExpectations(mock_delegate());
  EXPECT_TRUE(interceptor()->is_interception_in_progress_);

  // Check that there is no interception while another one is in progress.
  base::HistogramTester histogram_tester;
  MaybeIntercept(account_info.account_id);
  testing::Mock::VerifyAndClearExpectations(mock_delegate());
  histogram_tester.ExpectUniqueSample(
      "Signin.Intercept.HeuristicOutcome",
      SigninInterceptionHeuristicOutcome::kAbortInterceptInProgress, 1);

  // Complete the interception that was in progress.
  std::move(delegate_callback).Run(false);
  EXPECT_FALSE(interceptor()->is_interception_in_progress_);

  // A new interception can now start.
  EXPECT_CALL(*mock_delegate(),
              ShowSigninInterceptionBubble(
                  web_contents(), MatchBubbleParameters(expected_parameters),
                  testing::_));
  MaybeIntercept(account_info.account_id);
}

// Interception other than the profile switch require at least 2 accounts.
TEST_F(DiceWebSigninInterceptorTest, NoInterceptionWithOneAccount) {
  base::HistogramTester histogram_tester;
  AccountInfo account_info =
      identity_test_env()->MakeAccountAvailable("bob@example.com");
  // Interception aborts even if the account info is not available.
  ASSERT_FALSE(
      identity_test_env()
          ->identity_manager()
          ->FindExtendedAccountInfoForAccountWithRefreshTokenByAccountId(
              account_info.account_id)
          ->IsValid());
  MaybeIntercept(account_info.account_id);
  EXPECT_FALSE(interceptor()->is_interception_in_progress_);
  histogram_tester.ExpectUniqueSample(
      "Signin.Intercept.HeuristicOutcome",
      SigninInterceptionHeuristicOutcome::kAbortSingleAccount, 1);
}

// When profile creation is disallowed, profile switch interception is still
// enabled, but others are disabled.
TEST_F(DiceWebSigninInterceptorTest, ProfileCreationDisallowed) {
  base::HistogramTester histogram_tester;
  g_browser_process->local_state()->SetBoolean(prefs::kBrowserAddPersonEnabled,
                                               false);
  // Setup for profile switch interception.
  AccountInfo account_info =
      identity_test_env()->MakeAccountAvailable("bob@example.com");
  AccountInfo other_account_info =
      identity_test_env()->MakeAccountAvailable("alice@example.com");
  Profile* profile_2 = CreateTestingProfile("Profile 2");
  ProfileAttributesEntry* entry;
  ASSERT_TRUE(profile_attributes_storage()->GetProfileAttributesWithPath(
      profile_2->GetPath(), &entry));
  entry->SetAuthInfo(account_info.gaia, base::UTF8ToUTF16("Bob"),
                     /*is_consented_primary_account=*/false);

  // Interception that would offer creating a new profile does not work.
  MaybeIntercept(other_account_info.account_id);
  EXPECT_FALSE(interceptor()->is_interception_in_progress_);
  histogram_tester.ExpectUniqueSample(
      "Signin.Intercept.HeuristicOutcome",
      SigninInterceptionHeuristicOutcome::kAbortProfileCreationDisallowed, 1);

  // Profile switch interception still works.
  DiceWebSigninInterceptor::Delegate::BubbleParameters expected_parameters = {
      DiceWebSigninInterceptor::SigninInterceptionType::kProfileSwitch,
      account_info, AccountInfo(), SkColor()};
  EXPECT_CALL(*mock_delegate(),
              ShowSigninInterceptionBubble(
                  web_contents(), MatchBubbleParameters(expected_parameters),
                  testing::_));
  MaybeIntercept(account_info.account_id);
}

TEST_F(DiceWebSigninInterceptorTest, WaitForAccountInfoAvailable) {
  base::HistogramTester histogram_tester;
  AccountInfo primary_account_info =
      identity_test_env()->MakeUnconsentedPrimaryAccountAvailable(
          "bob@example.com");
  AccountInfo account_info =
      identity_test_env()->MakeAccountAvailable("alice@example.com");
  MaybeIntercept(account_info.account_id);
  // Delegate was not called yet.
  testing::Mock::VerifyAndClearExpectations(mock_delegate());

  // Account info becomes available, interception happens.
  DiceWebSigninInterceptor::Delegate::BubbleParameters expected_parameters = {
      DiceWebSigninInterceptor::SigninInterceptionType::kEnterprise,
      account_info, primary_account_info, SkColor()};
  EXPECT_CALL(*mock_delegate(),
              ShowSigninInterceptionBubble(
                  web_contents(), MatchBubbleParameters(expected_parameters),
                  testing::_));
  MakeValidAccountInfo(&account_info);
  account_info.hosted_domain = "example.com";
  identity_test_env()->UpdateAccountInfoForAccount(account_info);
  histogram_tester.ExpectTotalCount("Signin.Intercept.AccountInfoFetchDuration",
                                    1);
}

TEST_F(DiceWebSigninInterceptorTest, AccountInfoAlreadyAvailable) {
  base::HistogramTester histogram_tester;
  AccountInfo primary_account_info =
      identity_test_env()->MakeUnconsentedPrimaryAccountAvailable(
          "bob@example.com");
  AccountInfo account_info =
      identity_test_env()->MakeAccountAvailable("alice@example.com");
  MakeValidAccountInfo(&account_info);
  account_info.hosted_domain = "example.com";
  identity_test_env()->UpdateAccountInfoForAccount(account_info);

  // Account info is already available, interception happens immediately.
  DiceWebSigninInterceptor::Delegate::BubbleParameters expected_parameters = {
      DiceWebSigninInterceptor::SigninInterceptionType::kEnterprise,
      account_info, primary_account_info, SkColor()};
  EXPECT_CALL(*mock_delegate(),
              ShowSigninInterceptionBubble(
                  web_contents(), MatchBubbleParameters(expected_parameters),
                  testing::_));
  MaybeIntercept(account_info.account_id);
  histogram_tester.ExpectTotalCount("Signin.Intercept.AccountInfoFetchDuration",
                                    1);
  histogram_tester.ExpectUniqueSample(
      "Signin.Intercept.HeuristicOutcome",
      SigninInterceptionHeuristicOutcome::kInterceptEnterprise, 1);
}

TEST_F(DiceWebSigninInterceptorTest, MultiUserInterception) {
  base::HistogramTester histogram_tester;
  AccountInfo primary_account_info =
      identity_test_env()->MakeUnconsentedPrimaryAccountAvailable(
          "bob@example.com");
  AccountInfo account_info =
      identity_test_env()->MakeAccountAvailable("alice@example.com");
  MakeValidAccountInfo(&account_info);
  identity_test_env()->UpdateAccountInfoForAccount(account_info);

  // Account info is already available, interception happens immediately.
  DiceWebSigninInterceptor::Delegate::BubbleParameters expected_parameters = {
      DiceWebSigninInterceptor::SigninInterceptionType::kMultiUser,
      account_info, primary_account_info, SkColor()};
  EXPECT_CALL(*mock_delegate(),
              ShowSigninInterceptionBubble(
                  web_contents(), MatchBubbleParameters(expected_parameters),
                  testing::_));
  MaybeIntercept(account_info.account_id);
  histogram_tester.ExpectUniqueSample(
      "Signin.Intercept.HeuristicOutcome",
      SigninInterceptionHeuristicOutcome::kInterceptMultiUser, 1);
}
