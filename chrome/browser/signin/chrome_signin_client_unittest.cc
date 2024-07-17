// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/chrome_signin_client.h"

#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/enterprise/util/managed_browser_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/chrome_signin_client_factory.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/signin/signin_util.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/base/signin_metrics.h"
#include "components/signin/public/base/signin_pref_names.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

#if !BUILDFLAG(IS_ANDROID)
#include "chrome/test/base/browser_with_test_window_test.h"
#endif

#if !BUILDFLAG(IS_CHROMEOS_ASH) && !BUILDFLAG(IS_ANDROID)

class MockChromeSigninClient : public ChromeSigninClient {
 public:
  explicit MockChromeSigninClient(Profile* profile)
      : ChromeSigninClient(profile) {}

  MOCK_METHOD1(ShowUserManager, void(const base::FilePath&));
  MOCK_METHOD1(LockForceSigninProfile, void(const base::FilePath&));

  MOCK_METHOD2(SignOutCallback,
               void(signin_metrics::ProfileSignout,
                    SigninClient::SignoutDecision signout_decision));

  MOCK_METHOD0(GetAllBookmarksCount, std::optional<size_t>());
  MOCK_METHOD0(GetBookmarkBarBookmarksCount, std::optional<size_t>());
  MOCK_METHOD0(GetExtensionsCount, std::optional<size_t>());
};

class ChromeSigninClientSignoutTest : public BrowserWithTestWindowTest {
 public:
  ChromeSigninClientSignoutTest() : forced_signin_setter_(true) {}
  void SetUp() override {
    BrowserWithTestWindowTest::SetUp();
    CreateClient(browser()->profile());
  }

  void TearDown() override {
    BrowserWithTestWindowTest::TearDown();
    TestingBrowserProcess::GetGlobal()->SetLocalState(nullptr);
  }

  void CreateClient(Profile* profile) {
    client_ = std::make_unique<MockChromeSigninClient>(profile);
  }

  void PreSignOut(signin_metrics::ProfileSignout source_metric) {
    client_->PreSignOut(
        base::BindOnce(&MockChromeSigninClient::SignOutCallback,
                       base::Unretained(client_.get()), source_metric),
        source_metric,
        /*has_sync_account=*/false);
  }

  signin_util::ScopedForceSigninSetterForTesting forced_signin_setter_;
  std::unique_ptr<MockChromeSigninClient> client_;
};

TEST_F(ChromeSigninClientSignoutTest, SignOut) {
  signin_metrics::ProfileSignout source_metric =
      signin_metrics::ProfileSignout::kUserClickedSignoutSettings;

  EXPECT_CALL(*client_, ShowUserManager(browser()->profile()->GetPath()))
      .Times(1);
  EXPECT_CALL(*client_, LockForceSigninProfile(browser()->profile()->GetPath()))
      .Times(1);
  EXPECT_CALL(*client_, SignOutCallback(source_metric,
                                        SigninClient::SignoutDecision::ALLOW))
      .Times(1);

  PreSignOut(source_metric);
}

TEST_F(ChromeSigninClientSignoutTest, SignOutWithoutForceSignin) {
  signin_util::ScopedForceSigninSetterForTesting signin_setter(false);
  CreateClient(browser()->profile());

  signin_metrics::ProfileSignout source_metric =
      signin_metrics::ProfileSignout::kUserClickedSignoutSettings;

  EXPECT_CALL(*client_, ShowUserManager(browser()->profile()->GetPath()))
      .Times(0);
  EXPECT_CALL(*client_, LockForceSigninProfile(browser()->profile()->GetPath()))
      .Times(0);
  EXPECT_CALL(*client_, SignOutCallback(source_metric,
                                        SigninClient::SignoutDecision::ALLOW))
      .Times(1);
  PreSignOut(source_metric);
}

#if BUILDFLAG(IS_CHROMEOS_LACROS)
TEST_F(ChromeSigninClientSignoutTest, MainProfile) {
  TestingProfile::Builder builder;
  builder.SetIsMainProfile(true);
  std::unique_ptr<TestingProfile> profile = builder.Build();

  CreateClient(profile.get());
  EXPECT_FALSE(
      client_->IsClearPrimaryAccountAllowed(/*has_sync_account=*/false));
  EXPECT_TRUE(client_->IsRevokeSyncConsentAllowed());
}
#endif

TEST_F(ChromeSigninClientSignoutTest, AllAllowed) {
  std::unique_ptr<TestingProfile> profile = TestingProfile::Builder().Build();
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  EXPECT_FALSE(profile->IsMainProfile());
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)
  EXPECT_FALSE(profile->IsChild());

  CreateClient(profile.get());

  EXPECT_TRUE(
      client_->IsClearPrimaryAccountAllowed(/*has_sync_account=*/false));
#if BUILDFLAG(IS_ANDROID)
  EXPECT_TRUE(client_->IsRevokeSyncConsentAllowed());
#endif
}

TEST_F(ChromeSigninClientSignoutTest, ChildProfile) {
  TestingProfile::Builder builder;
  builder.SetIsSupervisedProfile();
  std::unique_ptr<TestingProfile> profile = builder.Build();
  EXPECT_TRUE(profile->IsChild());

  CreateClient(profile.get());
#if BUILDFLAG(IS_ANDROID)
  EXPECT_FALSE(
      client_->IsClearPrimaryAccountAllowed(/*has_sync_account=*/false));
#else
  EXPECT_TRUE(
      client_->IsClearPrimaryAccountAllowed(/*has_sync_account=*/false));
#endif
  EXPECT_TRUE(client_->IsRevokeSyncConsentAllowed());
}

