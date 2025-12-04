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

TEST(SigninPromoTest, IsSignInPromo_BookmarksWithExplicitSignin) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{switches::kSyncEnableBookmarksInTransportMode},
      /*disabled_features=*/{});

  EXPECT_TRUE(IsSignInPromo(signin_metrics::AccessPoint::kBookmarkBubble));
}

TEST(SigninPromoTest, IsSignInPromo_BookmarksWithoutExplicitSignin) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{},
      /*disabled_features=*/{switches::kSyncEnableBookmarksInTransportMode});

  EXPECT_FALSE(IsSignInPromo(signin_metrics::AccessPoint::kBookmarkBubble));
}

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
                     .SetManifest(base::Value::Dict()
                                      .Set("name", "test")
                                      .Set("manifest_version", 2)
                                      .Set("version", "1.0.0"))
                     .SetLocation(location)
                     .Build();

    return extension_.get();
  }

 private:
  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<TestingProfile> profile_;
  std::unique_ptr<IdentityTestEnvironmentProfileAdaptor>
      identity_test_env_adaptor_;
  scoped_refptr<const extensions::Extension> extension_;
};

TEST_F(ShowPromoTest, DoNotShowBookmarkSignInPromoWithoutExplicitSignIn) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{},
      /*disabled_features=*/{switches::kSyncEnableBookmarksInTransportMode});

  EXPECT_FALSE(ShouldShowBookmarkSignInPromo(*profile()));
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
#if BUILDFLAG(IS_CHROMEOS)
  // No sync promo on Ash.
  EXPECT_FALSE(ShouldShowSyncPromo(*profile()));
#else
  EXPECT_TRUE(ShouldShowSyncPromo(*profile()));
#endif
}
#endif  // !BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
TEST_F(ShowSyncPromoTest, ShowExtensionSyncPromoWithoutFeatureFlag) {
  EXPECT_TRUE(ShouldShowExtensionSyncPromo(*profile(), *CreateExtension()));
}

TEST_F(ShowSyncPromoTest, DoNotShowExtensionSyncPromoWithSyncDisabled) {
  DisableSync();
  ASSERT_FALSE(ShouldShowSyncPromo(*profile()));

  EXPECT_FALSE(ShouldShowExtensionSyncPromo(*profile(), *CreateExtension()));
}

TEST_F(ShowSyncPromoTest, DoNotShowExtensionSyncPromoWithUnpackedExtension) {
  const extensions::Extension* unpacked_extension =
      CreateExtension(extensions::mojom::ManifestLocation::kUnpacked);

  // Unpacked extensions cannot be synced so the sync promo is not shown.
  ASSERT_TRUE(unpacked_extension);
  ASSERT_FALSE(
      extensions::sync_util::ShouldSync(profile(), unpacked_extension));

  EXPECT_FALSE(ShouldShowExtensionSyncPromo(*profile(), *unpacked_extension));
}

TEST_F(ShowSyncPromoTest,
       DoNotShowExtensionSyncPromoWithSyncingExtensionsEnabled) {
  ON_CALL(*sync_service()->GetMockUserSettings(), GetSelectedTypes())
      .WillByDefault(testing::Return(syncer::UserSelectableTypeSet::All()));
  ASSERT_TRUE(extensions::sync_util::IsSyncingExtensionsEnabled(profile()));

  EXPECT_FALSE(ShouldShowExtensionSyncPromo(*profile(), *CreateExtension()));
}

TEST_F(ShowSyncPromoTest,
       DoNotShowExtensionSyncPromoWithExplicitBrowserSigninPref) {
  profile()->GetPrefs()->SetBoolean(prefs::kExplicitBrowserSignin, true);
  ASSERT_TRUE(profile()->GetPrefs()->GetBoolean(prefs::kExplicitBrowserSignin));

  EXPECT_FALSE(ShouldShowExtensionSyncPromo(*profile(), *CreateExtension()));
}

#endif  // BUILDFLAG(ENABLE_DICE_SUPPORT)

#if !BUILDFLAG(IS_CHROMEOS)
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
#endif  // !BUILDFLAG(IS_CHROMEOS)

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
class ShowSigninPromoTestWithFeatureFlags : public ShowPromoTest {
 public:
  void SetUp() override {
    ShowPromoTest::SetUp();
    feature_list_.InitWithFeatures(
        /*enabled_features=*/
        {switches::kSyncEnableBookmarksInTransportMode,
         syncer::kReplaceSyncPromosWithSignInPromos,
         syncer::kUnoPhase2FollowUp},
        /*disabled_features=*/{});
    ON_CALL(*sync_service(), GetDataTypesForTransportOnlyMode())
        .WillByDefault(testing::Return(syncer::DataTypeSet::All()));
  }

