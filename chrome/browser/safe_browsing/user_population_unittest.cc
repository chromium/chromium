// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/user_population.h"

#include "chrome/browser/safe_browsing/advanced_protection_status_manager.h"
#include "chrome/browser/safe_browsing/advanced_protection_status_manager_factory.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/test/base/testing_profile.h"
#include "components/prefs/pref_service.h"
#include "components/safe_browsing/buildflags.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "components/sync/base/model_type.h"
#include "components/sync/driver/test_sync_service.h"
#include "components/unified_consent/pref_names.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace safe_browsing {

namespace {

std::unique_ptr<KeyedService> CreateTestSyncService(
    content::BrowserContext* context) {
  return std::make_unique<syncer::TestSyncService>();
}

}  // namespace

TEST(GetUserPopulationTest, PopulatesPopulation) {
  content::BrowserTaskEnvironment task_environment;
  TestingProfile profile;
  SetSafeBrowsingState(profile.GetPrefs(),
                       SafeBrowsingState::STANDARD_PROTECTION);
  ChromeUserPopulation population = GetUserPopulation(&profile);
  EXPECT_EQ(population.user_population(), ChromeUserPopulation::SAFE_BROWSING);

  SetSafeBrowsingState(profile.GetPrefs(),
                       SafeBrowsingState::ENHANCED_PROTECTION);
  population = GetUserPopulation(&profile);
  EXPECT_EQ(population.user_population(),
            ChromeUserPopulation::ENHANCED_PROTECTION);

  SetSafeBrowsingState(profile.GetPrefs(),
                       SafeBrowsingState::STANDARD_PROTECTION);
  SetExtendedReportingPrefForTests(profile.GetPrefs(), true);
  population = GetUserPopulation(&profile);
  EXPECT_EQ(population.user_population(),
            ChromeUserPopulation::EXTENDED_REPORTING);
}

TEST(GetUserPopulationTest, PopulatesMBB) {
  content::BrowserTaskEnvironment task_environment;
  TestingProfile profile;
  profile.GetPrefs()->SetBoolean(
      unified_consent::prefs::kUrlKeyedAnonymizedDataCollectionEnabled, false);
  ChromeUserPopulation population = GetUserPopulation(&profile);
  EXPECT_FALSE(population.is_mbb_enabled());

  profile.GetPrefs()->SetBoolean(
      unified_consent::prefs::kUrlKeyedAnonymizedDataCollectionEnabled, true);
  population = GetUserPopulation(&profile);
  EXPECT_TRUE(population.is_mbb_enabled());
}

TEST(GetUserPopulationTest, PopulatesIncognito) {
  content::BrowserTaskEnvironment task_environment;
  TestingProfile profile;
  ChromeUserPopulation population = GetUserPopulation(&profile);
  EXPECT_FALSE(population.is_incognito());

  Profile* incognito_profile = profile.GetOffTheRecordProfile(
      Profile::OTRProfileID("Test::SafeBrowsingUserPopulation"));
  population = GetUserPopulation(incognito_profile);
  EXPECT_TRUE(population.is_incognito());
}

TEST(GetUserPopulationTest, PopulatesSync) {
  content::BrowserTaskEnvironment task_environment;
  TestingProfile profile;
  syncer::TestSyncService* sync_service = static_cast<syncer::TestSyncService*>(
      ProfileSyncServiceFactory::GetInstance()->SetTestingFactoryAndUse(
          &profile, base::BindRepeating(&CreateTestSyncService)));

  {
    sync_service->SetTransportState(
        syncer::SyncService::TransportState::ACTIVE);
    sync_service->SetLocalSyncEnabled(false);
    sync_service->SetActiveDataTypes(syncer::ModelTypeSet::All());

    ChromeUserPopulation population = GetUserPopulation(&profile);
    EXPECT_TRUE(population.is_history_sync_enabled());
  }

  {
    sync_service->SetTransportState(
        syncer::SyncService::TransportState::DISABLED);
    sync_service->SetLocalSyncEnabled(false);
    sync_service->SetActiveDataTypes(syncer::ModelTypeSet::All());

    ChromeUserPopulation population = GetUserPopulation(&profile);
    EXPECT_FALSE(population.is_history_sync_enabled());
  }

  {
    sync_service->SetTransportState(
        syncer::SyncService::TransportState::ACTIVE);
    sync_service->SetLocalSyncEnabled(true);
    sync_service->SetActiveDataTypes(syncer::ModelTypeSet::All());

    ChromeUserPopulation population = GetUserPopulation(&profile);
    EXPECT_FALSE(population.is_history_sync_enabled());
  }

  {
    sync_service->SetTransportState(
        syncer::SyncService::TransportState::ACTIVE);
    sync_service->SetLocalSyncEnabled(false);
    sync_service->SetActiveDataTypes(syncer::ModelTypeSet());

    ChromeUserPopulation population = GetUserPopulation(&profile);
    EXPECT_FALSE(population.is_history_sync_enabled());
  }
}

#if BUILDFLAG(FULL_SAFE_BROWSING)
TEST(GetUserPopulationTest, PopulatesAdvancedProtection) {
  content::BrowserTaskEnvironment task_environment;
  TestingProfile profile;

  AdvancedProtectionStatusManagerFactory::GetForProfile(&profile)
      ->SetAdvancedProtectionStatusForTesting(true);
  ChromeUserPopulation population = GetUserPopulation(&profile);
  EXPECT_TRUE(population.is_under_advanced_protection());

  AdvancedProtectionStatusManagerFactory::GetForProfile(&profile)
      ->SetAdvancedProtectionStatusForTesting(false);
  population = GetUserPopulation(&profile);
  EXPECT_FALSE(population.is_under_advanced_protection());
}
#endif

}  // namespace safe_browsing