class ChromeSigninClientSignoutSourceTest
    : public ::testing::WithParamInterface<signin_metrics::ProfileSignout>,
      public ChromeSigninClientSignoutTest {
 protected:
  signin::IdentityTestEnvironment* identity_test_env() {
    return &identity_test_env_;
  }

 private:
  signin::IdentityTestEnvironment identity_test_env_;
};

// Returns true if signout can be disallowed by policy for the given source.
bool IsAlwaysAllowedSignoutSources(
    signin_metrics::ProfileSignout signout_source) {
  switch (signout_source) {
    // NOTE: SIGNOUT_TEST == SIGNOUT_PREF_CHANGED.
    case signin_metrics::ProfileSignout::kPrefChanged:
    case signin_metrics::ProfileSignout::kGoogleServiceNamePatternChanged:
    case signin_metrics::ProfileSignout::kUserClickedSignoutSettings:
    case signin_metrics::ProfileSignout::kServerForcedDisable:
    case signin_metrics::ProfileSignout::kAuthenticationFailedWithForceSignin:
    case signin_metrics::ProfileSignout::kSigninNotAllowedOnProfileInit:
    case signin_metrics::ProfileSignout::kSigninRetriggeredFromWebSignin:
    case signin_metrics::ProfileSignout::
        kUserClickedSignoutFromClearBrowsingDataPage:
    case signin_metrics::ProfileSignout::
        kIosAccountRemovedFromDeviceAfterRestore:
    case signin_metrics::ProfileSignout::kUserDeletedAccountCookies:
    case signin_metrics::ProfileSignout::kGaiaCookieUpdated:
    case signin_metrics::ProfileSignout::kAccountReconcilorReconcile:
    case signin_metrics::ProfileSignout::kUserClickedSignoutProfileMenu:
    case signin_metrics::ProfileSignout::kAccountEmailUpdated:
    case signin_metrics::ProfileSignout::kSigninManagerUpdateUPA:
    case signin_metrics::ProfileSignout::kUserTappedUndoRightAfterSignIn:
    case signin_metrics::ProfileSignout::
        kUserDeclinedHistorySyncAfterDedicatedSignIn:
    case signin_metrics::ProfileSignout::kDeviceLockRemovedOnAutomotive:
    case signin_metrics::ProfileSignout::kRevokeSyncFromSettings:
    case signin_metrics::ProfileSignout::kIdleTimeoutPolicyTriggeredSignOut:
    case signin_metrics::ProfileSignout::kChangeAccountInAccountMenu:
    case signin_metrics::ProfileSignout::kUserClickedSignoutInAccountMenu:
    case signin_metrics::ProfileSignout::kUserDisabledAllowChromeSignIn:
      return false;

    case signin_metrics::ProfileSignout::kAccountRemovedFromDevice:
    // Allow signout because data has not been synced yet.
    case signin_metrics::ProfileSignout::kAbortSignin:
    case signin_metrics::ProfileSignout::
        kCancelSyncConfirmationOnWebOnlySignedIn:
    case signin_metrics::ProfileSignout::kCancelSyncConfirmationRemoveAccount:
    case signin_metrics::ProfileSignout::kMovePrimaryAccount:
    // Allow signout for tests that want to force it.
    case signin_metrics::ProfileSignout::kForceSignoutAlwaysAllowedForTest:
    case signin_metrics::ProfileSignout::kUserClickedRevokeSyncConsentSettings:
    case signin_metrics::ProfileSignout::
        kUserClickedSignoutFromUserPolicyNotificationDialog:
    case signin_metrics::ProfileSignout::kSignoutDuringProfileDeletion:
      return true;
  }
}

#if BUILDFLAG(IS_CHROMEOS_LACROS)
TEST_P(ChromeSigninClientSignoutSourceTest, UserSignoutMainProfile) {
  signin_metrics::ProfileSignout signout_source = GetParam();

  TestingProfile::Builder builder;
  builder.SetIsMainProfile(true);
  std::unique_ptr<TestingProfile> profile = builder.Build();

  CreateClient(profile.get());
  ASSERT_FALSE(
      client_->IsClearPrimaryAccountAllowed(/*has_sync_account=*/false));

  SigninClient::SignoutDecision signout_decision =
      IsAlwaysAllowedSignoutSources(signout_source)
          ? SigninClient::SignoutDecision::ALLOW
          : SigninClient::SignoutDecision::CLEAR_PRIMARY_ACCOUNT_DISALLOWED;
  EXPECT_CALL(*client_, SignOutCallback(signout_source, signout_decision))
      .Times(1);
  PreSignOut(signout_source);
}
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

TEST_P(ChromeSigninClientSignoutSourceTest, UserSignoutAllowed) {
  signin_metrics::ProfileSignout signout_source = GetParam();

  TestingProfile::Builder builder;
  builder.SetGuestSession();
  std::unique_ptr<TestingProfile> profile = builder.Build();

  CreateClient(profile.get());
  ASSERT_TRUE(
      client_->IsClearPrimaryAccountAllowed(/*has_sync_account=*/false));
  ASSERT_TRUE(client_->IsRevokeSyncConsentAllowed());

  // Verify IdentityManager gets callback indicating sign-out is always allowed.
  EXPECT_CALL(*client_, SignOutCallback(signout_source,
                                        SigninClient::SignoutDecision::ALLOW))
      .Times(1);

  PreSignOut(signout_source);
}

