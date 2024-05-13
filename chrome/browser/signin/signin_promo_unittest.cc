// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/signin_promo.h"

#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/signin/chrome_signin_pref_names.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/signin/identity_test_environment_profile_adaptor.h"
#include "chrome/browser/signin/signin_promo_util.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "chrome/test/base/scoped_testing_local_state.h"
#include "chrome/test/base/testing_browser_process.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/base/signin_prefs.h"
#include "components/signin/public/base/signin_switches.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "components/sync/base/command_line_switches.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
namespace signin {

#if !BUILDFLAG(IS_CHROMEOS_ASH)
TEST(SigninPromoTest, TestPromoURL) {
  GURL::Replacements replace_query;
  replace_query.SetQueryStr("access_point=0&reason=0&auto_close=1");
  EXPECT_EQ(
      GURL(chrome::kChromeUIChromeSigninURL).ReplaceComponents(replace_query),
      GetEmbeddedPromoURL(signin_metrics::AccessPoint::ACCESS_POINT_START_PAGE,
                          signin_metrics::Reason::kSigninPrimaryAccount, true));
  replace_query.SetQueryStr("access_point=15&reason=1");
  EXPECT_EQ(
      GURL(chrome::kChromeUIChromeSigninURL).ReplaceComponents(replace_query),
      GetEmbeddedPromoURL(
          signin_metrics::AccessPoint::ACCESS_POINT_SIGNIN_PROMO,
          signin_metrics::Reason::kAddSecondaryAccount, false));
}

TEST(SigninPromoTest, TestReauthURL) {
  GURL::Replacements replace_query;
  replace_query.SetQueryStr(
      "access_point=0&reason=6&auto_close=1"
      "&email=example%40domain.com&validateEmail=1"
      "&readOnlyEmail=1");
  EXPECT_EQ(
      GURL(chrome::kChromeUIChromeSigninURL).ReplaceComponents(replace_query),
      GetEmbeddedReauthURLWithEmail(
          signin_metrics::AccessPoint::ACCESS_POINT_START_PAGE,
          signin_metrics::Reason::kFetchLstOnly, "example@domain.com"));
}
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)

TEST(SigninPromoTest, SigninURLForDice) {
  EXPECT_EQ(
      "https://accounts.google.com/signin/chrome/sync?ssp=1&"
      "color_scheme=dark&flow=promo",
      GetChromeSyncURLForDice(
          {.request_dark_scheme = true, .flow = Flow::PROMO}));
  EXPECT_EQ(
      "https://accounts.google.com/signin/chrome/sync?ssp=1&"
      "email_hint=email%40gmail.com&continue=https%3A%2F%2Fcontinue_url%2F",
      GetChromeSyncURLForDice(
          {"email@gmail.com", GURL("https://continue_url/")}));
  EXPECT_EQ(
      "https://accounts.google.com/signin/chrome/"
      "sync?ssp=1&flow=embedded_promo",
      GetChromeSyncURLForDice({.flow = Flow::EMBEDDED_PROMO}));
  EXPECT_EQ(
      "https://accounts.google.com/AddSession?"
      "Email=email%40gmail.com&continue=https%3A%2F%2Fcontinue_url%2F",
      GetAddAccountURLForDice("email@gmail.com",
                              GURL("https://continue_url/")));
}

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
TEST(SignInPromoVersionTest, SignInPromoVersions) {
  ScopedTestingLocalState local_state(TestingBrowserProcess::GetGlobal());
  content::BrowserTaskEnvironment task_environment;

  IdentityTestEnvironment identity_test_env;
  IdentityManager* identity_manager = identity_test_env.identity_manager();

  // No Account present.
  EXPECT_EQ(SignInAutofillBubbleVersion::kNoAccount,
            GetSignInPromoVersion(identity_manager));

  // Web signed in.
  identity_test_env.MakeAccountAvailable("test@email.com",
                                         {.set_cookie = true});
  EXPECT_EQ(SignInAutofillBubbleVersion::kWebSignedIn,
            GetSignInPromoVersion(identity_manager));

  // Syncing.
  AccountInfo info = identity_test_env.MakePrimaryAccountAvailable(
      "test@email.com", ConsentLevel::kSync);
  EXPECT_EQ(SignInAutofillBubbleVersion::kNoPromo,
            GetSignInPromoVersion(identity_manager));

  // Sync paused state.
  identity_test_env.UpdatePersistentErrorOfRefreshTokenForAccount(
      info.account_id, GoogleServiceAuthError(
                           GoogleServiceAuthError::State::USER_NOT_SIGNED_UP));
  EXPECT_EQ(SignInAutofillBubbleVersion::kNoPromo,
            GetSignInPromoVersion(identity_manager));

  // Remove account.
  identity_test_env.ClearPrimaryAccount();
  EXPECT_EQ(SignInAutofillBubbleVersion::kNoAccount,
            GetSignInPromoVersion(identity_manager));

  // Signed in.
  info = identity_test_env.MakePrimaryAccountAvailable("test@email.com",
                                                       ConsentLevel::kSignin);
  EXPECT_EQ(SignInAutofillBubbleVersion::kNoPromo,
            GetSignInPromoVersion(identity_manager));

  // Sign in pending state.
  identity_test_env.UpdatePersistentErrorOfRefreshTokenForAccount(
      info.account_id, GoogleServiceAuthError(
                           GoogleServiceAuthError::State::USER_NOT_SIGNED_UP));
  EXPECT_EQ(SignInAutofillBubbleVersion::kSignInPending,
            GetSignInPromoVersion(identity_manager));
}
#endif  // !BUILDFLAG(ENABLE_DICE_SUPPORT)