  GaiaId gaia_id() {
    return identity_manager()
        ->GetPrimaryAccountInfo(ConsentLevel::kSignin)
        .gaia;
  }

  autofill::AutofillProfile CreateAddress(
      const std::string& country_code = "US") {
    return autofill::test::StandardProfile(
        autofill::AddressCountryCode(country_code));
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

  ON_CALL(*sync_service(), GetDisableReasons())
      .WillByDefault(testing::Return(syncer::SyncService::DisableReasonSet(
          {syncer::SyncService::DISABLE_REASON_ENTERPRISE_POLICY})));

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
       DoNotShowPromoAfterTwoTimesDismissed) {
  ASSERT_TRUE(ShouldShowAddressSignInPromo(*profile(), CreateAddress()));

  profile()->GetPrefs()->SetInteger(
      prefs::kAutofillSignInPromoDismissCountPerProfile, 2);

  EXPECT_FALSE(ShouldShowPasswordSignInPromo(*profile()));
  EXPECT_FALSE(ShouldShowAddressSignInPromo(*profile(), CreateAddress()));
  EXPECT_FALSE(ShouldShowBookmarkSignInPromo(*profile()));
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
       OnlyShowBookmarkPromoInSignInPendingWithAccountStorageEnabled) {
  MakePrimaryAccountAvailable(identity_manager(), "test@email.com",
                              ConsentLevel::kSignin);
  signin::SetInvalidRefreshTokenForPrimaryAccount(identity_manager());

  // Promo is showing in sign in pending with account storage enabled.
  ON_CALL(*sync_service()->GetMockUserSettings(), GetSelectedTypes())
      .WillByDefault(testing::Return(syncer::UserSelectableTypeSet(
          {syncer::UserSelectableType::kBookmarks})));
  EXPECT_TRUE(ShouldShowBookmarkSignInPromo(*profile()));

  // Promo is not showing in sign in pending with account storage disabled.
  ON_CALL(*sync_service()->GetMockUserSettings(), GetSelectedTypes())
      .WillByDefault(testing::Return(syncer::UserSelectableTypeSet()));
  EXPECT_FALSE(ShouldShowBookmarkSignInPromo(*profile()));

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
          .WithAccessPoint(signin_metrics::AccessPoint::kUnknown)
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
          .WithAccessPoint(signin_metrics::AccessPoint::kUnknown)
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
          .WithAccessPoint(signin_metrics::AccessPoint::kUnknown)
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
        {
            switches::kSyncEnableBookmarksInTransportMode,
            syncer::kReplaceSyncPromosWithSignInPromos,
        },
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
          .WithAccessPoint(signin_metrics::AccessPoint::kUnknown)
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
       DoNotShowAddressPromoAfterTwentyTimesShown) {
  ASSERT_TRUE(ShouldShowAddressSignInPromo(*profile(), CreateAddress()));

  profile()->GetPrefs()->SetInteger(
      prefs::kAddressSignInPromoShownCountPerProfileForLimitsExperiment,
      switches::kContextualSigninPromoShownThreshold.Get());

  EXPECT_FALSE(ShouldShowAddressSignInPromo(*profile(), CreateAddress()));
  EXPECT_TRUE(ShouldShowPasswordSignInPromo(*profile()));
}

TEST_F(ShowSigninPromoTestWithFeatureFlagsPromoLimitsExperiment,
       DoNotShowPasswordPromoAfterTwentyTimesShown) {
  ASSERT_TRUE(ShouldShowPasswordSignInPromo(*profile()));

  profile()->GetPrefs()->SetInteger(
      prefs::kPasswordSignInPromoShownCountPerProfileForLimitsExperiment,
      switches::kContextualSigninPromoShownThreshold.Get());

  EXPECT_FALSE(ShouldShowPasswordSignInPromo(*profile()));
  EXPECT_TRUE(ShouldShowAddressSignInPromo(*profile(), CreateAddress()));
}

TEST_F(ShowSigninPromoTestWithFeatureFlagsPromoLimitsExperiment,
       DoNotShowBookmarkPromoAfterTwentyTimesShown) {
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
       SkipCheckForNumberOfTimesDismissed) {
  EXPECT_TRUE(ShouldShowPasswordSignInPromo(*profile()));

  profile()->GetPrefs()->SetInteger(
      prefs::kAutofillSignInPromoDismissCountPerProfile, INT_MAX);

  EXPECT_TRUE(ShouldShowPasswordSignInPromo(*profile()));
}

class SyncPromoIdentityPillManagerTest : public testing::Test {
 public:
  SyncPromoIdentityPillManagerTest() {
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/{syncer::kReplaceSyncPromosWithSignInPromos,
                              switches::kAvatarButtonSyncPromoForTesting,
                              switches::
                                  kSigninWindows10DepreciationStateForTesting},
        /*disabled_features=*/{});