// TODO(crbug.com/40240718): Enable |ChromeSigninClientSignoutSourceTest| test
// suite on Android.
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || \
    BUILDFLAG(IS_MAC)
TEST_P(ChromeSigninClientSignoutSourceTest, UserSignoutDisallowed) {
  signin_metrics::ProfileSignout signout_source = GetParam();

  TestingProfile::Builder builder;
  builder.SetGuestSession();
  std::unique_ptr<TestingProfile> profile = builder.Build();

  CreateClient(profile.get());

  client_->set_is_clear_primary_account_allowed_for_testing(
      SigninClient::SignoutDecision::CLEAR_PRIMARY_ACCOUNT_DISALLOWED);
  ASSERT_FALSE(
      client_->IsClearPrimaryAccountAllowed(/*has_sync_account=*/false));

  // Verify IdentityManager gets callback indicating sign-out is disallowed iff
  // the source of the sign-out is a user-action.
  SigninClient::SignoutDecision signout_decision =
      IsAlwaysAllowedSignoutSources(signout_source)
          ? SigninClient::SignoutDecision::ALLOW
          : SigninClient::SignoutDecision::CLEAR_PRIMARY_ACCOUNT_DISALLOWED;
  EXPECT_CALL(*client_, SignOutCallback(signout_source, signout_decision))
      .Times(1);

  PreSignOut(signout_source);
}

TEST_P(ChromeSigninClientSignoutSourceTest, RevokeSyncDisallowed) {
  signin_metrics::ProfileSignout signout_source = GetParam();
  TestingProfile::Builder builder;
  builder.SetGuestSession();
  std::unique_ptr<TestingProfile> profile = builder.Build();

  CreateClient(profile.get());

  client_->set_is_clear_primary_account_allowed_for_testing(
      SigninClient::SignoutDecision::REVOKE_SYNC_DISALLOWED);
  ASSERT_FALSE(
      client_->IsClearPrimaryAccountAllowed(/*has_sync_account=*/false));
  ASSERT_FALSE(client_->IsRevokeSyncConsentAllowed());

  // Verify IdentityManager gets callback indicating sign-out is disallowed iff
  // the source of the sign-out is a user-action.
  SigninClient::SignoutDecision signout_decision =
      IsAlwaysAllowedSignoutSources(signout_source)
          ? SigninClient::SignoutDecision::ALLOW
          : SigninClient::SignoutDecision::REVOKE_SYNC_DISALLOWED;
  EXPECT_CALL(*client_, SignOutCallback(signout_source, signout_decision))
      .Times(1);

  PreSignOut(signout_source);
}
#endif

const signin_metrics::ProfileSignout kSignoutSources[] = {
    signin_metrics::ProfileSignout::kPrefChanged,
    signin_metrics::ProfileSignout::kGoogleServiceNamePatternChanged,
    signin_metrics::ProfileSignout::kUserClickedSignoutSettings,
    signin_metrics::ProfileSignout::kAbortSignin,
    signin_metrics::ProfileSignout::kServerForcedDisable,
    signin_metrics::ProfileSignout::kAuthenticationFailedWithForceSignin,
    signin_metrics::ProfileSignout::kAccountRemovedFromDevice,
    signin_metrics::ProfileSignout::kSigninNotAllowedOnProfileInit,
    signin_metrics::ProfileSignout::kForceSignoutAlwaysAllowedForTest,
    signin_metrics::ProfileSignout::kUserDeletedAccountCookies,
    signin_metrics::ProfileSignout::kIosAccountRemovedFromDeviceAfterRestore,
    signin_metrics::ProfileSignout::kUserClickedRevokeSyncConsentSettings,
    signin_metrics::ProfileSignout::kUserClickedSignoutProfileMenu,
    signin_metrics::ProfileSignout::kSigninRetriggeredFromWebSignin,
    signin_metrics::ProfileSignout::
        kUserClickedSignoutFromUserPolicyNotificationDialog,
    signin_metrics::ProfileSignout::kAccountEmailUpdated,
    signin_metrics::ProfileSignout::
        kUserClickedSignoutFromClearBrowsingDataPage,
    signin_metrics::ProfileSignout::kGaiaCookieUpdated,
    signin_metrics::ProfileSignout::kAccountReconcilorReconcile,
    signin_metrics::ProfileSignout::kSigninManagerUpdateUPA,
    signin_metrics::ProfileSignout::kUserTappedUndoRightAfterSignIn,
    signin_metrics::ProfileSignout::
        kUserDeclinedHistorySyncAfterDedicatedSignIn,
    signin_metrics::ProfileSignout::kDeviceLockRemovedOnAutomotive,
    signin_metrics::ProfileSignout::kRevokeSyncFromSettings,
    signin_metrics::ProfileSignout::kCancelSyncConfirmationOnWebOnlySignedIn,
    signin_metrics::ProfileSignout::kIdleTimeoutPolicyTriggeredSignOut,
    signin_metrics::ProfileSignout::kCancelSyncConfirmationRemoveAccount,
    signin_metrics::ProfileSignout::kMovePrimaryAccount,
    signin_metrics::ProfileSignout::kSignoutDuringProfileDeletion,
    signin_metrics::ProfileSignout::kChangeAccountInAccountMenu,
    signin_metrics::ProfileSignout::kUserClickedSignoutInAccountMenu,
    signin_metrics::ProfileSignout::kUserDisabledAllowChromeSignIn,
};