class ShowPromoTest : public testing::Test {
 public:
  ShowPromoTest() {
    profile_ = IdentityTestEnvironmentProfileAdaptor::
        CreateProfileForIdentityTestEnvironment();
    identity_test_env_adaptor_ =
        std::make_unique<IdentityTestEnvironmentProfileAdaptor>(profile_.get());
  }

  IdentityManager* identity_manager() {
    return identity_test_env_adaptor_->identity_test_env()->identity_manager();
  }

  Profile* profile() { return profile_.get(); }

 private:
  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<TestingProfile> profile_;
  std::unique_ptr<IdentityTestEnvironmentProfileAdaptor>
      identity_test_env_adaptor_;
};

TEST_F(ShowPromoTest, DoNotShowSignInPromoWithoutExplicitBrowserSignin) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(
      switches::kExplicitBrowserSigninUIOnDesktop);

  EXPECT_FALSE(ShouldShowSignInPromo(*profile(),
                                     SignInAutofillBubblePromoType::Passwords));
}

#if !BUILDFLAG(IS_ANDROID)
class ShowSyncPromoTest : public ShowPromoTest {
 protected:
  void DisableSync() {
    base::CommandLine::ForCurrentProcess()->AppendSwitch(syncer::kDisableSync);
  }
};

// Verifies that ShouldShowSyncPromo returns false if sync is disabled by
// policy.
TEST_F(ShowSyncPromoTest, ShouldShowSyncPromoSyncDisabled) {
  DisableSync();
  EXPECT_FALSE(ShouldShowSyncPromo(*profile()));
}

// Verifies that ShouldShowSyncPromo returns true if all conditions to
// show the promo are met.
TEST_F(ShowSyncPromoTest, ShouldShowSyncPromoSyncEnabled) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  // No sync promo on Ash.
  EXPECT_FALSE(ShouldShowSyncPromo(*profile()));
#else
  EXPECT_TRUE(ShouldShowSyncPromo(*profile()));
#endif
}
#endif  // !BUILDFLAG(IS_ANDROID)

#if !BUILDFLAG(IS_CHROMEOS_ASH)
TEST_F(ShowSyncPromoTest, ShowPromoWithSignedInAccount) {
  MakePrimaryAccountAvailable(identity_manager(), "test@email.com",
                              ConsentLevel::kSignin);
  EXPECT_TRUE(ShouldShowSyncPromo(*profile()));
}

TEST_F(ShowSyncPromoTest, DoNotShowPromoWithSyncingAccount) {
  MakePrimaryAccountAvailable(identity_manager(), "test@email.com",
                              ConsentLevel::kSync);
  EXPECT_FALSE(ShouldShowSyncPromo(*profile()));
}
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
class ShowSigninPromoTestExplicitBrowserSignin : public ShowPromoTest {
 public:
  std::string gaia_id() {
    return identity_manager()
        ->GetPrimaryAccountInfo(ConsentLevel::kSignin)
        .gaia;
  }

 private:
  base::test::ScopedFeatureList feature_list{
      switches::kExplicitBrowserSigninUIOnDesktop};
};

TEST_F(ShowSigninPromoTestExplicitBrowserSignin, ShowPromoWithNoAccount) {
  EXPECT_TRUE(ShouldShowSignInPromo(*profile(),
                                    SignInAutofillBubblePromoType::Payments));
}

