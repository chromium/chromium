// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/signin_promo.h"

#include <memory>

#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/strings/to_string.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/browser/autofill/personal_data_manager_factory.h"
#include "chrome/browser/extensions/sync/extension_sync_util.h"
#include "chrome/browser/profiles/batch_upload/batch_upload_service_factory.h"
#include "chrome/browser/profiles/batch_upload/batch_upload_service_test_helper.h"
#include "chrome/browser/signin/chrome_signin_client_factory.h"
#include "chrome/browser/signin/chrome_signin_client_test_util.h"
#include "chrome/browser/signin/chrome_signin_pref_names.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/signin/identity_test_environment_profile_adaptor.h"
#include "chrome/browser/signin/signin_promo_util.h"
#include "chrome/browser/signin/signin_ui_util.h"
#include "chrome/browser/signin/signin_util.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/testing_browser_process.h"
#include "components/autofill/core/browser/data_manager/personal_data_manager.h"
#include "components/autofill/core/browser/data_model/addresses/autofill_profile.h"
#include "components/autofill/core/browser/test_utils/test_profiles.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/base/signin_metrics.h"
#include "components/signin/public/base/signin_pref_names.h"
#include "components/signin/public/base/signin_prefs.h"
#include "components/signin/public/base/signin_switches.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "components/signin/public/identity_manager/primary_account_mutator.h"
#include "components/sync/base/command_line_switches.h"
#include "components/sync/base/data_type.h"
#include "components/sync/base/features.h"
#include "components/sync/base/pref_names.h"
#include "components/sync/base/user_selectable_type.h"
#include "components/sync/test/mock_sync_service.h"
#include "components/sync/test/test_sync_service.h"
#include "components/sync_bookmarks/switches.h"
#include "content/public/test/browser_task_environment.h"
#include "extensions/common/extension_builder.h"
#include "google_apis/gaia/gaia_id.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace signin {

#if !BUILDFLAG(IS_CHROMEOS)
TEST(SigninPromoTest, TestPromoURL) {
  GURL::Replacements replace_query;
  replace_query.SetQueryStr("access_point=0&reason=0&auto_close=1");
  EXPECT_EQ(
      GURL(chrome::kChromeUIChromeSigninURL).ReplaceComponents(replace_query),
      GetEmbeddedPromoURL(signin_metrics::AccessPoint::kStartPage,
                          signin_metrics::Reason::kSigninPrimaryAccount, true));
  replace_query.SetQueryStr("access_point=15&reason=1");
  EXPECT_EQ(
      GURL(chrome::kChromeUIChromeSigninURL).ReplaceComponents(replace_query),
      GetEmbeddedPromoURL(signin_metrics::AccessPoint::kFullscreenSigninPromo,
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
      GetEmbeddedReauthURLWithEmail(signin_metrics::AccessPoint::kStartPage,
                                    signin_metrics::Reason::kFetchLstOnly,
                                    "example@domain.com"));
}
#endif  // !BUILDFLAG(IS_CHROMEOS)

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
// This test can be deleted once kReplaceSyncPromosWithSignInPromos is launched.
// The behavior with the feature enabled is tested in
// SigninURLForDiceWithHistorySyncOptin.
TEST(SigninPromoTest, SigninURLForDice) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(
      syncer::kReplaceSyncPromosWithSignInPromos);

  EXPECT_EQ(
      "https://accounts.google.com/signin/chrome/sync?ssp=1&"
      "color_scheme=dark&flow=promo&theme=mn",
      GetChromeSyncURLForDice(
          {.request_dark_scheme = true, .flow = Flow::PROMO}));
  EXPECT_EQ(
      "https://accounts.google.com/signin/chrome/sync?ssp=1&"
      "email_hint=email%40gmail.com&continue=https%3A%2F%2Fcontinue_url%2F&"
      "theme=mn",
      GetChromeSyncURLForDice(
          {"email@gmail.com", GURL("https://continue_url/")}));
  EXPECT_EQ(
      "https://accounts.google.com/signin/chrome/"
      "sync?ssp=1&flow=embedded_promo&theme=mn",
      GetChromeSyncURLForDice({.flow = Flow::EMBEDDED_PROMO}));
  EXPECT_EQ(
      "https://accounts.google.com/AddSession?"
      "Email=email%40gmail.com&continue=https%3A%2F%2Fcontinue_url%2F",
      GetAddAccountURLForDice("email@gmail.com",
                              GURL("https://continue_url/")));
}

TEST(SigninPromoTest, SigninURLForDiceWithHistorySyncOptin) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{syncer::kReplaceSyncPromosWithSignInPromos},
      /*disabled_features=*/{});
  EXPECT_EQ(
      "https://accounts.google.com/signin/chrome/sync?ssp=1&"
      "color_scheme=dark&flow=promo&theme=mn",
      GetChromeSyncURLForDice(
          {.request_dark_scheme = true, .flow = Flow::PROMO}));
  EXPECT_EQ(
      "https://accounts.google.com/signin/chrome/"
      "sync?ssp=1&email_hint=email%40gmail.com&continue=https%3A%2F%2Fcontinue_"
      "url%2F&flow=history_opt_in&theme=mn",
      GetChromeSyncURLForDice(
          {"email@gmail.com", GURL("https://continue_url/")}));
  EXPECT_EQ(
      "https://accounts.google.com/signin/chrome/"
      "sync?ssp=1&flow=embedded_promo&theme=mn",
      GetChromeSyncURLForDice({.flow = Flow::EMBEDDED_PROMO}));
  EXPECT_EQ(
      "https://accounts.google.com/AddSession?"
      "Email=email%40gmail.com&continue=https%3A%2F%2Fcontinue_url%2F",
      GetAddAccountURLForDice("email@gmail.com",
                              GURL("https://continue_url/")));
}

TEST(SigninPromoTest, SigninURLForDiceMagiChromeExperiments) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      switches::kMagiChromeSignInExperimentsBatch1,
      {{"magichrome_fre_exp_branch", "test_branch"}});

  EXPECT_EQ(
      "https://accounts.google.com/signin/chrome/sync?ssp=1&"
      "theme=mn&magichrome_fre_exp_branch=test_branch",
      GetChromeSyncURLForDice({}));
}

TEST(SigninPromoTest, IsSignInPromo_AutofillTypes) {
  EXPECT_TRUE(IsSignInPromo(signin_metrics::AccessPoint::kPasswordBubble));
  EXPECT_TRUE(IsSignInPromo(signin_metrics::AccessPoint::kAddressBubble));
}
#endif  // BUILDFLAG(ENABLE_DICE_SUPPORT)

// ChromeOS currently does not show any sign in promos.
#if BUILDFLAG(IS_CHROMEOS)
TEST(SigninPromoTest, IsSignInPromo) {
  EXPECT_FALSE(IsSignInPromo(signin_metrics::AccessPoint::kPasswordBubble));
  EXPECT_FALSE(IsSignInPromo(signin_metrics::AccessPoint::kAddressBubble));
  EXPECT_FALSE(IsSignInPromo(signin_metrics::AccessPoint::kBookmarkBubble));
  EXPECT_FALSE(
      IsSignInPromo(signin_metrics::AccessPoint::kExtensionInstallBubble));
}
#endif  // !BUILDFLAG(IS_CHROMEOS)

// Extensions explicit signin is not enabled in ChromeOS.
#if !BUILDFLAG(IS_CHROMEOS)
TEST(SigninPromoTest, IsSignInPromo_ExtensionsWithExplicitSignin) {
  EXPECT_TRUE(
      IsSignInPromo(signin_metrics::AccessPoint::kExtensionInstallBubble));
}
#endif  // !BUILDFLAG(IS_CHROMEOS)

class ShowPromoTest : public testing::Test {
 public:
  ShowPromoTest() {
    TestingProfile::Builder profile_builder;
    profile_builder.AddTestingFactory(
        SyncServiceFactory::GetInstance(),
        base::BindRepeating([](content::BrowserContext* context) {
          return static_cast<std::unique_ptr<KeyedService>>(
              std::make_unique<syncer::MockSyncService>());
        }));
    profile_ = IdentityTestEnvironmentProfileAdaptor::
        CreateProfileForIdentityTestEnvironment(profile_builder);

    identity_test_env_adaptor_ =
        std::make_unique<IdentityTestEnvironmentProfileAdaptor>(profile_.get());
  }

  void SetUp() override {
    ON_CALL(*sync_service(), GetDataTypesForTransportOnlyMode())
        .WillByDefault(testing::Return(syncer::DataTypeSet::All()));
  }

  syncer::MockSyncService* sync_service() {
    return static_cast<syncer::MockSyncService*>(
        SyncServiceFactory::GetForProfile(profile()));
  }

  IdentityManager* identity_manager() {
    return identity_test_env_adaptor_->identity_test_env()->identity_manager();
  }

  TestingProfile* profile() { return profile_.get(); }

  const extensions::Extension* CreateExtension(
      extensions::mojom::ManifestLocation location =
          extensions::mojom::ManifestLocation::kInternal) {
    extension_ = extensions::ExtensionBuilder()
                     .SetManifest(base::DictValue()
                                      .Set("name", "test")
                                      .Set("manifest_version", 2)
                                      .Set("version", "1.0.0"))
                     .SetLocation(location)
                     .Build();

    return extension_.get();
  }

 protected:
  void DisableSync() {
    ON_CALL(*sync_service(), GetDisableReasons())
        .WillByDefault(testing::Return(syncer::SyncService::DisableReasonSet(
            {syncer::SyncService::DISABLE_REASON_ENTERPRISE_POLICY})));
  }

  autofill::AutofillProfile CreateAddress(
      const std::string& country_code = "US") {
    return autofill::test::StandardProfile(
        autofill::AddressCountryCode(country_code));
  }

 private:
  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<TestingProfile> profile_;
  std::unique_ptr<IdentityTestEnvironmentProfileAdaptor>
      identity_test_env_adaptor_;
  scoped_refptr<const extensions::Extension> extension_;
};

#if !BUILDFLAG(IS_ANDROID)
TEST_F(ShowPromoTest, ShouldShowSigninPromoSyncDisabled) {
  DisableSync();
  EXPECT_FALSE(ShouldShowPasswordSignInPromo(*profile()));
  EXPECT_FALSE(ShouldShowAddressSignInPromo(*profile(), CreateAddress()));
  EXPECT_FALSE(ShouldShowBookmarkSignInPromo(*profile()));
  EXPECT_FALSE(ShouldShowExtensionSignInPromo(*profile(), *CreateExtension()));
}