// kNumberOfObsoleteSignoutSources should be updated when a ProfileSignout
// value is deprecated.
const int kNumberOfObsoleteSignoutSources = 6;
static_assert(std::size(kSignoutSources) + kNumberOfObsoleteSignoutSources ==
                  static_cast<int>(signin_metrics::ProfileSignout::kMaxValue) +
                      1,
              "kSignoutSources should enumerate all ProfileSignout values that "
              "are not obsolete");

INSTANTIATE_TEST_SUITE_P(AllSignoutSources,
                         ChromeSigninClientSignoutSourceTest,
                         testing::ValuesIn(kSignoutSources));

struct MetricsAccessPointHistogramNamesParam {
  signin_metrics::AccessPoint access_point;

  std::string extensions_signin_histogram_name;
  std::string extensions_sync_histogram_name;

  std::string all_bookmarks_signin_histogram_name;
  std::string bar_bookmarks_signin_histogram_name;
  std::string all_bookmarks_sync_histogram_name;
  std::string bar_bookmarks_sync_histogram_name;

  std::string suffix_test_name;
};

// Expected values for each access point group.
const MetricsAccessPointHistogramNamesParam params_per_access_point_group[] = {
    // Expecting 'PreUnoWebSignin'.
    {.access_point = signin_metrics::AccessPoint::ACCESS_POINT_WEB_SIGNIN,
     .extensions_signin_histogram_name =
         "Signin.Extensions.OnSignin.PreUnoWebSignin",
     .extensions_sync_histogram_name =
         "Signin.Extensions.OnSync.PreUnoWebSignin",
     .all_bookmarks_signin_histogram_name =
         "Signin.Bookmarks.OnSignin.AllBookmarks.PreUnoWebSignin",
     .bar_bookmarks_signin_histogram_name =
         "Signin.Bookmarks.OnSignin.BookmarksBar.PreUnoWebSignin",
     .all_bookmarks_sync_histogram_name =
         "Signin.Bookmarks.OnSync.AllBookmarks.PreUnoWebSignin",
     .bar_bookmarks_sync_histogram_name =
         "Signin.Bookmarks.OnSync.BookmarksBar.PreUnoWebSignin",
     .suffix_test_name = "AccessPointGroup_PreUnoWebSignin"},

    // Expecting 'UnoSigninBubble'.
    {.access_point = signin_metrics::AccessPoint::
         ACCESS_POINT_CHROME_SIGNIN_INTERCEPT_BUBBLE,
     .extensions_signin_histogram_name =
         "Signin.Extensions.OnSignin.UnoSigninBubble",
     .extensions_sync_histogram_name =
         "Signin.Extensions.OnSync.UnoSigninBubble",
     .all_bookmarks_signin_histogram_name =
         "Signin.Bookmarks.OnSignin.AllBookmarks.UnoSigninBubble",
     .bar_bookmarks_signin_histogram_name =
         "Signin.Bookmarks.OnSignin.BookmarksBar.UnoSigninBubble",
     .all_bookmarks_sync_histogram_name =
         "Signin.Bookmarks.OnSync.AllBookmarks.UnoSigninBubble",
     .bar_bookmarks_sync_histogram_name =
         "Signin.Bookmarks.OnSync.BookmarksBar.UnoSigninBubble",
     .suffix_test_name = "AccessPointGroup_UnoSigninBubble"},

    // Expecting 'ProfileCreation'.
    {.access_point = signin_metrics::AccessPoint::ACCESS_POINT_USER_MANAGER,
     .extensions_signin_histogram_name =
         "Signin.Extensions.OnSignin.ProfileCreation",
     .extensions_sync_histogram_name =
         "Signin.Extensions.OnSync.ProfileCreation",
     .all_bookmarks_signin_histogram_name =
         "Signin.Bookmarks.OnSignin.AllBookmarks.ProfileCreation",
     .bar_bookmarks_signin_histogram_name =
         "Signin.Bookmarks.OnSignin.BookmarksBar.ProfileCreation",
     .all_bookmarks_sync_histogram_name =
         "Signin.Bookmarks.OnSync.AllBookmarks.ProfileCreation",
     .bar_bookmarks_sync_histogram_name =
         "Signin.Bookmarks.OnSync.BookmarksBar.ProfileCreation",
     .suffix_test_name = "AccessPointGroup_ProfileCreation"},

    // Expecting 'ProfileMenu'.
    {.access_point =
         signin_metrics::AccessPoint::ACCESS_POINT_AVATAR_BUBBLE_SIGN_IN,
     .extensions_signin_histogram_name =
         "Signin.Extensions.OnSignin.ProfileMenu",
     .extensions_sync_histogram_name = "Signin.Extensions.OnSync.ProfileMenu",
     .all_bookmarks_signin_histogram_name =
         "Signin.Bookmarks.OnSignin.AllBookmarks.ProfileMenu",
     .bar_bookmarks_signin_histogram_name =
         "Signin.Bookmarks.OnSignin.BookmarksBar.ProfileMenu",
     .all_bookmarks_sync_histogram_name =
         "Signin.Bookmarks.OnSync.AllBookmarks.ProfileMenu",
     .bar_bookmarks_sync_histogram_name =
         "Signin.Bookmarks.OnSync.BookmarksBar.ProfileMenu",
     .suffix_test_name = "AccessPointGroup_ProfileMenu"},

    // Expecting 'Other'.
    {.access_point = signin_metrics::AccessPoint::ACCESS_POINT_EXTENSIONS,
     .extensions_signin_histogram_name = "Signin.Extensions.OnSignin.Other",
     .extensions_sync_histogram_name = "Signin.Extensions.OnSync.Other",
     .all_bookmarks_signin_histogram_name =
         "Signin.Bookmarks.OnSignin.AllBookmarks.Other",
     .bar_bookmarks_signin_histogram_name =
         "Signin.Bookmarks.OnSignin.BookmarksBar.Other",
     .all_bookmarks_sync_histogram_name =
         "Signin.Bookmarks.OnSync.AllBookmarks.Other",
     .bar_bookmarks_sync_histogram_name =
         "Signin.Bookmarks.OnSync.BookmarksBar.Other",
     .suffix_test_name = "AccessPointGroup_Other"},
};

