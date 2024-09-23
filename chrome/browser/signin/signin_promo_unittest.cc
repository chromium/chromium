// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/signin_promo.h"

#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/autofill/personal_data_manager_factory.h"
#include "chrome/browser/signin/chrome_signin_pref_names.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/signin/identity_test_environment_profile_adaptor.h"
#include "chrome/browser/signin/signin_promo_util.h"
#include "chrome/common/webui_url_constants.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/autofill/core/browser/test_utils/test_profiles.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/base/signin_metrics.h"
#include "components/signin/public/base/signin_prefs.h"
#include "components/signin/public/base/signin_switches.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "components/sync/base/command_line_switches.h"
#include "content/public/test/browser_task_environment.h"
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

TEST_F(ShowPromoTest, DoNotShowSignInPromoWithoutExplicitBrowserSignin) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{},
      /*disabled_features=*/{switches::kExplicitBrowserSigninUIOnDesktop,
                             switches::kImprovedSigninUIOnDesktop});

  EXPECT_FALSE(ShouldShowPasswordSignInPromo(*profile()));
  EXPECT_FALSE(ShouldShowAddressSignInPromo(*profile(),
                                            autofill::test::StandardProfile()));
}

TEST_F(ShowPromoTest, DoNotShowAddressSignInPromoWithoutImprovedBrowserSignin) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{switches::kExplicitBrowserSigninUIOnDesktop},
      /*disabled_features=*/{switches::kImprovedSigninUIOnDesktop});

  EXPECT_FALSE(ShouldShowAddressSignInPromo(*profile(),
                                            autofill::test::StandardProfile()));
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
  void SetUp() override {
    ShowPromoTest::SetUp();
    feature_list.InitWithFeatures(
        /*enabled_features=*/{switches::kExplicitBrowserSigninUIOnDesktop,
                              switches::kImprovedSigninUIOnDesktop},
        /*disabled_features=*/{});
  }
  std::string gaia_id() {
    return identity_manager()
        ->GetPrimaryAccountInfo(ConsentLevel::kSignin)
        .gaia;
  }

  autofill::AutofillProfile CreateAddress(
      const std::string& country_code = "US") {
    return autofill::test::StandardProfile(AddressCountryCode(country_code));
  }

 private:
  base::test::ScopedFeatureList feature_list;
};

TEST_F(ShowSigninPromoTestExplicitBrowserSignin, ShowPromoWithNoAccount) {
  EXPECT_TRUE(ShouldShowPasswordSignInPromo(*profile()));
}

TEST_F(ShowSigninPromoTestExplicitBrowserSignin,
       ShowPromoWithWebSignedInAccount) {
  MakeAccountAvailable(identity_manager(), "test@email.com");
  EXPECT_TRUE(ShouldShowPasswordSignInPromo(*profile()));
}

TEST_F(ShowSigninPromoTestExplicitBrowserSignin,
       ShowPromoWithSignInPausedAccount) {
  AccountInfo info = MakePrimaryAccountAvailable(
      identity_manager(), "test@email.com", ConsentLevel::kSignin);
  UpdatePersistentErrorOfRefreshTokenForAccount(
      identity_manager(), info.account_id,
      GoogleServiceAuthError(
          GoogleServiceAuthError::State::USER_NOT_SIGNED_UP));
  EXPECT_TRUE(ShouldShowPasswordSignInPromo(*profile()));
}

TEST_F(ShowSigninPromoTestExplicitBrowserSignin,
       DoNotShowPromoWithAlreadySignedInAccount) {
  MakePrimaryAccountAvailable(identity_manager(), "test@email.com",
                              ConsentLevel::kSignin);
  EXPECT_FALSE(ShouldShowPasswordSignInPromo(*profile()));
}

TEST_F(ShowSigninPromoTestExplicitBrowserSignin,
       DoNotShowPromoWithAlreadySyncingAccount) {
  MakePrimaryAccountAvailable(identity_manager(), "test@email.com",
                              ConsentLevel::kSync);
  EXPECT_FALSE(ShouldShowPasswordSignInPromo(*profile()));
}

TEST_F(ShowSigninPromoTestExplicitBrowserSignin,
       DoNotShowPromoWithOffTheRecordProfile) {
  EXPECT_FALSE(ShouldShowPasswordSignInPromo(
      *profile()->GetPrimaryOTRProfile(/*create_if_needed=*/true)));
}

// TODO (crbug.com/319411636): Add the same test for addresses.
TEST_F(ShowSigninPromoTestExplicitBrowserSignin,
       DoNotShowPromoAfterFiveTimesShown) {
  EXPECT_TRUE(ShouldShowPasswordSignInPromo(*profile()));

  profile()->GetPrefs()->SetInteger(
      prefs::kPasswordSignInPromoShownCountPerProfile, 5);

  EXPECT_FALSE(ShouldShowPasswordSignInPromo(*profile()));
  EXPECT_TRUE(ShouldShowAddressSignInPromo(*profile(), CreateAddress()));
}

TEST_F(ShowSigninPromoTestExplicitBrowserSignin,
       DoNotShowPromoAfterTwoTimesDismissed) {
  EXPECT_TRUE(ShouldShowAddressSignInPromo(*profile(), CreateAddress()));

  profile()->GetPrefs()->SetInteger(
      prefs::kAutofillSignInPromoDismissCountPerProfile, 2);

  EXPECT_FALSE(ShouldShowPasswordSignInPromo(*profile()));
  EXPECT_FALSE(ShouldShowAddressSignInPromo(*profile(), CreateAddress()));
}

TEST_F(ShowSigninPromoTestExplicitBrowserSignin,
       ShowPromoAfterTwoTimesDismissedByDifferentAccounts) {
  profile()->GetPrefs()->SetInteger(
      prefs::kAutofillSignInPromoDismissCountPerProfile, 1);
  SigninPrefs prefs(*profile()->GetPrefs());
  prefs.IncrementAutofillSigninPromoDismissCount("gaia_id");

  EXPECT_TRUE(ShouldShowPasswordSignInPromo(*profile()));
  EXPECT_TRUE(ShouldShowAddressSignInPromo(*profile(), CreateAddress()));
}

TEST_F(ShowSigninPromoTestExplicitBrowserSignin,
       DoNotShowAddressIfProfileMigrationBlocked) {
  autofill::AutofillProfile address = autofill::test::StandardProfile();
  autofill::PersonalDataManagerFactory::GetForBrowserContext(profile())
      ->address_data_manager()
      .AddMaxStrikesToBlockProfileMigration(address.guid());
  EXPECT_FALSE(ShouldShowAddressSignInPromo(*profile(), address));
}

TEST_F(ShowSigninPromoTestExplicitBrowserSignin,
       DoNotShowAddressIfCountryNotEligibleForAccountStorage) {
  const std::string non_eligible_country_code("IR");

  ASSERT_FALSE(
      autofill::PersonalDataManagerFactory::GetForBrowserContext(profile())
          ->address_data_manager()
          .IsCountryEligibleForAccountStorage(non_eligible_country_code));
  EXPECT_FALSE(ShouldShowAddressSignInPromo(
      *profile(), CreateAddress(non_eligible_country_code)));
}

#endif  // BUILDFLAG(ENABLE_DICE_SUPPORT)

}  // namespace signin