    SigninPrefs::RegisterProfilePrefs(pref_service_.registry());
  }

  AccountInfo Signin(const std::string& email) {
    return MakePrimaryAccountAvailable(identity_manager(), email,
                                       ConsentLevel::kSignin);
  }

  IdentityManager* identity_manager() {
    return identity_test_environment_.identity_manager();
  }
  PrefService& pref_service() { return pref_service_; }

 private:
  content::BrowserTaskEnvironment task_environment_;
  IdentityTestEnvironment identity_test_environment_;
  TestingPrefServiceSimple pref_service_;

  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(SyncPromoIdentityPillManagerTest, PromoTypesUseDifferentShownLimits) {
  std::array<ProfileMenuAvatarButtonPromoInfo::Type, 5> promo_type_list{
      ProfileMenuAvatarButtonPromoInfo::Type::kHistorySyncPromo,
      ProfileMenuAvatarButtonPromoInfo::Type::kBatchUploadPromo,
      ProfileMenuAvatarButtonPromoInfo::Type::kBatchUploadBookmarksPromo,
      ProfileMenuAvatarButtonPromoInfo::Type::
          kBatchUploadWindows10DepreciationPromo,
      ProfileMenuAvatarButtonPromoInfo::Type::kSyncPromo,
  };

  Signin("test@email.com");

  const size_t max_shown_count = 2;
  SyncPromoIdentityPillManager manager(identity_manager(), &pref_service(),
                                       max_shown_count, /*max_used_count=*/2);

  for (auto promo_type : promo_type_list) {
    for (size_t count = 0; count < max_shown_count; ++count) {
      ASSERT_TRUE(manager.ShouldShowPromo(promo_type));
      manager.RecordPromoShown(promo_type);
    }
    ASSERT_FALSE(manager.ShouldShowPromo(promo_type));
  }
}

TEST_F(SyncPromoIdentityPillManagerTest, PromoTypesUseDifferentUsedLimits) {
  std::array<ProfileMenuAvatarButtonPromoInfo::Type, 5> promo_type_list{
      ProfileMenuAvatarButtonPromoInfo::Type::kHistorySyncPromo,
      ProfileMenuAvatarButtonPromoInfo::Type::kBatchUploadPromo,
      ProfileMenuAvatarButtonPromoInfo::Type::kBatchUploadBookmarksPromo,
      ProfileMenuAvatarButtonPromoInfo::Type::
          kBatchUploadWindows10DepreciationPromo,
      ProfileMenuAvatarButtonPromoInfo::Type::kSyncPromo,
  };

  Signin("test@email.com");

  const size_t max_used_count = 2;
  SyncPromoIdentityPillManager manager(identity_manager(), &pref_service(),
                                       /*max_shown_count=*/2, max_used_count);

  for (auto promo_type : promo_type_list) {
    for (size_t count = 0; count < max_used_count; ++count) {
      ASSERT_TRUE(manager.ShouldShowPromo(promo_type));
      manager.RecordPromoUsed(promo_type);
    }
    ASSERT_FALSE(manager.ShouldShowPromo(promo_type));
  }
}

class SyncPromoIdentityPillManagerWithParamsTest
    : public SyncPromoIdentityPillManagerTest,
      public testing::WithParamInterface<
          ProfileMenuAvatarButtonPromoInfo::Type> {};

TEST_P(SyncPromoIdentityPillManagerWithParamsTest, MaxShownCount) {
  Signin("test@email.com");
  const int max_shown_count = 10;
  SyncPromoIdentityPillManager manager(identity_manager(), &pref_service(),
                                       max_shown_count,
                                       /*max_used_count=*/1);

  for (int i = 0; i < max_shown_count; ++i) {
    SCOPED_TRACE("Iteration: " + base::ToString(i));
    // The promo should be shown if the shown count is below the max.
    EXPECT_TRUE(manager.ShouldShowPromo(GetParam()));
    manager.RecordPromoShown(GetParam());
  }

  // The promo should not be shown if the shown count is at the max.
  EXPECT_FALSE(manager.ShouldShowPromo(GetParam()));
}

TEST_P(SyncPromoIdentityPillManagerWithParamsTest, MaxUsedCount) {
  Signin("test@email.com");
  const int max_used_count = 5;
  SyncPromoIdentityPillManager manager(identity_manager(), &pref_service(),
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

TEST_P(SyncPromoIdentityPillManagerWithParamsTest,
       ShouldNotShowPromoIfSignedOut) {
  SyncPromoIdentityPillManager manager(identity_manager(), &pref_service(),
                                       /*max_shown_count=*/10,
                                       /*max_used_count=*/2);
  EXPECT_FALSE(manager.ShouldShowPromo(GetParam()));
}

TEST_P(SyncPromoIdentityPillManagerWithParamsTest,
       ShouldNotShowPromoIfSigninPending) {
  Signin("test@email.com");
  signin::SetInvalidRefreshTokenForPrimaryAccount(identity_manager());
  SyncPromoIdentityPillManager manager(identity_manager(), &pref_service(),
                                       /*max_shown_count=*/10,
                                       /*max_used_count=*/2);
  EXPECT_FALSE(manager.ShouldShowPromo(GetParam()));
}

TEST_P(SyncPromoIdentityPillManagerWithParamsTest,
       ShouldNotShowPromoIfPromotionsDisabled) {
  TestingBrowserProcess::GetGlobal()->local_state()->SetBoolean(
      prefs::kPromotionsEnabled, false);
  Signin("test@email.com");
  SyncPromoIdentityPillManager manager(identity_manager(), &pref_service(),
                                       /*max_shown_count=*/10,
                                       /*max_used_count=*/2);
  EXPECT_FALSE(manager.ShouldShowPromo(GetParam()));
}

INSTANTIATE_TEST_SUITE_P(
    ,
    SyncPromoIdentityPillManagerWithParamsTest,
    testing::ValuesIn(
        {ProfileMenuAvatarButtonPromoInfo::Type::kHistorySyncPromo,
         ProfileMenuAvatarButtonPromoInfo::Type::kBatchUploadPromo,
         ProfileMenuAvatarButtonPromoInfo::Type::kBatchUploadBookmarksPromo,
         ProfileMenuAvatarButtonPromoInfo::Type::
             kBatchUploadWindows10DepreciationPromo,
         ProfileMenuAvatarButtonPromoInfo::Type::kSyncPromo}));

class ComputeProfileMenuAvatarButtonPromoInfoBaseTest : public testing::Test {
 public:
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

  void Signin(ConsentLevel consent_level = ConsentLevel::kSignin) {
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
        return 0u;
      case ProfileMenuAvatarButtonPromoInfo::Type::kBatchUploadPromo:
      case ProfileMenuAvatarButtonPromoInfo::Type::kBatchUploadBookmarksPromo:
      case ProfileMenuAvatarButtonPromoInfo::Type::
          kBatchUploadWindows10DepreciationPromo:
        return 5u;
    }
  }

  void SetRequirementsForInputPromo(
      ProfileMenuAvatarButtonPromoInfo::Type promo_type) {
    signin::IdentityManager* identity_manager =
        IdentityManagerFactory::GetForProfile(profile());
    CoreAccountInfo primary_account =
        identity_manager->GetPrimaryAccountInfo(ConsentLevel::kSignin);
    ASSERT_TRUE(!primary_account.IsEmpty());
    switch (promo_type) {
      case ProfileMenuAvatarButtonPromoInfo::Type::kHistorySyncPromo:
        SetHistorySyncPreferenceState(/*is_type_on=*/false);
        break;
      case ProfileMenuAvatarButtonPromoInfo::Type::kBatchUploadPromo:
      case ProfileMenuAvatarButtonPromoInfo::Type::
          kBatchUploadWindows10DepreciationPromo:
        SetHistorySyncPreferenceState(/*is_type_on=*/true);
        batch_upload_test_helper_.SetReturnDescriptions(
            syncer::PASSWORDS, GetLocalDataCount(promo_type));
        break;
      case ProfileMenuAvatarButtonPromoInfo::Type::kBatchUploadBookmarksPromo:
        profile()->GetPrefs()->SetString(
            prefs::kGoogleServicesLastSyncingGaiaId,
            primary_account.gaia.ToString());
        batch_upload_test_helper_.SetReturnDescriptions(
            syncer::BOOKMARKS, GetLocalDataCount(promo_type));
        break;
      case ProfileMenuAvatarButtonPromoInfo::Type::kSyncPromo:
        ASSERT_FALSE(identity_manager->HasPrimaryAccount(ConsentLevel::kSync));
        break;
    }
  }

 private:
  content::BrowserTaskEnvironment task_environment_;

  std::unique_ptr<TestingProfile> profile_;
  BatchUploadServiceTestHelper batch_upload_test_helper_;
};

class ComputeProfileMenuAvatarButtonPromoInfoTest
    : public ComputeProfileMenuAvatarButtonPromoInfoBaseTest,
      public testing::WithParamInterface<
          ProfileMenuAvatarButtonPromoInfo::Type> {
 public:
  ComputeProfileMenuAvatarButtonPromoInfoTest() {
    switch (GetParam()) {
      case ProfileMenuAvatarButtonPromoInfo::Type::kHistorySyncPromo:
      case ProfileMenuAvatarButtonPromoInfo::Type::kBatchUploadPromo:
      case ProfileMenuAvatarButtonPromoInfo::Type::kBatchUploadBookmarksPromo:
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
             switches::kSigninWindows10DepreciationStateBypassForTesting},
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
             switches::kSigninWindows10DepreciationStateForTesting},
            /*disabled_features=*/{});
        break;
      case ProfileMenuAvatarButtonPromoInfo::Type::kSyncPromo:
        scoped_feature_list_.InitWithFeatures(
            // For the Sync promo to be shown
            // `syncer::kReplaceSyncPromosWithSignInPromos` must be off.
            /*enabled_features=*/{switches::kAvatarButtonSyncPromoForTesting},
            /*disabled_features=*/{syncer::kReplaceSyncPromosWithSignInPromos});
        break;
    }
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_P(ComputeProfileMenuAvatarButtonPromoInfoTest, NoPromoNotSignedIn) {
  base::MockCallback<base::OnceCallback<void(ProfileMenuAvatarButtonPromoInfo)>>
      result_callback;
  // Default value.
  EXPECT_CALL(result_callback, Run(ProfileMenuAvatarButtonPromoInfo()));
  ComputeProfileMenuAvatarButtonPromoInfo(*profile(), result_callback.Get());
}

TEST_P(ComputeProfileMenuAvatarButtonPromoInfoTest,
       PromoShownWhenSignedInAndRequirementsForPromoSet) {
  Signin();
  ASSERT_NO_FATAL_FAILURE(SetRequirementsForInputPromo(GetParam()));

  base::MockCallback<base::OnceCallback<void(ProfileMenuAvatarButtonPromoInfo)>>
      result_callback;
  EXPECT_CALL(result_callback,
              Run(ProfileMenuAvatarButtonPromoInfo{
                  .type = GetParam(),
                  .local_data_count = GetLocalDataCount(GetParam())}));
  ComputeProfileMenuAvatarButtonPromoInfo(*profile(), result_callback.Get());
}

TEST_P(ComputeProfileMenuAvatarButtonPromoInfoTest,
       PromoNotShownWhenSignedInWithHistorySyncPreference) {
  ConsentLevel consent_level;
  switch (GetParam()) {
    case ProfileMenuAvatarButtonPromoInfo::Type::kHistorySyncPromo:
    case ProfileMenuAvatarButtonPromoInfo::Type::kBatchUploadPromo:
    case ProfileMenuAvatarButtonPromoInfo::Type::kBatchUploadBookmarksPromo:
    case ProfileMenuAvatarButtonPromoInfo::Type::
        kBatchUploadWindows10DepreciationPromo:
      consent_level = ConsentLevel::kSignin;
      break;
    case ProfileMenuAvatarButtonPromoInfo::Type::kSyncPromo:
      consent_level = ConsentLevel::kSync;
      break;
  }
  Signin(consent_level);
  SetHistorySyncPreferenceState(/*is_type_on=*/true);

  base::MockCallback<base::OnceCallback<void(ProfileMenuAvatarButtonPromoInfo)>>
      result_callback;
  EXPECT_CALL(result_callback, Run(ProfileMenuAvatarButtonPromoInfo()));
  ComputeProfileMenuAvatarButtonPromoInfo(*profile(), result_callback.Get());
}

TEST_P(ComputeProfileMenuAvatarButtonPromoInfoTest,
       PromoNotShownWhenSignedInPending) {
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
    ComputeProfileMenuAvatarButtonPromoInfoTest,
    testing::ValuesIn(
        {ProfileMenuAvatarButtonPromoInfo::Type::kHistorySyncPromo,
         ProfileMenuAvatarButtonPromoInfo::Type::kBatchUploadPromo,
         ProfileMenuAvatarButtonPromoInfo::Type::kBatchUploadBookmarksPromo,
         ProfileMenuAvatarButtonPromoInfo::Type::
             kBatchUploadWindows10DepreciationPromo,
         ProfileMenuAvatarButtonPromoInfo::Type::kSyncPromo}));

#endif  // BUILDFLAG(ENABLE_DICE_SUPPORT)

}  // namespace signin