// Helper to have a better parametrized test.
std::string ParamToTestSuffix(
    const ::testing::TestParamInfo<
        std::tuple<signin::ConsentLevel,
                   MetricsAccessPointHistogramNamesParam>>& info) {
  std::string consent_level_string =
      std::get<0>(info.param) == signin::ConsentLevel::kSignin ? "Signin"
                                                               : "Sync";
  return consent_level_string + "_" + std::get<1>(info.param).suffix_test_name;
}

class ChromeSigninClientMetricsTest
    : public ::testing::TestWithParam<
          std::tuple<signin::ConsentLevel,
                     MetricsAccessPointHistogramNamesParam>> {
 public:
  TestingProfile* profile() { return testing_profile_.get(); }

  const base::HistogramTester& histogram_tester() { return histogram_tester_; }

  // Checks both AllBookmarks and BookmarksBar histograms with no access point.
  void ExpectSigninExtensionsAndBookmarksHistogramValues(
      size_t expected_extensions_count,
      size_t expected_all_bookmark_count,
      size_t expected_bar_bookmarks_count,
      size_t signin_expected_bucket_count,
      size_t sync_expected_bucket_count) {
    // Extensions checks.
    histogram_tester_.ExpectUniqueSample("Signin.Extensions.OnSignin",
                                         expected_extensions_count,
                                         signin_expected_bucket_count);
    histogram_tester_.ExpectUniqueSample("Signin.Extensions.OnSync",
                                         expected_extensions_count,
                                         sync_expected_bucket_count);

    // Bookmarks checks.
    histogram_tester_.ExpectUniqueSample(
        "Signin.Bookmarks.OnSignin.AllBookmarks", expected_all_bookmark_count,
        signin_expected_bucket_count);
    histogram_tester_.ExpectUniqueSample(
        "Signin.Bookmarks.OnSignin.BookmarksBar", expected_bar_bookmarks_count,
        signin_expected_bucket_count);

    histogram_tester_.ExpectUniqueSample("Signin.Bookmarks.OnSync.AllBookmarks",
                                         expected_all_bookmark_count,
                                         sync_expected_bucket_count);
    histogram_tester_.ExpectUniqueSample("Signin.Bookmarks.OnSync.BookmarksBar",
                                         expected_bar_bookmarks_count,
                                         sync_expected_bucket_count);
  }

 private:
  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<TestingProfile> testing_profile_ =
      TestingProfile::Builder().Build();
  base::HistogramTester histogram_tester_;
};