TEST_F(ShowPromoTest, ShouldShowSigninPromoSyncEnabled) {
#if BUILDFLAG(IS_CHROMEOS)
  // No signin promos on Ash.
  EXPECT_FALSE(ShouldShowPasswordSignInPromo(*profile()));
  EXPECT_FALSE(ShouldShowAddressSignInPromo(*profile(), CreateAddress()));
  EXPECT_FALSE(ShouldShowBookmarkSignInPromo(*profile()));
  EXPECT_FALSE(ShouldShowExtensionSignInPromo(*profile(), *CreateExtension()));
#else
  EXPECT_TRUE(ShouldShowPasswordSignInPromo(*profile()));
  EXPECT_TRUE(ShouldShowAddressSignInPromo(*profile(), CreateAddress()));
  EXPECT_TRUE(ShouldShowBookmarkSignInPromo(*profile()));
  EXPECT_TRUE(ShouldShowExtensionSignInPromo(*profile(), *CreateExtension()));
#endif
}
#endif  // !BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
class ShowSigninPromoTestWithFeatureFlags : public ShowPromoTest {
 public:
  void SetUp() override {
    ShowPromoTest::SetUp();
    feature_list_.InitWithFeatures(
        /*enabled_features=*/
        {syncer::kReplaceSyncPromosWithSignInPromos,
         syncer::kUnoPhase2FollowUp},
        /*disabled_features=*/{});
  }

  GaiaId gaia_id() {
    return identity_manager()
        ->GetPrimaryAccountInfo(ConsentLevel::kSignin)
        .gaia;
  }

 protected:
  base::test::ScopedFeatureList feature_list_;
};

TEST_F(ShowSigninPromoTestWithFeatureFlags, ShowPromoWithNoAccount) {
  EXPECT_TRUE(ShouldShowPasswordSignInPromo(*profile()));
}

TEST_F(ShowSigninPromoTestWithFeatureFlags, ShowPromoWithWebSignedInAccount) {
  MakeAccountAvailable(identity_manager(), "test@email.com");
  EXPECT_TRUE(ShouldShowPasswordSignInPromo(*profile()));
}

TEST_F(ShowSigninPromoTestWithFeatureFlags, ShowPromoWithSignInPendingAccount) {
  AccountInfo info = MakePrimaryAccountAvailable(
      identity_manager(), "test@email.com", ConsentLevel::kSignin);
  signin::SetInvalidRefreshTokenForPrimaryAccount(identity_manager());
  EXPECT_TRUE(ShouldShowPasswordSignInPromo(*profile()));
}

TEST_F(ShowSigninPromoTestWithFeatureFlags,
       DoNotShowPromoWithAlreadySignedInAccount) {
  MakePrimaryAccountAvailable(identity_manager(), "test@email.com",
                              ConsentLevel::kSignin);
  EXPECT_FALSE(ShouldShowPasswordSignInPromo(*profile()));
}

TEST_F(ShowSigninPromoTestWithFeatureFlags,
       DoNotShowPromoWithAlreadySyncingAccount) {
  MakePrimaryAccountAvailable(identity_manager(), "test@email.com",
                              ConsentLevel::kSync);
  EXPECT_FALSE(ShouldShowPasswordSignInPromo(*profile()));
}

TEST_F(ShowSigninPromoTestWithFeatureFlags, DoNotShowPromoWithNoSyncService) {
  TestingProfile::Builder profile_builder;
  profile_builder.AddTestingFactory(
      SyncServiceFactory::GetInstance(),
      base::BindRepeating([](content::BrowserContext* context) {
        return static_cast<std::unique_ptr<KeyedService>>(nullptr);
      }));

  std::unique_ptr<TestingProfile> profile =
      IdentityTestEnvironmentProfileAdaptor::
          CreateProfileForIdentityTestEnvironment(profile_builder);

  ASSERT_EQ(nullptr, SyncServiceFactory::GetForProfile(profile.get()));
  EXPECT_FALSE(ShouldShowPasswordSignInPromo(*profile));
}

TEST_F(ShowSigninPromoTestWithFeatureFlags,
       DoNotShowPromoWithOffTheRecordProfile) {
  EXPECT_FALSE(ShouldShowPasswordSignInPromo(
      *profile()->GetPrimaryOTRProfile(/*create_if_needed=*/true)));
}

TEST_F(ShowSigninPromoTestWithFeatureFlags,
       DoNotShowPromoWithLocalSyncEnabled) {
  ASSERT_TRUE(ShouldShowPasswordSignInPromo(*profile()));

  profile()->GetPrefs()->SetBoolean(syncer::prefs::kEnableLocalSyncBackend,
                                    true);

  EXPECT_FALSE(ShouldShowPasswordSignInPromo(*profile()));
}

TEST_F(ShowSigninPromoTestWithFeatureFlags, DoNotShowPromoWithoutSyncAllowed) {
  ASSERT_TRUE(ShouldShowPasswordSignInPromo(*profile()));

  DisableSync();

  EXPECT_FALSE(ShouldShowPasswordSignInPromo(*profile()));
}

TEST_F(ShowSigninPromoTestWithFeatureFlags,
       DoNotShowPromoWithTypeManagedByPolicy) {
  ASSERT_TRUE(ShouldShowPasswordSignInPromo(*profile()));

  ON_CALL(*sync_service()->GetMockUserSettings(),
          IsTypeManagedByPolicy(syncer::UserSelectableType::kPasswords))
      .WillByDefault(testing::Return(true));

  EXPECT_FALSE(ShouldShowPasswordSignInPromo(*profile()));
}

TEST_F(ShowSigninPromoTestWithFeatureFlags,
       DoNotShowPromoWithoutTransportOnlyDataType) {
  ASSERT_TRUE(ShouldShowPasswordSignInPromo(*profile()));

  ON_CALL(*sync_service(), GetDataTypesForTransportOnlyMode())
      .WillByDefault(testing::Return(syncer::DataTypeSet()));

  EXPECT_FALSE(ShouldShowPasswordSignInPromo(*profile()));
}

TEST_F(ShowSigninPromoTestWithFeatureFlags, ShowExtensionsPromoWithNoAccount) {
  EXPECT_TRUE(ShouldShowExtensionSignInPromo(*profile(), *CreateExtension()));
}

TEST_F(ShowSigninPromoTestWithFeatureFlags,
       ShowExtensionsPromoWithSignInPendingAccount) {
  MakePrimaryAccountAvailable(identity_manager(), "test@email.com",
                              ConsentLevel::kSignin);
  signin::SetInvalidRefreshTokenForPrimaryAccount(identity_manager());

  EXPECT_TRUE(ShouldShowExtensionSignInPromo(*profile(), *CreateExtension()));
}

TEST_F(ShowSigninPromoTestWithFeatureFlags,
       DoNotShowExtensionPromoWithUnpackedExtension) {
  const extensions::Extension* unpacked_extension =
      CreateExtension(extensions::mojom::ManifestLocation::kUnpacked);

  // Unpacked extensions cannot be synced so the sign in promo is not shown.
  ASSERT_TRUE(unpacked_extension);
  ASSERT_FALSE(
      extensions::sync_util::ShouldSync(profile(), unpacked_extension));
  EXPECT_FALSE(ShouldShowExtensionSignInPromo(*profile(), *unpacked_extension));
}

TEST_F(ShowSigninPromoTestWithFeatureFlags,
       DoNotShowPasswordPromoAfterFiveTimesShown) {
  ASSERT_TRUE(ShouldShowPasswordSignInPromo(*profile()));

  profile()->GetPrefs()->SetInteger(
      prefs::kPasswordSignInPromoShownCountPerProfile, 5);

  EXPECT_FALSE(ShouldShowPasswordSignInPromo(*profile()));
  EXPECT_TRUE(ShouldShowAddressSignInPromo(*profile(), CreateAddress()));
}

TEST_F(ShowSigninPromoTestWithFeatureFlags,
       DoNotShowAddressPromoAfterFiveTimesShown) {
  ASSERT_TRUE(ShouldShowAddressSignInPromo(*profile(), CreateAddress()));

  profile()->GetPrefs()->SetInteger(
      prefs::kAddressSignInPromoShownCountPerProfile, 5);

  EXPECT_FALSE(ShouldShowAddressSignInPromo(*profile(), CreateAddress()));
  EXPECT_TRUE(ShouldShowPasswordSignInPromo(*profile()));
}

TEST_F(ShowSigninPromoTestWithFeatureFlags,
       DoNotShowBookmarkPromoAfterFiveTimesShown) {
  ASSERT_TRUE(ShouldShowAddressSignInPromo(*profile(), CreateAddress()));

  profile()->GetPrefs()->SetInteger(
      prefs::kBookmarkSignInPromoShownCountPerProfile, 5);

  EXPECT_FALSE(ShouldShowBookmarkSignInPromo(*profile()));
  EXPECT_TRUE(ShouldShowPasswordSignInPromo(*profile()));
}

TEST_F(ShowSigninPromoTestWithFeatureFlags,
       DoNotShowSearchAIModePromoAfterFiveTimesShown) {
  ASSERT_TRUE(ShouldShowSearchAIModeSignInPromo(*profile()));

  profile()->GetPrefs()->SetInteger(
      prefs::kSearchAIModeSignInPromoShownCountPerProfile, 5);

  EXPECT_FALSE(ShouldShowSearchAIModeSignInPromo(*profile()));
  EXPECT_TRUE(ShouldShowPasswordSignInPromo(*profile()));
}

TEST_F(ShowSigninPromoTestWithFeatureFlags,
       DoNotShowPromoAfterTwoTimesDismissed) {
  ASSERT_TRUE(ShouldShowAddressSignInPromo(*profile(), CreateAddress()));

  profile()->GetPrefs()->SetInteger(
      prefs::kAutofillSignInPromoDismissCountPerProfile, 2);

  EXPECT_FALSE(ShouldShowPasswordSignInPromo(*profile()));
  EXPECT_FALSE(ShouldShowAddressSignInPromo(*profile(), CreateAddress()));
  EXPECT_FALSE(ShouldShowBookmarkSignInPromo(*profile()));
}

TEST_F(ShowSigninPromoTestWithFeatureFlags,
       DoNotShowSearchAIModePromoAfterTwoTimesDismissed) {
  ASSERT_TRUE(ShouldShowSearchAIModeSignInPromo(*profile()));

  profile()->GetPrefs()->SetInteger(
      prefs::kSearchAIModeSignInPromoDismissCountPerProfile, 2);

  EXPECT_FALSE(ShouldShowSearchAIModeSignInPromo(*profile()));
  // Other promos are not affected by Search AI Mode dismissal as they use
  // kAutofillSignInPromoDismissCountPerProfile.
  EXPECT_TRUE(ShouldShowPasswordSignInPromo(*profile()));
}

TEST_F(ShowSigninPromoTestWithFeatureFlags,
       DoNotShowSearchAIModePromoShownTooRecently) {
  ASSERT_TRUE(ShouldShowSearchAIModeSignInPromo(*profile()));

  profile()->GetPrefs()->SetTime(
      prefs::kSearchAIModeSignInPromoLastImpressionTimestampPerProfile,
      base::Time::Now());

  EXPECT_FALSE(ShouldShowSearchAIModeSignInPromo(*profile()));
}

