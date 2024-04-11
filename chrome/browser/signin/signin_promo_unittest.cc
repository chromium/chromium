// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/signin_promo.h"

#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/signin/identity_test_environment_profile_adaptor.h"
#include "chrome/browser/signin/signin_promo_util.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/base/signin_switches.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
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

// Tests for ShouldShowPromo.
#if !BUILDFLAG(IS_CHROMEOS_ASH)
TEST_F(ShowPromoTest, ShowPromoWithNoAccount) {
  EXPECT_TRUE(ShouldShowPromo(*profile(), ConsentLevel::kSync));
}

TEST_F(ShowPromoTest, ShowPromoWithSignedInAccount) {
  MakePrimaryAccountAvailable(identity_manager(), "test@email.com",
                              ConsentLevel::kSignin);
  EXPECT_TRUE(ShouldShowPromo(*profile(), ConsentLevel::kSync));
}

TEST_F(ShowPromoTest, DoNotShowPromoWithSyncingAccount) {
  MakePrimaryAccountAvailable(identity_manager(), "test@email.com",
                              ConsentLevel::kSync);
  EXPECT_FALSE(ShouldShowPromo(*profile(), ConsentLevel::kSync));
}
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)

// Tests for ShouldShowSignInPromo.
TEST_F(ShowPromoTest, DoNotShowSignInPromoWithoutExplicitBrowserSignin) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(
      switches::kExplicitBrowserSigninUIOnDesktop);

  EXPECT_FALSE(ShouldShowSignInPromo(*profile(),
                                     SignInAutofillBubblePromoType::Passwords));
}

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
class ShowSigninPromoTestExplicitBrowserSignin : public ShowPromoTest {
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
       DoNotShowPromoAfterFiveTimesShown) {
  // TODO (crbug.com/319411728): Implement a counter and test it.
}

TEST_F(ShowSigninPromoTestExplicitBrowserSignin,
       DoNotShowPromoAfterTwoTimesDismissed) {
  // TODO (crbug.com/319411728): Implement a counter and test it.
}
#endif  // BUILDFLAG(ENABLE_DICE_SUPPORT)

}  // namespace signin