TEST_P(ChromeSigninClientMetricsTest, ExentsionsAndBookmarkCount) {
  MockChromeSigninClient client(profile());
  size_t all_bookmarks_count = 5;
  size_t bar_bookmarks_count = 3;
  size_t extensions_count = 4;

  EXPECT_CALL(client, GetAllBookmarksCount())
      .WillOnce(testing::Return(all_bookmarks_count));
  EXPECT_CALL(client, GetBookmarkBarBookmarksCount())
      .WillOnce(testing::Return(bar_bookmarks_count));
  EXPECT_CALL(client, GetExtensionsCount())
      .WillOnce(testing::Return(extensions_count));

  CoreAccountInfo account;
  account.email = "example@example.com";
  account.gaia = "gaia_example";
  ASSERT_FALSE(account.IsEmpty());

  signin::ConsentLevel consent_level = std::get<0>(GetParam());
  signin::PrimaryAccountChangeEvent::State previous_state;
  // When testing for `kSync`, simulate a previous state with the same account
  // having `kSignin`.
  // A separate test is done for a direct change to `kSync`:
  // `BookmarkCountWithAccountInSyncDirectly`.
  if (consent_level == signin::ConsentLevel::kSync) {
    previous_state.primary_account = account;
    previous_state.consent_level = signin::ConsentLevel::kSignin;
  }
  MetricsAccessPointHistogramNamesParam test_params = std::get<1>(GetParam());
  signin::PrimaryAccountChangeEvent event_details{
      previous_state,
      /*current_state=*/
      signin::PrimaryAccountChangeEvent::State(account, consent_level),
      test_params.access_point};
  // Ensure the events types are correct for both consent levels.
  if (consent_level == signin::ConsentLevel::kSync) {
    ASSERT_EQ(event_details.GetEventTypeFor(signin::ConsentLevel::kSignin),
              signin::PrimaryAccountChangeEvent::Type::kNone);
    ASSERT_EQ(event_details.GetEventTypeFor(signin::ConsentLevel::kSync),
              signin::PrimaryAccountChangeEvent::Type::kSet);
  } else {
    ASSERT_EQ(event_details.GetEventTypeFor(signin::ConsentLevel::kSignin),
              signin::PrimaryAccountChangeEvent::Type::kSet);
    ASSERT_EQ(event_details.GetEventTypeFor(signin::ConsentLevel::kSync),
              signin::PrimaryAccountChangeEvent::Type::kNone);
  }

  // Simulate primary account changed.
  client.OnPrimaryAccountChanged(event_details);

  // Check for expected histograms values below.
  const size_t signin_expected_bucket_count =
      consent_level == signin::ConsentLevel::kSignin ? 1 : 0;
  const size_t sync_expected_bucket_count =
      consent_level == signin::ConsentLevel::kSync ? 1 : 0;

  // Checks histogram values without access point group names.
  ExpectSigninExtensionsAndBookmarksHistogramValues(
      extensions_count, all_bookmarks_count, bar_bookmarks_count,
      signin_expected_bucket_count, sync_expected_bucket_count);

  // For Extensions with access point group name.
  histogram_tester().ExpectUniqueSample(
      test_params.extensions_signin_histogram_name, extensions_count,
      signin_expected_bucket_count);
  histogram_tester().ExpectUniqueSample(
      test_params.extensions_sync_histogram_name, extensions_count,
      sync_expected_bucket_count);

  // For AllBookmarks with access point group name.
  histogram_tester().ExpectUniqueSample(
      test_params.all_bookmarks_signin_histogram_name, all_bookmarks_count,
      signin_expected_bucket_count);
  histogram_tester().ExpectUniqueSample(
      test_params.all_bookmarks_sync_histogram_name, all_bookmarks_count,
      sync_expected_bucket_count);

  // For BookmarksBar with access point group name.
  histogram_tester().ExpectUniqueSample(
      test_params.bar_bookmarks_signin_histogram_name, bar_bookmarks_count,
      signin_expected_bucket_count);
  histogram_tester().ExpectUniqueSample(
      test_params.bar_bookmarks_sync_histogram_name, bar_bookmarks_count,
      sync_expected_bucket_count);

  // The exact counts makes sure that no other histograms within this family
  // records unwanted values. For example not recording Sync histograms with a
  // Signin event and vice versa, or histogram for different access points than
  // the one being tested.
  // Exact sample counts histograms are done above.
  base::HistogramTester::CountsMap expected_bkmark_counts;
  base::HistogramTester::CountsMap expected_extensions_count;
  if (consent_level == signin::ConsentLevel::kSignin) {
    expected_extensions_count["Signin.Extensions.OnSignin"] = 1;
    expected_extensions_count[test_params.extensions_signin_histogram_name] = 1;
    expected_bkmark_counts["Signin.Bookmarks.OnSignin.AllBookmarks"] = 1;
    expected_bkmark_counts["Signin.Bookmarks.OnSignin.BookmarksBar"] = 1;
    expected_bkmark_counts[test_params.all_bookmarks_signin_histogram_name] = 1;
    expected_bkmark_counts[test_params.bar_bookmarks_signin_histogram_name] = 1;
  } else if (consent_level == signin::ConsentLevel::kSync) {
    expected_extensions_count["Signin.Extensions.OnSync"] = 1;
    expected_extensions_count[test_params.extensions_sync_histogram_name] = 1;
    expected_bkmark_counts["Signin.Bookmarks.OnSync.AllBookmarks"] = 1;
    expected_bkmark_counts["Signin.Bookmarks.OnSync.BookmarksBar"] = 1;
    expected_bkmark_counts[test_params.all_bookmarks_sync_histogram_name] = 1;
    expected_bkmark_counts[test_params.bar_bookmarks_sync_histogram_name] = 1;
  }
  EXPECT_THAT(histogram_tester().GetTotalCountsForPrefix("Signin.Bookmarks."),
              testing::ContainerEq(expected_bkmark_counts));
  EXPECT_THAT(histogram_tester().GetTotalCountsForPrefix("Signin.Extensions."),
              testing::ContainerEq(expected_extensions_count));
}

INSTANTIATE_TEST_SUITE_P(
    ,
    ChromeSigninClientMetricsTest,
    testing::Combine(testing::ValuesIn({signin::ConsentLevel::kSignin,
                                        signin::ConsentLevel::kSync}),
                     testing::ValuesIn(params_per_access_point_group)),
    &ParamToTestSuffix);