TEST_F(ShowSigninPromoTestWithFeatureFlags, ShowSearchAIModePromoAfterTimeGap) {
  // Start without any impressions.
  ASSERT_TRUE(ShouldShowSearchAIModeSignInPromo(*profile()));
  // There should be a 14-day gap between impressions.
  profile()->GetPrefs()->SetTime(
      prefs::kSearchAIModeSignInPromoLastImpressionTimestampPerProfile,
      base::Time::Now() - base::Days(14) - base::Minutes(1));

  EXPECT_TRUE(ShouldShowSearchAIModeSignInPromo(*profile()));
}

TEST_F(ShowSigninPromoTestWithFeatureFlags,
       ShowSearchAIModePromoOnFirstAttempt) {
  // A clean profile with no recorded impressions should show the promo.
  EXPECT_TRUE(ShouldShowSearchAIModeSignInPromo(*profile()));
}

TEST_F(ShowSigninPromoTestWithFeatureFlags,
       ShowPromoAfterTwoTimesDismissedByDifferentAccounts) {
  profile()->GetPrefs()->SetInteger(
      prefs::kAutofillSignInPromoDismissCountPerProfile, 1);
  SigninPrefs prefs(*profile()->GetPrefs());
  prefs.IncrementAutofillSigninPromoDismissCount(GaiaId("gaia_id"));

  EXPECT_TRUE(ShouldShowPasswordSignInPromo(*profile()));
  EXPECT_TRUE(ShouldShowAddressSignInPromo(*profile(), CreateAddress()));
}

TEST_F(ShowSigninPromoTestWithFeatureFlags,
       DoNotShowAddressIfProfileMigrationBlocked) {
  autofill::AutofillProfile address = autofill::test::StandardProfile();
  autofill::PersonalDataManagerFactory::GetForBrowserContext(profile())
      ->address_data_manager()
      .AddMaxStrikesToBlockProfileMigration(address.guid());
  EXPECT_FALSE(ShouldShowAddressSignInPromo(*profile(), address));
}

TEST_F(ShowSigninPromoTestWithFeatureFlags,
       ShowBookmarkPromoInSignInPendingState) {
  MakePrimaryAccountAvailable(identity_manager(), "test@email.com",
                              ConsentLevel::kSignin);
  signin::SetInvalidRefreshTokenForPrimaryAccount(identity_manager());

  // Promo is showing in sign in pending with account storage enabled.
  ON_CALL(*sync_service()->GetMockUserSettings(), GetSelectedTypes())
      .WillByDefault(testing::Return(syncer::UserSelectableTypeSet(
          {syncer::UserSelectableType::kBookmarks})));
  EXPECT_TRUE(ShouldShowBookmarkSignInPromo(*profile()));

  // Promo is showing in sign in pending with account storage disabled.
  ON_CALL(*sync_service()->GetMockUserSettings(), GetSelectedTypes())
      .WillByDefault(testing::Return(syncer::UserSelectableTypeSet()));
  EXPECT_TRUE(ShouldShowBookmarkSignInPromo(*profile()));

  // Promo is showing when not in sign in pending with account storage disabled.
  ClearPrimaryAccount(identity_manager());
  EXPECT_TRUE(ShouldShowBookmarkSignInPromo(*profile()));
}

TEST_F(ShowSigninPromoTestWithFeatureFlags,
       RecordSignInPromoShownWithoutAccount) {
  // Add an account without cookies. The per-profile pref will be recorded.
  AccountInfo account =
      MakeAccountAvailable(identity_manager(), "test@email.com");

  RecordSignInPromoShown(signin_metrics::AccessPoint::kPasswordBubble,
                         profile());
  RecordSignInPromoShown(signin_metrics::AccessPoint::kAddressBubble,
                         profile());
  RecordSignInPromoShown(signin_metrics::AccessPoint::kBookmarkBubble,
                         profile());

  EXPECT_EQ(1, profile()->GetPrefs()->GetInteger(
                   prefs::kPasswordSignInPromoShownCountPerProfile));
  EXPECT_EQ(1, profile()->GetPrefs()->GetInteger(
                   prefs::kAddressSignInPromoShownCountPerProfile));
  EXPECT_EQ(1, profile()->GetPrefs()->GetInteger(
                   prefs::kBookmarkSignInPromoShownCountPerProfile));
  EXPECT_EQ(0, SigninPrefs(*profile()->GetPrefs())
                   .GetPasswordSigninPromoImpressionCount(account.gaia));
  EXPECT_EQ(0, SigninPrefs(*profile()->GetPrefs())
                   .GetAddressSigninPromoImpressionCount(account.gaia));
  EXPECT_EQ(0, SigninPrefs(*profile()->GetPrefs())
                   .GetBookmarkSigninPromoImpressionCount(account.gaia));

  EXPECT_TRUE(ShouldShowPasswordSignInPromo(*profile()));
  EXPECT_TRUE(ShouldShowAddressSignInPromo(*profile(), CreateAddress()));
  EXPECT_TRUE(ShouldShowBookmarkSignInPromo(*profile()));
}

TEST_F(ShowSigninPromoTestWithFeatureFlags,
       RecordSearchAIModeSignInPromoShownWithoutAccount) {
  RecordSignInPromoShown(signin_metrics::AccessPoint::kSearchAIModeBubble,
                         profile());

  EXPECT_EQ(1, profile()->GetPrefs()->GetInteger(
                   prefs::kSearchAIModeSignInPromoShownCountPerProfile));
  EXPECT_FALSE(
      profile()
          ->GetPrefs()
          ->GetTime(
              prefs::kSearchAIModeSignInPromoLastImpressionTimestampPerProfile)
          .is_null());

  EXPECT_FALSE(ShouldShowSearchAIModeSignInPromo(*profile()));
}

TEST_F(ShowSigninPromoTestWithFeatureFlags,
       RecordSignInPromoShownWithoutAccount_PromoShouldShowForDifferentType) {
  // Add an account without cookies. The per-profile pref will be recorded.
  AccountInfo account =
      MakeAccountAvailable(identity_manager(), "test@email.com");

  // Show the password promo five times. This does not influence whether the
  // address promo should be shown.
  for (int i = 0; i < 5; i++) {
    RecordSignInPromoShown(signin_metrics::AccessPoint::kPasswordBubble,
                           profile());
  }

  EXPECT_EQ(5, profile()->GetPrefs()->GetInteger(
                   prefs::kPasswordSignInPromoShownCountPerProfile));
  EXPECT_EQ(0, SigninPrefs(*profile()->GetPrefs())
                   .GetPasswordSigninPromoImpressionCount(account.gaia));

  EXPECT_FALSE(ShouldShowPasswordSignInPromo(*profile()));
  EXPECT_TRUE(ShouldShowAddressSignInPromo(*profile(), CreateAddress()));
  EXPECT_TRUE(ShouldShowBookmarkSignInPromo(*profile()));
}

TEST_F(ShowSigninPromoTestWithFeatureFlags, RecordSignInPromoShownWithAccount) {
  // Test setup for adding an account with cookies.
  network::TestURLLoaderFactory url_loader_factory =
      network::TestURLLoaderFactory();

  TestingProfile::Builder builder;
  builder.AddTestingFactories(
      IdentityTestEnvironmentProfileAdaptor::
          GetIdentityTestEnvironmentFactoriesWithAppendedFactories(
              {TestingProfile::TestingFactory{
                  ChromeSigninClientFactory::GetInstance(),
                  base::BindRepeating(&BuildChromeSigninClientWithURLLoader,
                                      &url_loader_factory)}}));

  std::unique_ptr<TestingProfile> profile = builder.Build();
  auto identity_test_env_adaptor =
      std::make_unique<IdentityTestEnvironmentProfileAdaptor>(profile.get());
  auto* identity_test_env = identity_test_env_adaptor->identity_test_env();
  identity_test_env->SetTestURLLoaderFactory(&url_loader_factory);

  // Add an account with cookies, which will record the per-account prefs.
  AccountInfo account = identity_test_env->MakeAccountAvailable(
      identity_test_env->CreateAccountAvailabilityOptionsBuilder()
          .WithCookie(true)
          .Build("test@email.com"));

  RecordSignInPromoShown(signin_metrics::AccessPoint::kPasswordBubble,
                         profile.get());
  RecordSignInPromoShown(signin_metrics::AccessPoint::kAddressBubble,
                         profile.get());
  RecordSignInPromoShown(signin_metrics::AccessPoint::kBookmarkBubble,
                         profile.get());

  EXPECT_EQ(0, profile.get()->GetPrefs()->GetInteger(
                   prefs::kPasswordSignInPromoShownCountPerProfile));
  EXPECT_EQ(0, profile.get()->GetPrefs()->GetInteger(
                   prefs::kAddressSignInPromoShownCountPerProfile));
  EXPECT_EQ(0, profile.get()->GetPrefs()->GetInteger(
                   prefs::kBookmarkSignInPromoShownCountPerProfile));
  EXPECT_EQ(1, SigninPrefs(*profile.get()->GetPrefs())
                   .GetPasswordSigninPromoImpressionCount(account.gaia));
  EXPECT_EQ(1, SigninPrefs(*profile.get()->GetPrefs())
                   .GetAddressSigninPromoImpressionCount(account.gaia));
  EXPECT_EQ(1, SigninPrefs(*profile.get()->GetPrefs())
                   .GetBookmarkSigninPromoImpressionCount(account.gaia));
}

TEST_F(ShowSigninPromoTestWithFeatureFlags,
       RecordSignInPromoShownWithAccount_PromoShouldShowForDifferentType) {
  // Test setup for adding an account with cookies.
  network::TestURLLoaderFactory url_loader_factory =
      network::TestURLLoaderFactory();

  TestingProfile::Builder builder;
  builder.AddTestingFactories(
      IdentityTestEnvironmentProfileAdaptor::
          GetIdentityTestEnvironmentFactoriesWithAppendedFactories(
              {TestingProfile::TestingFactory{
                   ChromeSigninClientFactory::GetInstance(),
                   base::BindRepeating(&BuildChromeSigninClientWithURLLoader,
                                       &url_loader_factory)},
               TestingProfile::TestingFactory{
                   SyncServiceFactory::GetInstance(),
                   base::BindRepeating([](content::BrowserContext* context) {
                     return static_cast<std::unique_ptr<KeyedService>>(
                         std::make_unique<syncer::MockSyncService>());
                   })}}));

  std::unique_ptr<TestingProfile> profile = builder.Build();
  auto identity_test_env_adaptor =
      std::make_unique<IdentityTestEnvironmentProfileAdaptor>(profile.get());
  auto* identity_test_env = identity_test_env_adaptor->identity_test_env();
  identity_test_env->SetTestURLLoaderFactory(&url_loader_factory);

  ON_CALL(*static_cast<syncer::MockSyncService*>(
              SyncServiceFactory::GetForProfile(profile.get())),
          GetDataTypesForTransportOnlyMode())
      .WillByDefault(testing::Return(syncer::DataTypeSet::All()));

  // Add an account with cookies, which will record the per-account prefs.
  AccountInfo account = identity_test_env->MakeAccountAvailable(
      identity_test_env->CreateAccountAvailabilityOptionsBuilder()
          .WithCookie(true)
          .Build("test@email.com"));

  // Show the address promo five times. This does not influence whether the
  // password promo should be shown.
  for (int i = 0; i < 5; i++) {
    RecordSignInPromoShown(signin_metrics::AccessPoint::kAddressBubble,
                           profile.get());
  }

  EXPECT_EQ(0, profile->GetPrefs()->GetInteger(
                   prefs::kAddressSignInPromoShownCountPerProfile));
  EXPECT_EQ(5, SigninPrefs(*profile.get()->GetPrefs())
                   .GetAddressSigninPromoImpressionCount(account.gaia));

  EXPECT_FALSE(ShouldShowAddressSignInPromo(*profile.get(), CreateAddress()));
  EXPECT_TRUE(ShouldShowPasswordSignInPromo(*profile.get()));
  EXPECT_TRUE(ShouldShowBookmarkSignInPromo(*profile.get()));
}

