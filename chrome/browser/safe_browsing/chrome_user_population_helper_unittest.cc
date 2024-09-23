// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/chrome_user_population_helper.h"

#include "base/feature_list.h"
#include "base/metrics/field_trial.h"
#include "base/strings/strcat.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/safe_browsing/advanced_protection_status_manager.h"
#include "chrome/browser/safe_browsing/advanced_protection_status_manager_factory.h"
#include "chrome/browser/safe_browsing/verdict_cache_manager_factory.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/test/base/testing_profile.h"
#include "components/prefs/pref_service.h"
#include "components/safe_browsing/buildflags.h"
#include "components/safe_browsing/core/browser/verdict_cache_manager.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "components/sync/base/data_type.h"
#include "components/sync/test/test_sync_service.h"
#include "components/unified_consent/pref_names.h"
#include "components/version_info/version_info.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/protobuf/src/google/protobuf/repeated_ptr_field.h"

namespace safe_browsing {

namespace {

std::unique_ptr<KeyedService> CreateTestSyncService(
    content::BrowserContext* context) {
  return std::make_unique<syncer::TestSyncService>();
}

}  // namespace

TEST(GetUserPopulationForProfileTest, PopulatesPopulation) {
  content::BrowserTaskEnvironment task_environment;
  TestingProfile profile;
  SetSafeBrowsingState(profile.GetPrefs(),
                       SafeBrowsingState::STANDARD_PROTECTION);
  ChromeUserPopulation population = GetUserPopulationForProfile(&profile);
  EXPECT_EQ(population.user_population(), ChromeUserPopulation::SAFE_BROWSING);

  SetSafeBrowsingState(profile.GetPrefs(),
                       SafeBrowsingState::ENHANCED_PROTECTION);
  population = GetUserPopulationForProfile(&profile);
  EXPECT_EQ(population.user_population(),
            ChromeUserPopulation::ENHANCED_PROTECTION);

  SetSafeBrowsingState(profile.GetPrefs(),
                       SafeBrowsingState::STANDARD_PROTECTION);
  SetExtendedReportingPrefForTests(profile.GetPrefs(), true);
  population = GetUserPopulationForProfile(&profile);
  EXPECT_EQ(population.user_population(),
            ChromeUserPopulation::EXTENDED_REPORTING);
}

TEST(GetUserPopulationForProfileTest, PopulatesMBB) {
  content::BrowserTaskEnvironment task_environment;
  TestingProfile profile;
  profile.GetPrefs()->SetBoolean(
      unified_consent::prefs::kUrlKeyedAnonymizedDataCollectionEnabled, false);
  ChromeUserPopulation population = GetUserPopulationForProfile(&profile);
  EXPECT_FALSE(population.is_mbb_enabled());

  profile.GetPrefs()->SetBoolean(
      unified_consent::prefs::kUrlKeyedAnonymizedDataCollectionEnabled, true);
  population = GetUserPopulationForProfile(&profile);
  EXPECT_TRUE(population.is_mbb_enabled());
}

TEST(GetUserPopulationForProfileTest, PopulatesIncognito) {
  content::BrowserTaskEnvironment task_environment;
  TestingProfile profile;
  ChromeUserPopulation population = GetUserPopulationForProfile(&profile);
  EXPECT_FALSE(population.is_incognito());

  Profile* incognito_profile = profile.GetOffTheRecordProfile(
      Profile::OTRProfileID::CreateUniqueForTesting(),
      /*create_if_needed=*/true);
  population = GetUserPopulationForProfile(incognito_profile);
  EXPECT_TRUE(population.is_incognito());
}

TEST(GetUserPopulationForProfileTest, PopulatesSync) {
  content::BrowserTaskEnvironment task_environment;
  TestingProfile profile;
  syncer::TestSyncService* sync_service = static_cast<syncer::TestSyncService*>(
      SyncServiceFactory::GetInstance()->SetTestingFactoryAndUse(
          &profile, base::BindRepeating(&CreateTestSyncService)));

  {
    ASSERT_TRUE(sync_service->GetActiveDataTypes().Has(
        syncer::HISTORY_DELETE_DIRECTIVES));
    ChromeUserPopulation population = GetUserPopulationForProfile(&profile);
    EXPECT_TRUE(population.is_history_sync_enabled());
  }

  {
    sync_service->SetSignedOut();

    ChromeUserPopulation population = GetUserPopulationForProfile(&profile);
    EXPECT_FALSE(population.is_history_sync_enabled());
  }

  {
    // Enabling local sync reports the sync service as signed-out, so this is
    // consistent with the SetSignedOut() call above.
    // TODO(crbug.com/350494796): TestSyncService should honor that.
    sync_service->SetLocalSyncEnabled(true);

    ChromeUserPopulation population = GetUserPopulationForProfile(&profile);
    EXPECT_FALSE(population.is_history_sync_enabled());
  }

  {
    sync_service->SetLocalSyncEnabled(false);
    sync_service->SetSignedIn(signin::ConsentLevel::kSync);
    sync_service->GetUserSettings()->SetSelectedTypes(
        /*sync_everything=*/false,
        /*types=*/syncer::UserSelectableTypeSet());

    ChromeUserPopulation population = GetUserPopulationForProfile(&profile);
    EXPECT_FALSE(population.is_history_sync_enabled());
  }
}

TEST(GetUserPopulationForProfileTest, PopulatesSignedIn) {
  content::BrowserTaskEnvironment task_environment;
  TestingProfile profile;

  {
    ChromeUserPopulation population = GetUserPopulationForProfile(&profile);
    EXPECT_FALSE(population.is_signed_in());
  }

  {
    signin::IdentityManager* identity_manager =
        IdentityManagerFactory::GetForProfile(&profile);
    signin::SetPrimaryAccount(identity_manager, "test@example.com",
                              signin::ConsentLevel::kSignin);
    ChromeUserPopulation population = GetUserPopulationForProfile(&profile);
    EXPECT_TRUE(population.is_signed_in());
  }
}

#if BUILDFLAG(FULL_SAFE_BROWSING)
TEST(GetUserPopulationForProfileTest, PopulatesAdvancedProtection) {
  content::BrowserTaskEnvironment task_environment;
  TestingProfile profile;

  AdvancedProtectionStatusManagerFactory::GetForProfile(&profile)
      ->SetAdvancedProtectionStatusForTesting(true);
  ChromeUserPopulation population = GetUserPopulationForProfile(&profile);
  EXPECT_TRUE(population.is_under_advanced_protection());

  AdvancedProtectionStatusManagerFactory::GetForProfile(&profile)
      ->SetAdvancedProtectionStatusForTesting(false);
  population = GetUserPopulationForProfile(&profile);
  EXPECT_FALSE(population.is_under_advanced_protection());
}
#endif

TEST(GetUserPopulationForProfileTest, PopulatesUserAgent) {
  content::BrowserTaskEnvironment task_environment;
  TestingProfile profile;
  std::string user_agent =
      base::StrCat({version_info::GetProductNameAndVersionForUserAgent(), "/",
                    version_info::GetOSType()});
  ChromeUserPopulation population = GetUserPopulationForProfile(&profile);
  EXPECT_EQ(population.user_agent(), user_agent);
}

TEST(GetPageLoadTokenForURLTest, PopulatesEmptyTokenForEmptyProfile) {
  content::BrowserTaskEnvironment task_environment;
  ChromeUserPopulation::PageLoadToken token =
      GetPageLoadTokenForURL(nullptr, GURL(""));
  EXPECT_FALSE(token.has_token_value());
}

TEST(GetPageLoadTokenForURLTest, PopulatesNewTokenValueForURL) {
  content::BrowserTaskEnvironment task_environment;
  TestingProfile profile;
  ChromeUserPopulation::PageLoadToken token =
      GetPageLoadTokenForURL(&profile, GURL("https://www.example.com"));
  EXPECT_TRUE(token.has_token_value());
}

TEST(GetPageLoadTokenForURLTest, PopulatesExistingTokenValueForURL) {
  content::BrowserTaskEnvironment task_environment;
  TestingProfile profile;
  VerdictCacheManager* cache_manager =
      VerdictCacheManagerFactory::GetForProfile(&profile);
  cache_manager->CreatePageLoadToken(profile.GetHomePage());
  ChromeUserPopulation::PageLoadToken token =
      GetPageLoadTokenForURL(&profile, profile.GetHomePage());
  EXPECT_TRUE(token.has_token_value());
}

#if BUILDFLAG(IS_WIN)
TEST(GetUserPopulationForProfileWithCookieTheftExperiments,
     PopulatesExperimentsForEsb) {
  content::BrowserTaskEnvironment task_environment;
  base::FieldTrialList::CreateFieldTrial("LockProfileCookieDatabase",
                                         "Enabled");
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitFromCommandLine(
      "LockProfileCookieDatabase<LockProfileCookieDatabase.Enabled", "");

  TestingProfile profile;
  SetSafeBrowsingState(profile.GetPrefs(),
                       SafeBrowsingState::ENHANCED_PROTECTION);
  ChromeUserPopulation population =
      GetUserPopulationForProfileWithCookieTheftExperiments(&profile);

  EXPECT_TRUE(base::Contains(population.finch_active_groups(),
                             "LockProfileCookieDatabase.Enabled"));
}

TEST(GetUserPopulationForProfileWithCookieTheftExperiments,
     DoesNotPopulateExperimentsForSsb) {
  content::BrowserTaskEnvironment task_environment;
  base::FieldTrialList::CreateFieldTrial("LockProfileCookieDatabase",
                                         "Enabled");
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitFromCommandLine(
      "LockProfileCookieDatabase<LockProfileCookieDatabase.Enabled", "");

  TestingProfile profile;
  SetSafeBrowsingState(profile.GetPrefs(),
                       SafeBrowsingState::STANDARD_PROTECTION);
  ChromeUserPopulation population =
      GetUserPopulationForProfileWithCookieTheftExperiments(&profile);

  EXPECT_FALSE(base::Contains(population.finch_active_groups(),
                              "LockProfileCookieDatabase.Enabled"));
}
#endif

}  // namespace safe_browsing