// In this test, the account changes is directly set to `kSync`, without a prior
// state where `kSignin` is set, this will trigger both changes for `kSignin`
// and `kSync`, only testing a single access point.
TEST_F(ChromeSigninClientMetricsTest,
       ExentsionsAndBookmarksCountWithAccountInSyncDirectly) {
  MockChromeSigninClient client(profile());
  size_t all_bookmarks_count = 7;
  size_t bar_bookmarks_count = 5;
  size_t extensions_count = 3;

  // `Times(2)` for both Signin then Sync.
  EXPECT_CALL(client, GetAllBookmarksCount())
      .Times(2)
      .WillRepeatedly(testing::Return(all_bookmarks_count));
  EXPECT_CALL(client, GetBookmarkBarBookmarksCount())
      .Times(2)
      .WillRepeatedly(testing::Return(bar_bookmarks_count));
  EXPECT_CALL(client, GetExtensionsCount())
      .Times(2)
      .WillRepeatedly(testing::Return(extensions_count));

  CoreAccountInfo account;
  account.email = "example@example.com";
  account.gaia = "gaia_example";
  ASSERT_FALSE(account.IsEmpty());

  // State goes from no account to an account with `kSync` set.
  // It will trigger both events to `kSignin` and `kSync`.
  signin::PrimaryAccountChangeEvent event_details{
      /*previous_state=*/signin::PrimaryAccountChangeEvent::State(),
      /*current_state=*/
      signin::PrimaryAccountChangeEvent::State(account,
                                               signin::ConsentLevel::kSync),
      signin_metrics::AccessPoint::ACCESS_POINT_WEB_SIGNIN};
  // Both Signin and Sync event are being set.
  ASSERT_EQ(event_details.GetEventTypeFor(signin::ConsentLevel::kSignin),
            signin::PrimaryAccountChangeEvent::Type::kSet);
  ASSERT_EQ(event_details.GetEventTypeFor(signin::ConsentLevel::kSync),
            signin::PrimaryAccountChangeEvent::Type::kSet);

  // Simulate primary account changed.
  client.OnPrimaryAccountChanged(event_details);

  // Check for expected histograms values below.

  // Checks histogram values without access point group names.
  histogram_tester().ExpectUniqueSample("Signin.Extensions.OnSignin",
                                        extensions_count, 1);
  histogram_tester().ExpectUniqueSample("Signin.Extensions.OnSync",
                                        extensions_count, 1);
  histogram_tester().ExpectUniqueSample(
      "Signin.Bookmarks.OnSignin.AllBookmarks", all_bookmarks_count, 1);
  histogram_tester().ExpectUniqueSample("Signin.Bookmarks.OnSync.AllBookmarks",
                                        all_bookmarks_count, 1);
  histogram_tester().ExpectUniqueSample(
      "Signin.Bookmarks.OnSignin.BookmarksBar", bar_bookmarks_count, 1);
  histogram_tester().ExpectUniqueSample("Signin.Bookmarks.OnSync.BookmarksBar",
                                        bar_bookmarks_count, 1);

  // For Extensions with access point group name.
  histogram_tester().ExpectUniqueSample(
      "Signin.Extensions.OnSignin.PreUnoWebSignin", extensions_count, 1);
  histogram_tester().ExpectUniqueSample(
      "Signin.Extensions.OnSync.PreUnoWebSignin", extensions_count, 1);

  // For AllBookmarks with access point group name.
  histogram_tester().ExpectUniqueSample(
      "Signin.Bookmarks.OnSignin.AllBookmarks.PreUnoWebSignin",
      all_bookmarks_count, 1);
  histogram_tester().ExpectUniqueSample(
      "Signin.Bookmarks.OnSync.AllBookmarks.PreUnoWebSignin",
      all_bookmarks_count, 1);

  // For BookmarksBar with access point group name.
  histogram_tester().ExpectUniqueSample(
      "Signin.Bookmarks.OnSignin.BookmarksBar.PreUnoWebSignin",
      bar_bookmarks_count, 1);
  histogram_tester().ExpectUniqueSample(
      "Signin.Bookmarks.OnSync.BookmarksBar.PreUnoWebSignin",
      bar_bookmarks_count, 1);

  // Makes sure that no other unwanted histograms are recorded (Mainly for
  // other access point groups). Exact sample counts are checked above.
  base::HistogramTester::CountsMap expected_counts;
  expected_counts["Signin.Bookmarks.OnSignin.AllBookmarks"] = 1;
  expected_counts["Signin.Bookmarks.OnSignin.BookmarksBar"] = 1;
  expected_counts["Signin.Bookmarks.OnSync.AllBookmarks"] = 1;
  expected_counts["Signin.Bookmarks.OnSync.BookmarksBar"] = 1;
  expected_counts["Signin.Bookmarks.OnSignin.AllBookmarks.PreUnoWebSignin"] = 1;
  expected_counts["Signin.Bookmarks.OnSignin.BookmarksBar.PreUnoWebSignin"] = 1;
  expected_counts["Signin.Bookmarks.OnSync.AllBookmarks.PreUnoWebSignin"] = 1;
  expected_counts["Signin.Bookmarks.OnSync.BookmarksBar.PreUnoWebSignin"] = 1;
  EXPECT_THAT(histogram_tester().GetTotalCountsForPrefix("Signin.Bookmarks."),
              testing::ContainerEq(expected_counts));

  // Makes sure that no other unwanted histograms are recorded (Mainly for
  // other access point groups). Exact sample counts are checked above.
  base::HistogramTester::CountsMap extensions_expected_counts;
  extensions_expected_counts["Signin.Extensions.OnSignin"] = 1;
  extensions_expected_counts["Signin.Extensions.OnSignin.PreUnoWebSignin"] = 1;
  extensions_expected_counts["Signin.Extensions.OnSync"] = 1;
  extensions_expected_counts["Signin.Extensions.OnSync.PreUnoWebSignin"] = 1;
  EXPECT_THAT(histogram_tester().GetTotalCountsForPrefix("Signin.Extensions."),
              testing::ContainerEq(extensions_expected_counts));
}