TEST_F(ShowSigninPromoTestWithFeatureFlags,
       RecordSignInPromoShownWithAccount_BookmarkPromoNotAlwaysShown) {
  // Test setup for adding an account with cookies.
  network::TestURLLoaderFactory url_loader_factory =
      network::TestURLLoaderFactory();

  TestingProfile::Builder builder;
  builder.AddTestingFactories(
      IdentityTestEnvironmentProfileAdaptor::
          GetIdentityTestEnvironmentFactoriesWithAppendedFactories(
              {TestingProfile::TestingFactory{
                   ChromeSigninClientFactory::GetInstance(),
                   base::BindRepeating(&BuildChromeSigninClientWithURLLoader,
                                       &url_loader_factory)},
               TestingProfile::TestingFactory{
                   SyncServiceFactory::GetInstance(),
                   base::BindRepeating([](content::BrowserContext* context) {
                     return static_cast<std::unique_ptr<KeyedService>>(
                         std::make_unique<syncer::MockSyncService>());
                   })}}));

  std::unique_ptr<TestingProfile> profile = builder.Build();
  auto identity_test_env_adaptor =
      std::make_unique<IdentityTestEnvironmentProfileAdaptor>(profile.get());
  auto* identity_test_env = identity_test_env_adaptor->identity_test_env();
  identity_test_env->SetTestURLLoaderFactory(&url_loader_factory);

  ON_CALL(*static_cast<syncer::MockSyncService*>(
              SyncServiceFactory::GetForProfile(profile.get())),
          GetDataTypesForTransportOnlyMode())
      .WillByDefault(testing::Return(syncer::DataTypeSet::All()));

  // Add an account with cookies, which will record the per-account prefs.
  identity_test_env->MakeAccountAvailable(
      identity_test_env->CreateAccountAvailabilityOptionsBuilder()
          .WithCookie(true)
          .Build("test@email.com"));
  ASSERT_TRUE(ShouldShowBookmarkSignInPromo(*profile.get()));

  // Show the bookmark promo five times. After this, the bookmark promo will not
  // be shown again.
  for (int i = 0; i < 5; i++) {
    RecordSignInPromoShown(signin_metrics::AccessPoint::kBookmarkBubble,
                           profile.get());
  }

  EXPECT_FALSE(ShouldShowBookmarkSignInPromo(*profile.get()));
}
class ShowSigninPromoTestWithoutPhase2FollowUp
    : public ShowSigninPromoTestWithFeatureFlags {
 public:
  void SetUp() override {
    feature_list_.InitWithFeatures(
        /*enabled_features=*/
        {syncer::kReplaceSyncPromosWithSignInPromos},
        /*disabled_features=*/{syncer::kUnoPhase2FollowUp});
    ON_CALL(*sync_service(), GetDataTypesForTransportOnlyMode())
        .WillByDefault(testing::Return(syncer::DataTypeSet::All()));
  }
};

TEST_F(ShowSigninPromoTestWithoutPhase2FollowUp,
       RecordSignInPromoShownWithoutAccount_BookmarkPromoAlwaysShown) {
  // Add an account without cookies. The per-profile pref will be recorded.
  MakeAccountAvailable(identity_manager(), "test@email.com");
  ASSERT_TRUE(ShouldShowBookmarkSignInPromo(*profile()));

  // Show the bookmark promo five times. This does not influence whether it is
  // shown again or not.
  for (int i = 0; i < 5; i++) {
    RecordSignInPromoShown(signin_metrics::AccessPoint::kBookmarkBubble,
                           profile());
  }

  EXPECT_TRUE(ShouldShowBookmarkSignInPromo(*profile()));
}

TEST_F(ShowSigninPromoTestWithoutPhase2FollowUp,
       RecordSignInPromoShownWithAccount_BookmarkPromoAlwaysShown) {
  // Test setup for adding an account with cookies.
  network::TestURLLoaderFactory url_loader_factory =
      network::TestURLLoaderFactory();

  TestingProfile::Builder builder;
  builder.AddTestingFactories(
      IdentityTestEnvironmentProfileAdaptor::
          GetIdentityTestEnvironmentFactoriesWithAppendedFactories(
              {TestingProfile::TestingFactory{
                   ChromeSigninClientFactory::GetInstance(),
                   base::BindRepeating(&BuildChromeSigninClientWithURLLoader,
                                       &url_loader_factory)},
               TestingProfile::TestingFactory{
                   SyncServiceFactory::GetInstance(),
                   base::BindRepeating([](content::BrowserContext* context) {
                     return static_cast<std::unique_ptr<KeyedService>>(
                         std::make_unique<syncer::MockSyncService>());
                   })}}));

  std::unique_ptr<TestingProfile> profile = builder.Build();
  auto identity_test_env_adaptor =
      std::make_unique<IdentityTestEnvironmentProfileAdaptor>(profile.get());
  auto* identity_test_env = identity_test_env_adaptor->identity_test_env();
  identity_test_env->SetTestURLLoaderFactory(&url_loader_factory);

  ON_CALL(*static_cast<syncer::MockSyncService*>(
              SyncServiceFactory::GetForProfile(profile.get())),
          GetDataTypesForTransportOnlyMode())
      .WillByDefault(testing::Return(syncer::DataTypeSet::All()));

  // Add an account with cookies, which will record the per-account prefs.
  identity_test_env->MakeAccountAvailable(
      identity_test_env->CreateAccountAvailabilityOptionsBuilder()
          .WithCookie(true)
          .Build("test@email.com"));
  ASSERT_TRUE(ShouldShowBookmarkSignInPromo(*profile.get()));

  // Show the bookmark promo five times. This does not influence whether it is
  // shown again or not.
  for (int i = 0; i < 5; i++) {
    RecordSignInPromoShown(signin_metrics::AccessPoint::kBookmarkBubble,
                           profile.get());
  }

  EXPECT_TRUE(ShouldShowBookmarkSignInPromo(*profile.get()));
}