TEST_F(ShowSigninPromoTestExplicitBrowserSignin,
       ShowPromoWithWebSignedInAccount) {
  MakeAccountAvailable(identity_manager(), "test@email.com");
  EXPECT_TRUE(ShouldShowSignInPromo(*profile(),
                                    SignInAutofillBubblePromoType::Addresses));
}

TEST_F(ShowSigninPromoTestExplicitBrowserSignin,
       ShowPromoWithSignInPausedAccount) {
  AccountInfo info = MakePrimaryAccountAvailable(
      identity_manager(), "test@email.com", ConsentLevel::kSignin);
  UpdatePersistentErrorOfRefreshTokenForAccount(
      identity_manager(), info.account_id,
      GoogleServiceAuthError(
          GoogleServiceAuthError::State::USER_NOT_SIGNED_UP));
  EXPECT_TRUE(ShouldShowSignInPromo(*profile(),
                                    SignInAutofillBubblePromoType::Passwords));
}

TEST_F(ShowSigninPromoTestExplicitBrowserSignin,
       DoNotShowPromoWithAlreadySignedInAccount) {
  MakePrimaryAccountAvailable(identity_manager(), "test@email.com",
                              ConsentLevel::kSignin);
  EXPECT_FALSE(ShouldShowSignInPromo(*profile(),
                                     SignInAutofillBubblePromoType::Payments));
}

TEST_F(ShowSigninPromoTestExplicitBrowserSignin,
       DoNotShowPromoWithAlreadySyncingAccount) {
  MakePrimaryAccountAvailable(identity_manager(), "test@email.com",
                              ConsentLevel::kSync);
  EXPECT_FALSE(ShouldShowSignInPromo(*profile(),
                                     SignInAutofillBubblePromoType::Addresses));
}

TEST_F(ShowSigninPromoTestExplicitBrowserSignin,
       DoNotShowPromoWithOffTheRecordProfile) {
  EXPECT_FALSE(ShouldShowSignInPromo(
      *profile()->GetPrimaryOTRProfile(/*create_if_needed=*/true),
      SignInAutofillBubblePromoType::Payments));
}

TEST_F(ShowSigninPromoTestExplicitBrowserSignin,
       DoNotShowPromoAfterFiveTimesShown) {
  EXPECT_TRUE(ShouldShowSignInPromo(*profile(),
                                    SignInAutofillBubblePromoType::Passwords));

  profile()->GetPrefs()->SetInteger(
      prefs::kPasswordSignInPromoShownCountPerProfile, 5);

  EXPECT_FALSE(ShouldShowSignInPromo(*profile(),
                                     SignInAutofillBubblePromoType::Passwords));
  EXPECT_TRUE(ShouldShowSignInPromo(*profile(),
                                    SignInAutofillBubblePromoType::Addresses));
}

TEST_F(ShowSigninPromoTestExplicitBrowserSignin,
       DoNotShowPromoAfterTwoTimesDismissed) {
  EXPECT_TRUE(ShouldShowSignInPromo(*profile(),
                                    SignInAutofillBubblePromoType::Passwords));
  EXPECT_TRUE(ShouldShowSignInPromo(*profile(),
                                    SignInAutofillBubblePromoType::Addresses));

  profile()->GetPrefs()->SetInteger(
      prefs::kAutofillSignInPromoDismissCountPerProfile, 2);

  EXPECT_FALSE(ShouldShowSignInPromo(*profile(),
                                     SignInAutofillBubblePromoType::Passwords));
  EXPECT_FALSE(ShouldShowSignInPromo(*profile(),
                                     SignInAutofillBubblePromoType::Addresses));
}

TEST_F(ShowSigninPromoTestExplicitBrowserSignin,
       ShowPromoAfterTwoTimesDismissedByDifferentAccounts) {
  profile()->GetPrefs()->SetInteger(
      prefs::kAutofillSignInPromoDismissCountPerProfile, 1);
  SigninPrefs prefs(*profile()->GetPrefs());
  prefs.IncrementAutofillSigninPromoDismissCount("gaia_id");

  EXPECT_TRUE(ShouldShowSignInPromo(*profile(),
                                    SignInAutofillBubblePromoType::Passwords));
  EXPECT_TRUE(ShouldShowSignInPromo(*profile(),
                                    SignInAutofillBubblePromoType::Addresses));
}
#endif  // BUILDFLAG(ENABLE_DICE_SUPPORT)

}  // namespace signin