// Not expecting any histogram to be recorded when no account update happens.
TEST_F(ChromeSigninClientMetricsTest,
       ExentsionsAndBookmarksCountWithAccountUpdate_kNone) {
  MockChromeSigninClient client(profile());

  EXPECT_CALL(client, GetAllBookmarksCount()).Times(0);
  EXPECT_CALL(client, GetBookmarkBarBookmarksCount()).Times(0);
  EXPECT_CALL(client, GetExtensionsCount()).Times(0);

  // Event details to simulate no update. Either empty or same value set.
  signin::PrimaryAccountChangeEvent event_details{
      /*previous_state=*/signin::PrimaryAccountChangeEvent::State(),
      /*current_state=*/signin::PrimaryAccountChangeEvent::State(),
      signin_metrics::AccessPoint::ACCESS_POINT_WEB_SIGNIN};
  ASSERT_EQ(event_details.GetEventTypeFor(signin::ConsentLevel::kSignin),
            signin::PrimaryAccountChangeEvent::Type::kNone);
  ASSERT_EQ(event_details.GetEventTypeFor(signin::ConsentLevel::kSync),
            signin::PrimaryAccountChangeEvent::Type::kNone);

  // Simulate primary account changed.
  client.OnPrimaryAccountChanged(event_details);

  // `expected_counts` is empty as we expect no histograms related to
  // `Signin.Bookmarks` or `Signin.Extensions to be recorded.
  base::HistogramTester::CountsMap expected_counts;
  EXPECT_THAT(histogram_tester().GetTotalCountsForPrefix("Signin.Bookmarks."),
              testing::ContainerEq(expected_counts));
  EXPECT_THAT(histogram_tester().GetTotalCountsForPrefix("Signin.Extensions."),
              testing::ContainerEq(expected_counts));
}

// Not expecting any histogram to be recorded when revoking account consent.
TEST_F(ChromeSigninClientMetricsTest,
       ExentsionsAndBookmarksCountWithAccountUpdate_kCleared) {
  MockChromeSigninClient client(profile());

  EXPECT_CALL(client, GetAllBookmarksCount()).Times(0);
  EXPECT_CALL(client, GetBookmarkBarBookmarksCount()).Times(0);
  EXPECT_CALL(client, GetExtensionsCount()).Times(0);

  CoreAccountInfo account;
  account.email = "example@example.com";
  account.gaia = "gaia_example";
  ASSERT_FALSE(account.IsEmpty());

  // Simulating revoking Signin consent.
  signin::PrimaryAccountChangeEvent event_details{
      /*previous_state=*/signin::PrimaryAccountChangeEvent::State(
          account, signin::ConsentLevel::kSignin),
      /*current_state=*/signin::PrimaryAccountChangeEvent::State(),
      signin_metrics::ProfileSignout::kTest};
  ASSERT_EQ(event_details.GetEventTypeFor(signin::ConsentLevel::kSignin),
            signin::PrimaryAccountChangeEvent::Type::kCleared);
  ASSERT_EQ(event_details.GetEventTypeFor(signin::ConsentLevel::kSync),
            signin::PrimaryAccountChangeEvent::Type::kNone);

  // Simulate primary account changed.
  client.OnPrimaryAccountChanged(event_details);

  // `expected_counts` is empty as we expect no histograms related to
  // `Signin.Bookmarks` or `Signin.Extensions to be recorded.
  base::HistogramTester::CountsMap expected_counts;
  EXPECT_THAT(histogram_tester().GetTotalCountsForPrefix("Signin.Bookmarks."),
              testing::ContainerEq(expected_counts));
  EXPECT_THAT(histogram_tester().GetTotalCountsForPrefix("Signin.Extensions."),
              testing::ContainerEq(expected_counts));
}

// Not expecting any histogram to be recorded when the bookmark service is null.
TEST_F(ChromeSigninClientMetricsTest,
       ExentsionsAndBookmarksCountWithAccountSigningin_ServiceNull) {
  MockChromeSigninClient client(profile());

  // Returning `std::nullopt` to simulate the service being nullptr.
  EXPECT_CALL(client, GetAllBookmarksCount())
      .WillOnce(testing::Return(std::nullopt));
  EXPECT_CALL(client, GetBookmarkBarBookmarksCount())
      .WillOnce(testing::Return(std::nullopt));
  EXPECT_CALL(client, GetExtensionsCount())
      .WillOnce(testing::Return(std::nullopt));

  CoreAccountInfo account;
  account.email = "example@example.com";
  account.gaia = "gaia_example";
  ASSERT_FALSE(account.IsEmpty());

  // Simulating signing in update.
  signin::PrimaryAccountChangeEvent event_details{
      /*previous_state=*/signin::PrimaryAccountChangeEvent::State(),
      /*current_state=*/
      signin::PrimaryAccountChangeEvent::State(account,
                                               signin::ConsentLevel::kSignin),
      signin_metrics::AccessPoint::ACCESS_POINT_WEB_SIGNIN};
  ASSERT_EQ(event_details.GetEventTypeFor(signin::ConsentLevel::kSignin),
            signin::PrimaryAccountChangeEvent::Type::kSet);
  ASSERT_EQ(event_details.GetEventTypeFor(signin::ConsentLevel::kSync),
            signin::PrimaryAccountChangeEvent::Type::kNone);

  // Simulate primary account changed.
  client.OnPrimaryAccountChanged(event_details);

  // `expected_counts` is empty as we expect no histograms related to
  // `Signin.Bookmarks` or `Signin.Extensions to be recorded.
  base::HistogramTester::CountsMap expected_counts;
  EXPECT_THAT(histogram_tester().GetTotalCountsForPrefix("Signin.Bookmarks."),
              testing::ContainerEq(expected_counts));
  EXPECT_THAT(histogram_tester().GetTotalCountsForPrefix("Signin.Extensions."),
              testing::ContainerEq(expected_counts));
}

#endif  // !BUILDFLAG(IS_CHROMEOS_ASH) && !BUILDFLAG(IS_ANDROID)