class ShowSigninPromoTestWithFeatureFlagsPromoLimitsExperiment
    : public ShowSigninPromoTestWithFeatureFlags {
 public:
  void SetUp() override {
    ShowSigninPromoTestWithFeatureFlags::SetUp();
    scoped_feature_list_.InitAndEnableFeature(
        switches::kSigninPromoLimitsExperiment);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(ShowSigninPromoTestWithFeatureFlagsPromoLimitsExperiment,
       DoNotShowAddressPromoAfterMaxTimesShown) {
  ASSERT_TRUE(ShouldShowAddressSignInPromo(*profile(), CreateAddress()));

  profile()->GetPrefs()->SetInteger(
      prefs::kAddressSignInPromoShownCountPerProfileForLimitsExperiment,
      switches::kContextualSigninPromoShownThreshold.Get());

  EXPECT_FALSE(ShouldShowAddressSignInPromo(*profile(), CreateAddress()));
  EXPECT_TRUE(ShouldShowPasswordSignInPromo(*profile()));
}

TEST_F(ShowSigninPromoTestWithFeatureFlagsPromoLimitsExperiment,
       DoNotShowPasswordPromoAfterMaxTimesShown) {
  ASSERT_TRUE(ShouldShowPasswordSignInPromo(*profile()));

  profile()->GetPrefs()->SetInteger(
      prefs::kPasswordSignInPromoShownCountPerProfileForLimitsExperiment,
      switches::kContextualSigninPromoShownThreshold.Get());

  EXPECT_FALSE(ShouldShowPasswordSignInPromo(*profile()));
  EXPECT_TRUE(ShouldShowAddressSignInPromo(*profile(), CreateAddress()));
}

TEST_F(ShowSigninPromoTestWithFeatureFlagsPromoLimitsExperiment,
       DoNotShowBookmarkPromoAfterMaxTimesShown) {
  ASSERT_TRUE(ShouldShowBookmarkSignInPromo(*profile()));

  profile()->GetPrefs()->SetInteger(
      prefs::kBookmarkSignInPromoShownCountPerProfileForLimitsExperiment, 20);

  EXPECT_FALSE(ShouldShowBookmarkSignInPromo(*profile()));
  EXPECT_TRUE(ShouldShowAddressSignInPromo(*profile(), CreateAddress()));
}

TEST_F(ShowSigninPromoTestWithFeatureFlagsPromoLimitsExperiment,
       RecordSignInPromoShownWithoutAccount) {
  // Add an account without cookies. The per-profile pref will be recorded.
  AccountInfo account =
      MakeAccountAvailable(identity_manager(), "test@email.com");

  RecordSignInPromoShown(signin_metrics::AccessPoint::kPasswordBubble,
                         profile());
  RecordSignInPromoShown(signin_metrics::AccessPoint::kAddressBubble,
                         profile());
  RecordSignInPromoShown(signin_metrics::AccessPoint::kBookmarkBubble,
                         profile());

  EXPECT_EQ(
      1,
      profile()->GetPrefs()->GetInteger(
          prefs::kPasswordSignInPromoShownCountPerProfileForLimitsExperiment));
  EXPECT_EQ(
      1,
      profile()->GetPrefs()->GetInteger(
          prefs::kAddressSignInPromoShownCountPerProfileForLimitsExperiment));
  EXPECT_EQ(
      1,
      profile()->GetPrefs()->GetInteger(
          prefs::kBookmarkSignInPromoShownCountPerProfileForLimitsExperiment));
  EXPECT_EQ(0, SigninPrefs(*profile()->GetPrefs())
                   .GetPasswordSigninPromoImpressionCount(account.gaia));
  EXPECT_EQ(0, SigninPrefs(*profile()->GetPrefs())
                   .GetAddressSigninPromoImpressionCount(account.gaia));
  EXPECT_EQ(0, SigninPrefs(*profile()->GetPrefs())
                   .GetBookmarkSigninPromoImpressionCount(account.gaia));

  EXPECT_TRUE(ShouldShowPasswordSignInPromo(*profile()));
  EXPECT_TRUE(ShouldShowAddressSignInPromo(*profile(), CreateAddress()));
  EXPECT_TRUE(ShouldShowBookmarkSignInPromo(*profile()));
}

TEST_F(ShowSigninPromoTestWithFeatureFlagsPromoLimitsExperiment,
       RecordSignInPromoShownWithAccount) {
  // Test setup for adding an account with cookies.
  network::TestURLLoaderFactory url_loader_factory =
      network::TestURLLoaderFactory();

  TestingProfile::Builder builder;
  builder.AddTestingFactories(
      IdentityTestEnvironmentProfileAdaptor::
          GetIdentityTestEnvironmentFactoriesWithAppendedFactories(
              {TestingProfile::TestingFactory{
                  ChromeSigninClientFactory::GetInstance(),
                  base::BindRepeating(&BuildChromeSigninClientWithURLLoader,
                                      &url_loader_factory)}}));

  std::unique_ptr<TestingProfile> profile = builder.Build();
  auto identity_test_env_adaptor =
      std::make_unique<IdentityTestEnvironmentProfileAdaptor>(profile.get());
  auto* identity_test_env = identity_test_env_adaptor->identity_test_env();
  identity_test_env->SetTestURLLoaderFactory(&url_loader_factory);

  // Add an account with cookies, which will record the per-account prefs.
  AccountInfo account = identity_test_env->MakeAccountAvailable(
      identity_test_env->CreateAccountAvailabilityOptionsBuilder()
          .WithCookie(true)
          .Build("test@email.com"));

  RecordSignInPromoShown(signin_metrics::AccessPoint::kPasswordBubble,
                         profile.get());
  RecordSignInPromoShown(signin_metrics::AccessPoint::kAddressBubble,
                         profile.get());
  RecordSignInPromoShown(signin_metrics::AccessPoint::kBookmarkBubble,
                         profile.get());

  EXPECT_EQ(
      0,
      profile.get()->GetPrefs()->GetInteger(
          prefs::kPasswordSignInPromoShownCountPerProfileForLimitsExperiment));
  EXPECT_EQ(
      0,
      profile.get()->GetPrefs()->GetInteger(
          prefs::kAddressSignInPromoShownCountPerProfileForLimitsExperiment));
  EXPECT_EQ(
      0,
      profile.get()->GetPrefs()->GetInteger(
          prefs::kBookmarkSignInPromoShownCountPerProfileForLimitsExperiment));
  EXPECT_EQ(1, SigninPrefs(*profile.get()->GetPrefs())
                   .GetPasswordSigninPromoImpressionCount(account.gaia));
  EXPECT_EQ(1, SigninPrefs(*profile.get()->GetPrefs())
                   .GetAddressSigninPromoImpressionCount(account.gaia));
  EXPECT_EQ(1, SigninPrefs(*profile.get()->GetPrefs())
                   .GetBookmarkSigninPromoImpressionCount(account.gaia));
}

TEST_F(ShowSigninPromoTestWithFeatureFlagsPromoLimitsExperiment,
       SkipCheckForNonExperimentNumberOfTimesShownForPasswordPromo) {
  ASSERT_TRUE(ShouldShowPasswordSignInPromo(*profile()));

  profile()->GetPrefs()->SetInteger(
      prefs::kPasswordSignInPromoShownCountPerProfile, INT_MAX);

  EXPECT_TRUE(ShouldShowPasswordSignInPromo(*profile()));
}

TEST_F(ShowSigninPromoTestWithFeatureFlagsPromoLimitsExperiment,
       SkipCheckForNonExperimentNumberOfTimesShownForAddressPromo) {
  ASSERT_TRUE(ShouldShowAddressSignInPromo(*profile(), CreateAddress()));

  profile()->GetPrefs()->SetInteger(
      prefs::kAddressSignInPromoShownCountPerProfile, INT_MAX);

  EXPECT_TRUE(ShouldShowAddressSignInPromo(*profile(), CreateAddress()));
}

TEST_F(ShowSigninPromoTestWithFeatureFlagsPromoLimitsExperiment,
       SkipCheckForNonExperimentNumberOfTimesShownForBookmarkPromo) {
  ASSERT_TRUE(ShouldShowBookmarkSignInPromo(*profile()));

  profile()->GetPrefs()->SetInteger(
      prefs::kBookmarkSignInPromoShownCountPerProfile, INT_MAX);

  EXPECT_TRUE(ShouldShowBookmarkSignInPromo(*profile()));
}

TEST_F(ShowSigninPromoTestWithFeatureFlagsPromoLimitsExperiment,
       DoNotShowPasswordPromoAfterMaxTimesDismissed) {
  EXPECT_TRUE(ShouldShowPasswordSignInPromo(*profile()));

  profile()->GetPrefs()->SetInteger(
      prefs::kPasswordSignInPromoDismissCountPerProfileForLimitsExperiment,
      switches::kContextualSigninPromoDismissedThreshold.Get());

  EXPECT_FALSE(ShouldShowPasswordSignInPromo(*profile()));
}

TEST_F(ShowSigninPromoTestWithFeatureFlagsPromoLimitsExperiment,
       DoNotShowAddressPromoAfterMaxTimesDismissed) {
  EXPECT_TRUE(ShouldShowAddressSignInPromo(*profile(), CreateAddress()));

  profile()->GetPrefs()->SetInteger(
      prefs::kAddressSignInPromoDismissCountPerProfileForLimitsExperiment,
      switches::kContextualSigninPromoDismissedThreshold.Get());

  EXPECT_FALSE(ShouldShowAddressSignInPromo(*profile(), CreateAddress()));
}

TEST_F(ShowSigninPromoTestWithFeatureFlagsPromoLimitsExperiment,
       DoNotShowBookmarkPromoAfterMaxTimesDismissed) {
  base::test::ScopedFeatureList scoped_feature_list{syncer::kUnoPhase2FollowUp};

  EXPECT_TRUE(ShouldShowBookmarkSignInPromo(*profile()));

  profile()->GetPrefs()->SetInteger(
      prefs::kBookmarkSignInPromoDismissCountPerProfileForLimitsExperiment,
      switches::kContextualSigninPromoDismissedThreshold.Get());

  EXPECT_FALSE(ShouldShowBookmarkSignInPromo(*profile()));
}

TEST_F(ShowSigninPromoTestWithFeatureFlagsPromoLimitsExperiment,
       SearchAIModePromoIgnoresOtherExperimentThresholds) {
  ASSERT_TRUE(ShouldShowSearchAIModeSignInPromo(*profile()));
  ASSERT_TRUE(ShouldShowBookmarkSignInPromo(*profile()));

  // Set the bookmark promo shown limit to max (6 times).
  profile()->GetPrefs()->SetInteger(
      prefs::kBookmarkSignInPromoShownCountPerProfileForLimitsExperiment, 6);
  EXPECT_FALSE(ShouldShowBookmarkSignInPromo(*profile()));
  EXPECT_TRUE(ShouldShowSearchAIModeSignInPromo(*profile()));

  // Set the bookmark promo impression and dismissal limit to max (2 times).
  profile()->GetPrefs()->SetInteger(
      prefs::kBookmarkSignInPromoShownCountPerProfileForLimitsExperiment, 2);
  profile()->GetPrefs()->SetInteger(
      prefs::kBookmarkSignInPromoDismissCountPerProfileForLimitsExperiment, 2);
  EXPECT_FALSE(ShouldShowBookmarkSignInPromo(*profile()));
  EXPECT_TRUE(ShouldShowSearchAIModeSignInPromo(*profile()));
}

TEST_F(ShowSigninPromoTestWithFeatureFlagsPromoLimitsExperiment,
       SearchAIModePromoLimitsDoNotAffectOtherPromos) {
  ASSERT_TRUE(ShouldShowSearchAIModeSignInPromo(*profile()));
  ASSERT_TRUE(ShouldShowBookmarkSignInPromo(*profile()));

  // Set the Search AIM max impression limit (5 times)
  profile()->GetPrefs()->SetInteger(
      prefs::kSearchAIModeSignInPromoShownCountPerProfile, 5);
  EXPECT_FALSE(ShouldShowSearchAIModeSignInPromo(*profile()));
  EXPECT_TRUE(ShouldShowBookmarkSignInPromo(*profile()));

  // Set the Search AIM max dismissal limit (2 times)
  profile()->GetPrefs()->SetInteger(
      prefs::kSearchAIModeSignInPromoShownCountPerProfile, 2);
  profile()->GetPrefs()->SetInteger(
      prefs::kSearchAIModeSignInPromoDismissCountPerProfile, 2);
  EXPECT_FALSE(ShouldShowSearchAIModeSignInPromo(*profile()));
  EXPECT_TRUE(ShouldShowBookmarkSignInPromo(*profile()));
}

class AvatarButtonPromoManagerTest : public testing::Test {
 public:
  AvatarButtonPromoManagerTest()
      : identity_test_environment_(&test_url_loader_factory_) {
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/{syncer::kReplaceSyncPromosWithSignInPromos,
                              switches::kAvatarButtonSyncPromoForTesting,
                              switches::
                                  kSigninWindows10DepreciationStateForTesting},
        /*disabled_features=*/{});

    AvatarButtonPromoManager::RegisterProfilePrefs(pref_service_.registry());
    SigninPrefs::RegisterProfilePrefs(pref_service_.registry());
  }

  void SetSigninStateFromPromoType(
      ProfileMenuAvatarButtonPromoInfo::Type promo_type) {
    switch (promo_type) {
      case ProfileMenuAvatarButtonPromoInfo::Type::kHistorySyncPromo:
      case ProfileMenuAvatarButtonPromoInfo::Type::kBatchUploadPromo:
      case ProfileMenuAvatarButtonPromoInfo::Type::kBatchUploadBookmarksPromo:
      case ProfileMenuAvatarButtonPromoInfo::Type::
          kBatchUploadWindows10DepreciationPromo:
      case ProfileMenuAvatarButtonPromoInfo::Type::kSyncPromo:
        // Signed in.
        // Only sign in if the profile was not already signed in.
        if (!identity_manager()->HasPrimaryAccount(ConsentLevel::kSignin)) {
          MakePrimaryAccountAvailable(identity_manager(), "test@email.com",
                                      ConsentLevel::kSignin);
        }
        break;
      case ProfileMenuAvatarButtonPromoInfo::Type::kSigninPromo:
        // Signed out.
        // Sign out in case the profile was already signed in.
        if (identity_manager()->HasPrimaryAccount(ConsentLevel::kSignin)) {
          identity_manager()->GetPrimaryAccountMutator()->ClearPrimaryAccount(
              signin_metrics::ProfileSignout::kTest);
        }
        break;
    }
  }

  // Fast forwarding is required for promos that have a shown time check.
  void FastForwardToBypassPromoTypeShownTimeCheck(
      ProfileMenuAvatarButtonPromoInfo::Type promo_type) {
    switch (promo_type) {
      case ProfileMenuAvatarButtonPromoInfo::Type::kHistorySyncPromo:
      case ProfileMenuAvatarButtonPromoInfo::Type::kBatchUploadPromo:
      case ProfileMenuAvatarButtonPromoInfo::Type::kBatchUploadBookmarksPromo:
      case ProfileMenuAvatarButtonPromoInfo::Type::
          kBatchUploadWindows10DepreciationPromo:
      case ProfileMenuAvatarButtonPromoInfo::Type::kSyncPromo:
        break;
      case ProfileMenuAvatarButtonPromoInfo::Type::kSigninPromo:
        task_environment_.FastForwardBy(
            switches::kSigninPromoOnAvatarPillDelayForNextPromoAllowed.Get() +
            base::Days(1));
        break;
    }
  }

  void FastForwardBy(base::TimeDelta time_delta) {
    task_environment_.FastForwardBy(time_delta);
  }

  IdentityManager* identity_manager() {
    return identity_test_environment_.identity_manager();
  }
  PrefService& pref_service() { return pref_service_; }
  network::TestURLLoaderFactory* test_url_loader_factory() {
    return &test_url_loader_factory_;
  }

 private:
  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  network::TestURLLoaderFactory test_url_loader_factory_;
  IdentityTestEnvironment identity_test_environment_;
  TestingPrefServiceSimple pref_service_;

  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(AvatarButtonPromoManagerTest, PromoTypesUseDifferentShownLimits) {
  std::array<ProfileMenuAvatarButtonPromoInfo::Type, 6> promo_type_list{
      ProfileMenuAvatarButtonPromoInfo::Type::kHistorySyncPromo,
      ProfileMenuAvatarButtonPromoInfo::Type::kBatchUploadPromo,
      ProfileMenuAvatarButtonPromoInfo::Type::kBatchUploadBookmarksPromo,
      ProfileMenuAvatarButtonPromoInfo::Type::
          kBatchUploadWindows10DepreciationPromo,
      ProfileMenuAvatarButtonPromoInfo::Type::kSyncPromo,
      ProfileMenuAvatarButtonPromoInfo::Type::kSigninPromo,
  };

  const size_t max_shown_count = 4;
  AvatarButtonPromoManager manager(identity_manager(), &pref_service(),
                                   max_shown_count, /*max_used_count=*/2);

  for (auto promo_type : promo_type_list) {
    SCOPED_TRACE("Iteration: promo_type - " + base::ToString(promo_type));
    SetSigninStateFromPromoType(promo_type);
    for (size_t count = 0; count < max_shown_count; ++count) {
      FastForwardToBypassPromoTypeShownTimeCheck(promo_type);
      ASSERT_TRUE(manager.ShouldShowPromo(promo_type));
      manager.RecordPromoShown(promo_type);
    }
    ASSERT_FALSE(manager.ShouldShowPromo(promo_type));
  }
}

TEST_F(AvatarButtonPromoManagerTest, PromoTypesUseDifferentUsedLimits) {
  std::array<ProfileMenuAvatarButtonPromoInfo::Type, 6> promo_type_list{
      ProfileMenuAvatarButtonPromoInfo::Type::kHistorySyncPromo,
      ProfileMenuAvatarButtonPromoInfo::Type::kBatchUploadPromo,
      ProfileMenuAvatarButtonPromoInfo::Type::kBatchUploadBookmarksPromo,
      ProfileMenuAvatarButtonPromoInfo::Type::
          kBatchUploadWindows10DepreciationPromo,
      ProfileMenuAvatarButtonPromoInfo::Type::kSyncPromo,
      ProfileMenuAvatarButtonPromoInfo::Type::kSigninPromo,
  };

  const size_t max_used_count = 2;
  AvatarButtonPromoManager manager(identity_manager(), &pref_service(),
                                   /*max_shown_count=*/4, max_used_count);

  for (auto promo_type : promo_type_list) {
    SCOPED_TRACE("Iteration: promo_type - " + base::ToString(promo_type));
    SetSigninStateFromPromoType(promo_type);
    for (size_t count = 0; count < max_used_count; ++count) {
      ASSERT_TRUE(manager.ShouldShowPromo(promo_type));
      manager.RecordPromoUsed(promo_type);
    }
    ASSERT_FALSE(manager.ShouldShowPromo(promo_type));
  }
}

TEST_F(AvatarButtonPromoManagerTest,
       SigninPromoSupportIndependantProfileAndAccountShownCount) {
  ProfileMenuAvatarButtonPromoInfo::Type promo_type =
      ProfileMenuAvatarButtonPromoInfo::Type::kSigninPromo;
  const int max_shown_count = 3;
  AvatarButtonPromoManager manager(identity_manager(), &pref_service(),
                                   max_shown_count, /*max_used_count=*/2);
  // Signed out state.
  {
    ASSERT_EQ(signin_util::GetSignedInState(identity_manager()),
              signin_util::SignedInState::kSignedOut);

    // Exhaust max used count for the signed out profile.
    for (int i = 0; i < max_shown_count; ++i) {
      SCOPED_TRACE("Iteration: " + base::ToString(i));
      FastForwardToBypassPromoTypeShownTimeCheck(promo_type);
      // The promo should be shown if the used count is below the max.
      EXPECT_TRUE(manager.ShouldShowPromo(promo_type));
      manager.RecordPromoShown(promo_type);
    }
    EXPECT_FALSE(manager.ShouldShowPromo(promo_type));
  }

  // Web Signed in.
  {
    std::string_view email1("test1@email.com");
    signin::MakeAccountAvailable(
        identity_manager(),
        AccountAvailabilityOptionsBuilder(test_url_loader_factory())
            .WithCookie()
            .Build(email1));
    ASSERT_EQ(signin_util::GetSignedInState(identity_manager()),
              signin_util::SignedInState::kWebOnlySignedIn);

    // Web signed in count should be counted separately, therefore we should be
    // able to reach the maximum in the same manner as the signed out profile.
    for (int i = 0; i < max_shown_count; ++i) {
      SCOPED_TRACE("Iteration: " + base::ToString(i));
      FastForwardToBypassPromoTypeShownTimeCheck(promo_type);
      // The promo should be shown if the used count is below the max.
      EXPECT_TRUE(manager.ShouldShowPromo(promo_type));
      manager.RecordPromoShown(promo_type);
    }
    EXPECT_FALSE(manager.ShouldShowPromo(promo_type));
  }
}

TEST_F(AvatarButtonPromoManagerTest,
       SigninPromoSupportIndependantProfileAndAccountUsedCount) {
  ProfileMenuAvatarButtonPromoInfo::Type promo_type =
      ProfileMenuAvatarButtonPromoInfo::Type::kSigninPromo;
  const int max_used_count = 2;
  AvatarButtonPromoManager manager(identity_manager(), &pref_service(),
                                   /*max_shown_count=*/3, max_used_count);
  // Signed out state.
  {
    ASSERT_EQ(signin_util::GetSignedInState(identity_manager()),
              signin_util::SignedInState::kSignedOut);

    // Exhaust max used count for the signed out profile.
    for (int i = 0; i < max_used_count; ++i) {
      SCOPED_TRACE("Iteration: " + base::ToString(i));
      // The promo should be shown if the used count is below the max.
      EXPECT_TRUE(manager.ShouldShowPromo(promo_type));
      manager.RecordPromoUsed(promo_type);
    }
    EXPECT_FALSE(manager.ShouldShowPromo(promo_type));
  }

  // Web Signed in.
  {
    std::string_view email1("test1@email.com");
    signin::MakeAccountAvailable(
        identity_manager(),
        AccountAvailabilityOptionsBuilder(test_url_loader_factory())
            .WithCookie()
            .Build(email1));
    ASSERT_EQ(signin_util::GetSignedInState(identity_manager()),
              signin_util::SignedInState::kWebOnlySignedIn);

    // Web signed in count should be counted separately, therefore we should be
    // able to reach the maximum in the same manner as the signed out profile.
    for (int i = 0; i < max_used_count; ++i) {
      SCOPED_TRACE("Iteration: " + base::ToString(i));
      // The promo should be shown if the used count is below the max.
      EXPECT_TRUE(manager.ShouldShowPromo(promo_type));
      manager.RecordPromoUsed(promo_type);
    }
    EXPECT_FALSE(manager.ShouldShowPromo(promo_type));
  }
}

TEST_F(AvatarButtonPromoManagerTest,
       SigninPromoHasShownTimeCheckForSignedOutState) {
  ProfileMenuAvatarButtonPromoInfo::Type signin_promo_type =
      ProfileMenuAvatarButtonPromoInfo::Type::kSigninPromo;
  AvatarButtonPromoManager manager(identity_manager(), &pref_service(),
                                   /*max_shown_count=*/3, /*max_used_count=*/2);

  ASSERT_EQ(signin_util::GetSignedInState(identity_manager()),
            signin_util::SignedInState::kSignedOut);

  ASSERT_TRUE(manager.ShouldShowPromo(signin_promo_type));
  manager.RecordPromoShown(signin_promo_type);

  // Promo shown time check does not allow the promo to show yet.
  ASSERT_FALSE(manager.ShouldShowPromo(signin_promo_type));

  // Fast forward by less than expected.
  base::TimeDelta time_remaining_for_promo_to_show = base::Days(1);
  FastForwardBy(
      switches::kSigninPromoOnAvatarPillDelayForNextPromoAllowed.Get() -
      time_remaining_for_promo_to_show);
  // Promo shown time check should still not allow the promo to show yet.
  ASSERT_FALSE(manager.ShouldShowPromo(signin_promo_type));

  // Add enough time to allow promo to show.
  FastForwardBy(2 * time_remaining_for_promo_to_show);

  ASSERT_TRUE(manager.ShouldShowPromo(signin_promo_type));
}

TEST_F(AvatarButtonPromoManagerTest,
       SigninPromoHasShownTimeCheckForWebSigninState) {
  ProfileMenuAvatarButtonPromoInfo::Type signin_promo_type =
      ProfileMenuAvatarButtonPromoInfo::Type::kSigninPromo;
  AvatarButtonPromoManager manager(identity_manager(), &pref_service(),
                                   /*max_shown_count=*/3, /*max_used_count=*/2);

  signin::MakeAccountAvailable(
      identity_manager(),
      AccountAvailabilityOptionsBuilder(test_url_loader_factory())
          .WithCookie()
          .Build("test1@email.com"));
  ASSERT_EQ(signin_util::GetSignedInState(identity_manager()),
            signin_util::SignedInState::kWebOnlySignedIn);

  ASSERT_TRUE(manager.ShouldShowPromo(signin_promo_type));
  manager.RecordPromoShown(signin_promo_type);

  // Promo shown time check does not allow the promo to show yet.
  ASSERT_FALSE(manager.ShouldShowPromo(signin_promo_type));

  // Fast forward by less than expected.
  base::TimeDelta time_remaining_for_promo_to_show = base::Days(1);
  FastForwardBy(
      switches::kSigninPromoOnAvatarPillDelayForNextPromoAllowed.Get() -
      time_remaining_for_promo_to_show);
  // Promo shown time check should still not allow the promo to show yet.
  ASSERT_FALSE(manager.ShouldShowPromo(signin_promo_type));

  // Add enough time to allow promo to show.
  FastForwardBy(2 * time_remaining_for_promo_to_show);

  ASSERT_TRUE(manager.ShouldShowPromo(signin_promo_type));
}

TEST_F(AvatarButtonPromoManagerTest, SigninPromoHasLastExternalEventTimeCheck) {
  ProfileMenuAvatarButtonPromoInfo::Type signin_promo_type =
      ProfileMenuAvatarButtonPromoInfo::Type::kSigninPromo;
  AvatarButtonPromoManager manager(identity_manager(), &pref_service(),
                                   /*max_shown_count=*/3, /*max_used_count=*/2);

  AccountInfo account_info = signin::MakeAccountAvailable(
      identity_manager(),
      AccountAvailabilityOptionsBuilder(test_url_loader_factory())
          .WithCookie()
          .Build("test1@email.com"));
  ASSERT_EQ(signin_util::GetSignedInState(identity_manager()),
            signin_util::SignedInState::kWebOnlySignedIn);

  ASSERT_TRUE(manager.ShouldShowPromo(signin_promo_type));

  // Changes the last external event time.
  SigninPrefs signin_prefs(pref_service());
  signin_prefs.SetChromeSigninInterceptionLastBubbleDeclineTime(
      account_info.GetGaiaId(), base::Time::Now());

  // Last event time check does not allow the promo to show yet.
  ASSERT_FALSE(manager.ShouldShowPromo(signin_promo_type));

  // Fast forward by less than expected.
  base::TimeDelta time_remaining_for_promo_to_show = base::Days(1);
  // Uses `switches::kSigninPromoOnAvatarPillDelayForNextPromoAllowed`
  // explicitly as the threshold value is shared.
  FastForwardBy(
      switches::kSigninPromoOnAvatarPillDelayForNextPromoAllowed.Get() -
      time_remaining_for_promo_to_show);
  // Last external event time check should still not allow the promo to show
  // yet.
  ASSERT_FALSE(manager.ShouldShowPromo(signin_promo_type));

  // Add enough time to allow promo to show.
  FastForwardBy(2 * time_remaining_for_promo_to_show);

  ASSERT_TRUE(manager.ShouldShowPromo(signin_promo_type));
}

class AvatarButtonPromoManagerPromoTypeParamTest
    : public AvatarButtonPromoManagerTest,
      public testing::WithParamInterface<
          ProfileMenuAvatarButtonPromoInfo::Type> {};

TEST_P(AvatarButtonPromoManagerPromoTypeParamTest, MaxShownCountReached) {
  SetSigninStateFromPromoType(GetParam());
  const int max_shown_count = 10;
  AvatarButtonPromoManager manager(identity_manager(), &pref_service(),
                                   max_shown_count,
                                   /*max_used_count=*/1);

  for (int i = 0; i < max_shown_count; ++i) {
    SCOPED_TRACE("Iteration: " + base::ToString(i));
    FastForwardToBypassPromoTypeShownTimeCheck(GetParam());
    // The promo should be shown if the shown count is below the max.
    EXPECT_TRUE(manager.ShouldShowPromo(GetParam()));
    manager.RecordPromoShown(GetParam());
  }

  // The promo should not be shown if the shown count is at the max.
  EXPECT_FALSE(manager.ShouldShowPromo(GetParam()));
}

TEST_P(AvatarButtonPromoManagerPromoTypeParamTest, MaxUsedCountReached) {
  SetSigninStateFromPromoType(GetParam());
  const int max_used_count = 5;
  AvatarButtonPromoManager manager(identity_manager(), &pref_service(),
                                   /*max_shown_count=*/10, max_used_count);

  for (int i = 0; i < max_used_count; ++i) {
    SCOPED_TRACE("Iteration: " + base::ToString(i));
    // The promo should be shown if the used count is below the max.
    EXPECT_TRUE(manager.ShouldShowPromo(GetParam()));
    manager.RecordPromoUsed(GetParam());
  }

  // The promo should not be shown if the used count is at the max.
  EXPECT_FALSE(manager.ShouldShowPromo(GetParam()));
}

TEST_P(AvatarButtonPromoManagerPromoTypeParamTest, ShowPromoStateIfSignedOut) {
  AvatarButtonPromoManager manager(identity_manager(), &pref_service(),
                                   /*max_shown_count=*/10,
                                   /*max_used_count=*/2);

  switch (GetParam()) {
    case ProfileMenuAvatarButtonPromoInfo::Type::kHistorySyncPromo:
    case ProfileMenuAvatarButtonPromoInfo::Type::kBatchUploadPromo:
    case ProfileMenuAvatarButtonPromoInfo::Type::kBatchUploadBookmarksPromo:
    case ProfileMenuAvatarButtonPromoInfo::Type::
        kBatchUploadWindows10DepreciationPromo:
    case ProfileMenuAvatarButtonPromoInfo::Type::kSyncPromo:
      EXPECT_FALSE(manager.ShouldShowPromo(GetParam()));
      break;
    case ProfileMenuAvatarButtonPromoInfo::Type::kSigninPromo:
      EXPECT_TRUE(manager.ShouldShowPromo(GetParam()));
      break;
  }
}

TEST_P(AvatarButtonPromoManagerPromoTypeParamTest,
       ShouldNotShowPromoIfSigninPending) {
  if (GetParam() == ProfileMenuAvatarButtonPromoInfo::Type::kSigninPromo) {
    GTEST_SKIP() << "Signin promo does not support the SigninPending state as "
                    "the profile is not signed in to display the promo.";
  }

  SetSigninStateFromPromoType(GetParam());
  signin::SetInvalidRefreshTokenForPrimaryAccount(identity_manager());
  AvatarButtonPromoManager manager(identity_manager(), &pref_service(),
                                   /*max_shown_count=*/10,
                                   /*max_used_count=*/2);
  EXPECT_FALSE(manager.ShouldShowPromo(GetParam()));
}

TEST_P(AvatarButtonPromoManagerPromoTypeParamTest,
       ShouldNotShowPromoIfPromotionsDisabled) {
  TestingBrowserProcess::GetGlobal()->local_state()->SetBoolean(
      prefs::kPromotionsEnabled, false);
  SetSigninStateFromPromoType(GetParam());
  AvatarButtonPromoManager manager(identity_manager(), &pref_service(),
                                   /*max_shown_count=*/10,
                                   /*max_used_count=*/2);
  EXPECT_FALSE(manager.ShouldShowPromo(GetParam()));
}

INSTANTIATE_TEST_SUITE_P(
    ,
    AvatarButtonPromoManagerPromoTypeParamTest,
    testing::ValuesIn(
        {ProfileMenuAvatarButtonPromoInfo::Type::kHistorySyncPromo,
         ProfileMenuAvatarButtonPromoInfo::Type::kBatchUploadPromo,
         ProfileMenuAvatarButtonPromoInfo::Type::kBatchUploadBookmarksPromo,
         ProfileMenuAvatarButtonPromoInfo::Type::
             kBatchUploadWindows10DepreciationPromo,
         ProfileMenuAvatarButtonPromoInfo::Type::kSyncPromo,
         ProfileMenuAvatarButtonPromoInfo::Type::kSigninPromo}));

class ComputeProfileMenuAvatarButtonPromoInfoParamTest
    : public testing::Test,
      public testing::WithParamInterface<
          ProfileMenuAvatarButtonPromoInfo::Type> {
 public:
  ComputeProfileMenuAvatarButtonPromoInfoParamTest() {
    switch (GetParam()) {
      case ProfileMenuAvatarButtonPromoInfo::Type::kHistorySyncPromo:
      case ProfileMenuAvatarButtonPromoInfo::Type::kBatchUploadPromo:
      case ProfileMenuAvatarButtonPromoInfo::Type::kBatchUploadBookmarksPromo:
      case ProfileMenuAvatarButtonPromoInfo::Type::kSigninPromo:
        scoped_feature_list_.InitWithFeatures(
            // Enabling both features to ensure that
            // `syncer::kReplaceSyncPromosWithSignInPromos` takes over.
            // Enable
            // `switches::kSigninWindows10DepreciationStateBypassForTesting` to
            // allow Windows machine to test the regular flow (non-Windows10
            // specific flow).
            /*enabled_features=*/
            {syncer::kReplaceSyncPromosWithSignInPromos,
             switches::kAvatarButtonSyncPromoForTesting,
             switches::kSigninWindows10DepreciationStateBypassForTesting,
             switches::kSigninPromoOnAvatarPill},
            /*disabled_features=*/{});
        break;
      case ProfileMenuAvatarButtonPromoInfo::Type::
          kBatchUploadWindows10DepreciationPromo:
        scoped_feature_list_.InitWithFeatures(
            // Enabling both features to ensure that
            // `syncer::kReplaceSyncPromosWithSignInPromos` takes over. Also
            // enabling `switches::kSigninWindows10DepreciationStateForTesting`
            // to simulate Windows10 setup.
            /*enabled_features=*/
            {syncer::kReplaceSyncPromosWithSignInPromos,
             switches::kAvatarButtonSyncPromoForTesting,
             switches::kSigninWindows10DepreciationStateForTesting,
             switches::kSigninPromoOnAvatarPill},
            /*disabled_features=*/{});
        break;
      case ProfileMenuAvatarButtonPromoInfo::Type::kSyncPromo:
        scoped_feature_list_.InitWithFeatures(
            // For the Sync promo to be shown
            // `syncer::kReplaceSyncPromosWithSignInPromos` must be off.
            /*enabled_features=*/{switches::kAvatarButtonSyncPromoForTesting,
                                  switches::kSigninPromoOnAvatarPill},
            /*disabled_features=*/{
                syncer::kReplaceSyncPromosWithSignInPromos,
                syncer::kReplaceSyncPromosWithSigninPromosNewSignin});
        break;
    }
  }

  void SetUp() override {
    TestingProfile::Builder builder;
    builder.AddTestingFactories(
        IdentityTestEnvironmentProfileAdaptor::
            GetIdentityTestEnvironmentFactoriesWithAppendedFactories(
                {TestingProfile::TestingFactory{
                     SyncServiceFactory::GetInstance(),
                     base::BindRepeating([](content::BrowserContext* context) {
                       return static_cast<std::unique_ptr<KeyedService>>(
                           std::make_unique<syncer::TestSyncService>());
                     })},
                 TestingProfile::TestingFactory{
                     BatchUploadServiceFactory::GetInstance(),
                     base::BindRepeating(
                         [](BatchUploadServiceTestHelper*
                                batch_upload_test_helper,
                            content::BrowserContext* context) {
                           return static_cast<std::unique_ptr<KeyedService>>(
                               batch_upload_test_helper
                                   ->CreateBatchUploadService(
                                       IdentityManagerFactory::GetForProfile(
                                           Profile::FromBrowserContext(
                                               context)),
                                       std::make_unique<
                                           BatchUploadUIDelegate>()));
                         },
                         &batch_upload_test_helper_)}}));
    profile_ = builder.Build();
  }

  Profile* profile() { return profile_.get(); }

  AccountInfo Signin(ConsentLevel consent_level = ConsentLevel::kSignin) {
    IdentityManager* identity_manager =
        IdentityManagerFactory::GetForProfile(profile());
    AccountInfo account_info = MakePrimaryAccountAvailable(
        identity_manager, "test@email.com", consent_level);
    EXPECT_FALSE(account_info.IsEmpty());

    account_info = AccountInfo::Builder(account_info)
                       .SetFullName("full_name")
                       .SetGivenName("given_name")
                       .SetHostedDomain(std::string())
                       .SetAvatarUrl("SOME_FAKE_URL")
                       .SetLocale("en")
                       .Build();

    UpdateAccountInfoForAccount(identity_manager, account_info);

    // This simplifies the setup for tests that expect to show the SyncPromo.
    if (switches::IsAvatarSyncPromoFeatureEnabled()) {
      // Simulate setting enough time passing for the cookie change.
      profile()->GetPrefs()->SetDouble(
          prefs::kGaiaCookieChangedTime,
          (base::Time::Now() -
           (switches::GetAvatarSyncPromoFeatureMinimumCookeAgeParam() +
            base::Minutes(1)))
              .InSecondsFSinceUnixEpoch());
    }

    return account_info;
  }

  void SetHistorySyncPreferenceState(bool is_type_on) {
    syncer::TestSyncService* test_sync_service =
        static_cast<syncer::TestSyncService*>(
            SyncServiceFactory::GetForProfile(profile()));
    test_sync_service->GetUserSettings()->SetSelectedType(
        syncer::UserSelectableType::kHistory, is_type_on);
    test_sync_service->GetUserSettings()->SetSelectedType(
        syncer::UserSelectableType::kTabs, is_type_on);
    test_sync_service->GetUserSettings()->SetSelectedType(
        syncer::UserSelectableType::kSavedTabGroups, is_type_on);
  }

  size_t GetLocalDataCount(ProfileMenuAvatarButtonPromoInfo::Type promo_type) {
    switch (promo_type) {
      case ProfileMenuAvatarButtonPromoInfo::Type::kHistorySyncPromo:
      case ProfileMenuAvatarButtonPromoInfo::Type::kSyncPromo:
      case ProfileMenuAvatarButtonPromoInfo::Type::kSigninPromo:
        return 0u;
      case ProfileMenuAvatarButtonPromoInfo::Type::kBatchUploadPromo:
      case ProfileMenuAvatarButtonPromoInfo::Type::kBatchUploadBookmarksPromo:
      case ProfileMenuAvatarButtonPromoInfo::Type::
          kBatchUploadWindows10DepreciationPromo:
        return 5u;
    }
  }

  // Sets the profile requirements to be able to compute the `promo_type`.
  void SetRequirementsForInputPromo(
      ProfileMenuAvatarButtonPromoInfo::Type promo_type) {
    signin::IdentityManager* identity_manager =
        IdentityManagerFactory::GetForProfile(profile());
    switch (promo_type) {
      case ProfileMenuAvatarButtonPromoInfo::Type::kHistorySyncPromo:
        Signin();
        SetHistorySyncPreferenceState(/*is_type_on=*/false);
        break;
      case ProfileMenuAvatarButtonPromoInfo::Type::kBatchUploadPromo:
      case ProfileMenuAvatarButtonPromoInfo::Type::
          kBatchUploadWindows10DepreciationPromo:
        Signin();
        SetHistorySyncPreferenceState(/*is_type_on=*/true);
        batch_upload_test_helper_.SetReturnDescriptions(
            syncer::PASSWORDS, GetLocalDataCount(promo_type));
        break;
      case ProfileMenuAvatarButtonPromoInfo::Type::kBatchUploadBookmarksPromo: {
        AccountInfo primary_account = Signin();
        profile()->GetPrefs()->SetString(
            prefs::kGoogleServicesLastSyncingGaiaId,
            primary_account.gaia.ToString());
        batch_upload_test_helper_.SetReturnDescriptions(
            syncer::BOOKMARKS, GetLocalDataCount(promo_type));
        break;
      }
      case ProfileMenuAvatarButtonPromoInfo::Type::kSyncPromo:
        Signin();
        ASSERT_FALSE(identity_manager->HasPrimaryAccount(ConsentLevel::kSync));
        break;
      case ProfileMenuAvatarButtonPromoInfo::Type::kSigninPromo:
        ASSERT_FALSE(
            identity_manager->HasPrimaryAccount(ConsentLevel::kSignin));
    }
  }

  // Sets the profile requirements to resolve the `promo_type` so that it is not
  // computed anymore.
  void ResolveRequirementsForInputPromo(
      ProfileMenuAvatarButtonPromoInfo::Type promo_type) {
    switch (promo_type) {
      case ProfileMenuAvatarButtonPromoInfo::Type::kHistorySyncPromo:
        SetHistorySyncPreferenceState(/*is_type_on=*/true);
        break;
      case ProfileMenuAvatarButtonPromoInfo::Type::kBatchUploadPromo:
      case ProfileMenuAvatarButtonPromoInfo::Type::
          kBatchUploadWindows10DepreciationPromo:
      case ProfileMenuAvatarButtonPromoInfo::Type::kBatchUploadBookmarksPromo:
        SetHistorySyncPreferenceState(/*is_type_on=*/true);
        batch_upload_test_helper_.ClearReturnDescriptions();
        break;
      case ProfileMenuAvatarButtonPromoInfo::Type::kSyncPromo:
        Signin(ConsentLevel::kSync);
        break;
      case ProfileMenuAvatarButtonPromoInfo::Type::kSigninPromo:
        Signin();
        break;
    }
  }

 private:
  content::BrowserTaskEnvironment task_environment_;

  std::unique_ptr<TestingProfile> profile_;
  BatchUploadServiceTestHelper batch_upload_test_helper_;

  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_P(ComputeProfileMenuAvatarButtonPromoInfoParamTest,
       OnlySigninPromoIfNotSignedIn) {
  base::MockCallback<base::OnceCallback<void(ProfileMenuAvatarButtonPromoInfo)>>
      result_callback;

  ProfileMenuAvatarButtonPromoInfo expected_info;
  switch (GetParam()) {
    case ProfileMenuAvatarButtonPromoInfo::Type::kHistorySyncPromo:
    case ProfileMenuAvatarButtonPromoInfo::Type::kBatchUploadPromo:
    case ProfileMenuAvatarButtonPromoInfo::Type::kBatchUploadBookmarksPromo:
    case ProfileMenuAvatarButtonPromoInfo::Type::
        kBatchUploadWindows10DepreciationPromo:
    case ProfileMenuAvatarButtonPromoInfo::Type::kSigninPromo:
      expected_info.type = ProfileMenuAvatarButtonPromoInfo::Type::kSigninPromo;
      break;
    case ProfileMenuAvatarButtonPromoInfo::Type::kSyncPromo:
      // When this promo is possible - the sign in promo should not be shown.
      break;
  }

  EXPECT_CALL(result_callback, Run(expected_info));
  ComputeProfileMenuAvatarButtonPromoInfo(*profile(), result_callback.Get());
}

TEST_P(ComputeProfileMenuAvatarButtonPromoInfoParamTest,
       PromoWhenRequirementsForPromoSet) {
  ASSERT_NO_FATAL_FAILURE(SetRequirementsForInputPromo(GetParam()));

  base::MockCallback<base::OnceCallback<void(ProfileMenuAvatarButtonPromoInfo)>>
      result_callback;
  EXPECT_CALL(result_callback,
              Run(ProfileMenuAvatarButtonPromoInfo{
                  .type = GetParam(),
                  .local_data_count = GetLocalDataCount(GetParam())}));
  ComputeProfileMenuAvatarButtonPromoInfo(*profile(), result_callback.Get());
}

TEST_P(ComputeProfileMenuAvatarButtonPromoInfoParamTest,
       NoPromoWhenRequirementsForPromoSetAndResolved) {
  ASSERT_NO_FATAL_FAILURE(SetRequirementsForInputPromo(GetParam()));
  ResolveRequirementsForInputPromo(GetParam());

  base::MockCallback<base::OnceCallback<void(ProfileMenuAvatarButtonPromoInfo)>>
      result_callback;
  EXPECT_CALL(result_callback, Run(ProfileMenuAvatarButtonPromoInfo()));
  ComputeProfileMenuAvatarButtonPromoInfo(*profile(), result_callback.Get());
}

TEST_P(ComputeProfileMenuAvatarButtonPromoInfoParamTest,
       NoPromoWhenSignedInPending) {
  IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile());
  Signin();
  signin::SetInvalidRefreshTokenForPrimaryAccount(identity_manager);
  SetHistorySyncPreferenceState(/*is_type_on=*/false);

  base::MockCallback<base::OnceCallback<void(ProfileMenuAvatarButtonPromoInfo)>>
      result_callback;
  EXPECT_CALL(result_callback, Run(ProfileMenuAvatarButtonPromoInfo()));
  ComputeProfileMenuAvatarButtonPromoInfo(*profile(), result_callback.Get());
}

INSTANTIATE_TEST_SUITE_P(
    ,
    ComputeProfileMenuAvatarButtonPromoInfoParamTest,
    testing::ValuesIn(
        {ProfileMenuAvatarButtonPromoInfo::Type::kHistorySyncPromo,
         ProfileMenuAvatarButtonPromoInfo::Type::kBatchUploadPromo,
         ProfileMenuAvatarButtonPromoInfo::Type::kBatchUploadBookmarksPromo,
         ProfileMenuAvatarButtonPromoInfo::Type::
             kBatchUploadWindows10DepreciationPromo,
         ProfileMenuAvatarButtonPromoInfo::Type::kSyncPromo,
         ProfileMenuAvatarButtonPromoInfo::Type::kSigninPromo}));

#endif  // BUILDFLAG(ENABLE_DICE_SUPPORT)

}  // namespace signin
