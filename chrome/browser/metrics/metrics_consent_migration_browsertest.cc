// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/path_service.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/metrics/testing/metrics_consent_override.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profile_test_util.h"
#include "chrome/browser/sync/test/integration/sync_service_impl_harness.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/common/chrome_paths.h"
#include "components/metrics/enabled_state_provider.h"
#include "components/metrics/metrics_features.h"
#include "components/metrics/metrics_profile_pref_names.h"
#include "components/metrics_services_manager/metrics_services_manager.h"
#include "components/prefs/pref_service.h"
#include "components/sync/base/pref_names.h"
#include "components/sync/base/user_selectable_type.h"
#include "components/sync/service/sync_user_settings.h"
#include "components/ukm/ukm_service.h"
#include "components/ukm/ukm_test_helper.h"
#include "components/unified_consent/pref_names.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_launcher.h"

class MetricsConsentMigrationBrowserTest : public SyncTest {
 public:
  MetricsConsentMigrationBrowserTest() : SyncTest(SINGLE_CLIENT) {
    metrics::EnabledStateProvider::SetIgnoreForceFieldTrialsForTesting(true);
    if (content::IsPreTest()) {
      // PRE_ test: disable the feature to keep profile in pre-migration state.
      scoped_feature_list_.InitAndDisableFeature(
          metrics::features::kRestructureMetricsConsentSettings);
    } else {
      // Non-PRE_ test: enable the feature to trigger migration on startup.
      scoped_feature_list_.InitAndEnableFeature(
          metrics::features::kRestructureMetricsConsentSettings);
    }
  }

  MetricsConsentMigrationBrowserTest(
      const MetricsConsentMigrationBrowserTest&) = delete;
  MetricsConsentMigrationBrowserTest& operator=(
      const MetricsConsentMigrationBrowserTest&) = delete;

  ~MetricsConsentMigrationBrowserTest() override = default;

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// 1. Migrate User with All Sync Types Enabled
IN_PROC_BROWSER_TEST_F(MetricsConsentMigrationBrowserTest,
                       PRE_MigrateSignedIn_AllEnabled) {
  metrics::test::MetricsConsentOverride consent(true);
  browser()->GetProfile()->GetPrefs()->SetBoolean(
      unified_consent::prefs::kUrlKeyedAnonymizedDataCollectionEnabled, true);
  ASSERT_TRUE(SetupClients());
  Profile* profile = GetProfile(0);
  std::unique_ptr<SyncServiceImplHarness> harness =
      SyncServiceImplHarness::Create(
          profile, SyncServiceImplHarness::SigninType::FAKE_SIGNIN);
  ASSERT_TRUE(harness->SetupSync());

  // Set MSBB to true.
  profile->GetPrefs()->SetBoolean(
      unified_consent::prefs::kUrlKeyedAnonymizedDataCollectionEnabled, true);

  // Sync is already fully enabled by default.
  // Check that migration has not run yet.
  EXPECT_FALSE(profile->GetPrefs()->GetBoolean(
      metrics::prefs::kAdvancedReportingProfileMigrationDone));
}

IN_PROC_BROWSER_TEST_F(MetricsConsentMigrationBrowserTest,
                       MigrateSignedIn_AllEnabled) {
  metrics::test::MetricsConsentOverride consent(true);
  browser()->GetProfile()->GetPrefs()->SetBoolean(
      unified_consent::prefs::kUrlKeyedAnonymizedDataCollectionEnabled, true);
  ASSERT_TRUE(SetupClients());
  Profile* profile = GetProfile(0);

  std::unique_ptr<SyncServiceImplHarness> harness =
      SyncServiceImplHarness::Create(
          profile, SyncServiceImplHarness::SigninType::FAKE_SIGNIN);
  ASSERT_TRUE(harness->AwaitSyncTransportActive());

  // Migration should run on startup.
  EXPECT_TRUE(profile->GetPrefs()->GetBoolean(
      metrics::prefs::kAdvancedReportingProfileMigrationDone));
  EXPECT_TRUE(profile->GetPrefs()->GetBoolean(
      metrics::prefs::kAdvancedReportingEnabled));

  ukm::UkmTestHelper ukm_test_helper(
      g_browser_process->GetMetricsServicesManager()->GetUkmService());
  EXPECT_TRUE(ukm_test_helper.IsRecordingEnabled());
}

// 2. Migrate User with Apps Sync Disabled
IN_PROC_BROWSER_TEST_F(MetricsConsentMigrationBrowserTest,
                       PRE_MigrateSignedIn_AppsDisabled) {
  metrics::test::MetricsConsentOverride consent(true);
  browser()->GetProfile()->GetPrefs()->SetBoolean(
      unified_consent::prefs::kUrlKeyedAnonymizedDataCollectionEnabled, true);
  ASSERT_TRUE(SetupClients());
  Profile* profile = GetProfile(0);
  std::unique_ptr<SyncServiceImplHarness> harness =
      SyncServiceImplHarness::Create(
          profile, SyncServiceImplHarness::SigninType::FAKE_SIGNIN);
  ASSERT_TRUE(harness->SetupSync());

  // Disable Apps sync.
#if BUILDFLAG(IS_CHROMEOS)
  syncer::UserSelectableOsTypeSet os_types =
      harness->service()->GetUserSettings()->GetSelectedOsTypes();
  os_types.Remove(syncer::UserSelectableOsType::kOsApps);
  harness->service()->GetUserSettings()->SetSelectedOsTypes(
      /*sync_all_os_types=*/false, os_types);
#else
  syncer::UserSelectableTypeSet types =
      harness->service()->GetUserSettings()->GetSelectedTypes();
  types.Remove(syncer::UserSelectableType::kApps);
  harness->service()->GetUserSettings()->SetSelectedTypes(
      /*sync_everything=*/false, types);
#endif

  // Set MSBB to true.
  profile->GetPrefs()->SetBoolean(
      unified_consent::prefs::kUrlKeyedAnonymizedDataCollectionEnabled, true);

  EXPECT_FALSE(profile->GetPrefs()->GetBoolean(
      metrics::prefs::kAdvancedReportingProfileMigrationDone));
}

IN_PROC_BROWSER_TEST_F(MetricsConsentMigrationBrowserTest,
                       MigrateSignedIn_AppsDisabled) {
  metrics::test::MetricsConsentOverride consent(true);
  browser()->GetProfile()->GetPrefs()->SetBoolean(
      unified_consent::prefs::kUrlKeyedAnonymizedDataCollectionEnabled, true);
  ASSERT_TRUE(SetupClients());
  Profile* profile = GetProfile(0);

  std::unique_ptr<SyncServiceImplHarness> harness =
      SyncServiceImplHarness::Create(
          profile, SyncServiceImplHarness::SigninType::FAKE_SIGNIN);
  ASSERT_TRUE(harness->AwaitSyncTransportActive());

  EXPECT_TRUE(profile->GetPrefs()->GetBoolean(
      metrics::prefs::kAdvancedReportingProfileMigrationDone));

  // On desktop platforms and ChromeOS, apps sync is supported but
  // disabled, so migration fails (false).
  EXPECT_FALSE(profile->GetPrefs()->GetBoolean(
      metrics::prefs::kAdvancedReportingEnabled));
  ukm::UkmTestHelper ukm_test_helper(
      g_browser_process->GetMetricsServicesManager()->GetUkmService());
  EXPECT_FALSE(ukm_test_helper.IsRecordingEnabled());
}

// 3. Migrate User with MSBB Disabled
IN_PROC_BROWSER_TEST_F(MetricsConsentMigrationBrowserTest,
                       PRE_MigrateSignedIn_MsbbDisabled) {
  metrics::test::MetricsConsentOverride consent(true);
  browser()->GetProfile()->GetPrefs()->SetBoolean(
      unified_consent::prefs::kUrlKeyedAnonymizedDataCollectionEnabled, true);
  ASSERT_TRUE(SetupClients());
  Profile* profile = GetProfile(0);
  std::unique_ptr<SyncServiceImplHarness> harness =
      SyncServiceImplHarness::Create(
          profile, SyncServiceImplHarness::SigninType::FAKE_SIGNIN);
  ASSERT_TRUE(harness->SetupSync());

  // Set MSBB to false.
  profile->GetPrefs()->SetBoolean(
      unified_consent::prefs::kUrlKeyedAnonymizedDataCollectionEnabled, false);

  EXPECT_FALSE(profile->GetPrefs()->GetBoolean(
      metrics::prefs::kAdvancedReportingProfileMigrationDone));
}

IN_PROC_BROWSER_TEST_F(MetricsConsentMigrationBrowserTest,
                       MigrateSignedIn_MsbbDisabled) {
  metrics::test::MetricsConsentOverride consent(true);
  browser()->GetProfile()->GetPrefs()->SetBoolean(
      unified_consent::prefs::kUrlKeyedAnonymizedDataCollectionEnabled, true);
  ASSERT_TRUE(SetupClients());
  Profile* profile = GetProfile(0);

  std::unique_ptr<SyncServiceImplHarness> harness =
      SyncServiceImplHarness::Create(
          profile, SyncServiceImplHarness::SigninType::FAKE_SIGNIN);
  ASSERT_TRUE(harness->AwaitSyncTransportActive());

  EXPECT_TRUE(profile->GetPrefs()->GetBoolean(
      metrics::prefs::kAdvancedReportingProfileMigrationDone));
  EXPECT_FALSE(profile->GetPrefs()->GetBoolean(
      metrics::prefs::kAdvancedReportingEnabled));

  ukm::UkmTestHelper ukm_test_helper(
      g_browser_process->GetMetricsServicesManager()->GetUkmService());
  EXPECT_FALSE(ukm_test_helper.IsRecordingEnabled());
}

// 4. Migrate Signed-Out User with MSBB Enabled
IN_PROC_BROWSER_TEST_F(MetricsConsentMigrationBrowserTest,
                       PRE_MigrateSignedOut_MsbbEnabled) {
  metrics::test::MetricsConsentOverride consent(true);
  browser()->GetProfile()->GetPrefs()->SetBoolean(
      unified_consent::prefs::kUrlKeyedAnonymizedDataCollectionEnabled, true);
  ASSERT_TRUE(SetupClients());
  Profile* profile = GetProfile(0);
  // Do not sign in.

  // Set MSBB to true.
  profile->GetPrefs()->SetBoolean(
      unified_consent::prefs::kUrlKeyedAnonymizedDataCollectionEnabled, true);

  EXPECT_FALSE(profile->GetPrefs()->GetBoolean(
      metrics::prefs::kAdvancedReportingProfileMigrationDone));
}

IN_PROC_BROWSER_TEST_F(MetricsConsentMigrationBrowserTest,
                       MigrateSignedOut_MsbbEnabled) {
  metrics::test::MetricsConsentOverride consent(true);
  browser()->GetProfile()->GetPrefs()->SetBoolean(
      unified_consent::prefs::kUrlKeyedAnonymizedDataCollectionEnabled, true);
  ASSERT_TRUE(SetupClients());
  Profile* profile = GetProfile(0);

  EXPECT_TRUE(profile->GetPrefs()->GetBoolean(
      metrics::prefs::kAdvancedReportingProfileMigrationDone));
  EXPECT_TRUE(profile->GetPrefs()->GetBoolean(
      metrics::prefs::kAdvancedReportingEnabled));

  ukm::UkmTestHelper ukm_test_helper(
      g_browser_process->GetMetricsServicesManager()->GetUkmService());
  EXPECT_TRUE(ukm_test_helper.IsRecordingEnabled());
}

class MetricsConsentMigrationRollbackBrowserTest : public SyncTest {
 public:
  MetricsConsentMigrationRollbackBrowserTest() : SyncTest(SINGLE_CLIENT) {
    metrics::EnabledStateProvider::SetIgnoreForceFieldTrialsForTesting(true);
    if (content::IsPreTest()) {
      scoped_feature_list_.InitAndEnableFeature(
          metrics::features::kRestructureMetricsConsentSettings);
    } else {
      scoped_feature_list_.InitAndDisableFeature(
          metrics::features::kRestructureMetricsConsentSettings);
    }
  }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(MetricsConsentMigrationRollbackBrowserTest,
                       PRE_MigrationRollback) {
  metrics::test::MetricsConsentOverride consent(true);
  browser()->GetProfile()->GetPrefs()->SetBoolean(
      unified_consent::prefs::kUrlKeyedAnonymizedDataCollectionEnabled, true);
  ASSERT_TRUE(SetupClients());
  Profile* profile = GetProfile(0);
  std::unique_ptr<SyncServiceImplHarness> harness =
      SyncServiceImplHarness::Create(
          profile, SyncServiceImplHarness::SigninType::FAKE_SIGNIN);
  ASSERT_TRUE(harness->SetupSync());

  // Disable Apps sync.
#if BUILDFLAG(IS_CHROMEOS)
  syncer::UserSelectableOsTypeSet os_types =
      harness->service()->GetUserSettings()->GetSelectedOsTypes();
  os_types.Remove(syncer::UserSelectableOsType::kOsApps);
  harness->service()->GetUserSettings()->SetSelectedOsTypes(
      /*sync_all_os_types=*/false, os_types);
#else
  syncer::UserSelectableTypeSet types =
      harness->service()->GetUserSettings()->GetSelectedTypes();
  types.Remove(syncer::UserSelectableType::kApps);
  harness->service()->GetUserSettings()->SetSelectedTypes(
      /*sync_everything=*/false, types);
#endif

  // Set MSBB to true.
  profile->GetPrefs()->SetBoolean(
      unified_consent::prefs::kUrlKeyedAnonymizedDataCollectionEnabled, true);

  // Since the feature is enabled in PRE_ test, migration will run.
  // Since apps sync is disabled, kAdvancedReportingEnabled
  // will migrate to false, which disables UKM.
  EXPECT_TRUE(profile->GetPrefs()->GetBoolean(
      metrics::prefs::kAdvancedReportingProfileMigrationDone));
  EXPECT_FALSE(profile->GetPrefs()->GetBoolean(
      metrics::prefs::kAdvancedReportingEnabled));

  ukm::UkmTestHelper ukm_test_helper(
      g_browser_process->GetMetricsServicesManager()->GetUkmService());
  EXPECT_FALSE(ukm_test_helper.IsRecordingEnabled());
}

IN_PROC_BROWSER_TEST_F(MetricsConsentMigrationRollbackBrowserTest,
                       MigrationRollback) {
  metrics::test::MetricsConsentOverride consent(true);
  browser()->GetProfile()->GetPrefs()->SetBoolean(
      unified_consent::prefs::kUrlKeyedAnonymizedDataCollectionEnabled, true);
  ASSERT_TRUE(SetupClients());
  Profile* profile = GetProfile(0);

  std::unique_ptr<SyncServiceImplHarness> harness =
      SyncServiceImplHarness::Create(
          profile, SyncServiceImplHarness::SigninType::FAKE_SIGNIN);
  ASSERT_TRUE(harness->AwaitSyncTransportActive());

  // Migration had run in the PRE_ test.
  EXPECT_TRUE(profile->GetPrefs()->GetBoolean(
      metrics::prefs::kAdvancedReportingProfileMigrationDone));
  EXPECT_FALSE(profile->GetPrefs()->GetBoolean(
      metrics::prefs::kAdvancedReportingEnabled));

  // The feature is now disabled (rollback).
  // UKM recording should revert to the old logic: MSBB is enabled, so
  // UKM recording should be ENABLED, ignoring that kAdvancedReportingEnabled is
  // false.
  ukm::UkmTestHelper ukm_test_helper(
      g_browser_process->GetMetricsServicesManager()->GetUkmService());
  EXPECT_TRUE(ukm_test_helper.IsRecordingEnabled());
}

// Multi-profile settings tests
class MetricsConsentMigrationMultiProfileBrowserTest : public SyncTest {
 public:
  MetricsConsentMigrationMultiProfileBrowserTest() : SyncTest(SINGLE_CLIENT) {
    metrics::EnabledStateProvider::SetIgnoreForceFieldTrialsForTesting(true);
    if (content::IsPreTest()) {
      scoped_feature_list_.InitAndDisableFeature(
          metrics::features::kRestructureMetricsConsentSettings);
    } else {
      scoped_feature_list_.InitAndEnableFeature(
          metrics::features::kRestructureMetricsConsentSettings);
    }
  }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(MetricsConsentMigrationMultiProfileBrowserTest,
                       PRE_MigrateMultiProfile_AllEnabled) {
  metrics::test::MetricsConsentOverride consent(true);
  browser()->GetProfile()->GetPrefs()->SetBoolean(
      unified_consent::prefs::kUrlKeyedAnonymizedDataCollectionEnabled, true);

  ASSERT_TRUE(SetupClients());

  // Profile 0: Enable MSBB
  Profile* profile_0 = GetProfile(0);
  std::unique_ptr<SyncServiceImplHarness> harness_0 =
      SyncServiceImplHarness::Create(
          profile_0, SyncServiceImplHarness::SigninType::FAKE_SIGNIN);
  ASSERT_TRUE(harness_0->SetupSync());
  profile_0->GetPrefs()->SetBoolean(
      unified_consent::prefs::kUrlKeyedAnonymizedDataCollectionEnabled, true);

  // Profile 1: Enable MSBB
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  base::FilePath user_data_dir;
  base::PathService::Get(chrome::DIR_USER_DATA, &user_data_dir);
  base::FilePath profile_path_1 = user_data_dir.Append(GetProfileBaseName(1));
  Profile& profile_1 =
      profiles::testing::CreateProfileSync(profile_manager, profile_path_1);

  std::unique_ptr<SyncServiceImplHarness> harness_1 =
      SyncServiceImplHarness::Create(
          &profile_1, SyncServiceImplHarness::SigninType::FAKE_SIGNIN);
  ASSERT_TRUE(harness_1->SetupSync());
  profile_1.GetPrefs()->SetBoolean(
      unified_consent::prefs::kUrlKeyedAnonymizedDataCollectionEnabled, true);

  EXPECT_FALSE(profile_0->GetPrefs()->GetBoolean(
      metrics::prefs::kAdvancedReportingProfileMigrationDone));
  EXPECT_FALSE(profile_1.GetPrefs()->GetBoolean(
      metrics::prefs::kAdvancedReportingProfileMigrationDone));
}

IN_PROC_BROWSER_TEST_F(MetricsConsentMigrationMultiProfileBrowserTest,
                       MigrateMultiProfile_AllEnabled) {
  metrics::test::MetricsConsentOverride consent(true);
  browser()->GetProfile()->GetPrefs()->SetBoolean(
      unified_consent::prefs::kUrlKeyedAnonymizedDataCollectionEnabled, true);

  ASSERT_TRUE(SetupClients());
  Profile* profile_0 = GetProfile(0);
  std::unique_ptr<SyncServiceImplHarness> harness_0 =
      SyncServiceImplHarness::Create(
          profile_0, SyncServiceImplHarness::SigninType::FAKE_SIGNIN);
  ASSERT_TRUE(harness_0->AwaitSyncTransportActive());

  ProfileManager* profile_manager = g_browser_process->profile_manager();
  base::FilePath user_data_dir;
  base::PathService::Get(chrome::DIR_USER_DATA, &user_data_dir);
  base::FilePath profile_path_1 = user_data_dir.Append(GetProfileBaseName(1));
  Profile& profile_1 =
      profiles::testing::CreateProfileSync(profile_manager, profile_path_1);

  std::unique_ptr<SyncServiceImplHarness> harness_1 =
      SyncServiceImplHarness::Create(
          &profile_1, SyncServiceImplHarness::SigninType::FAKE_SIGNIN);
  ASSERT_TRUE(harness_1->AwaitSyncTransportActive());

  // Migration should run on both.
  EXPECT_TRUE(profile_0->GetPrefs()->GetBoolean(
      metrics::prefs::kAdvancedReportingProfileMigrationDone));
  EXPECT_TRUE(profile_0->GetPrefs()->GetBoolean(
      metrics::prefs::kAdvancedReportingEnabled));

  EXPECT_TRUE(profile_1.GetPrefs()->GetBoolean(
      metrics::prefs::kAdvancedReportingProfileMigrationDone));
  EXPECT_TRUE(profile_1.GetPrefs()->GetBoolean(
      metrics::prefs::kAdvancedReportingEnabled));

  ukm::UkmTestHelper ukm_test_helper(
      g_browser_process->GetMetricsServicesManager()->GetUkmService());
  EXPECT_TRUE(ukm_test_helper.IsRecordingEnabled());
}

IN_PROC_BROWSER_TEST_F(MetricsConsentMigrationMultiProfileBrowserTest,
                       PRE_MigrateMultiProfile_OneDisabled) {
  metrics::test::MetricsConsentOverride consent(true);
  browser()->GetProfile()->GetPrefs()->SetBoolean(
      unified_consent::prefs::kUrlKeyedAnonymizedDataCollectionEnabled, true);

  ASSERT_TRUE(SetupClients());

  // Profile 0: Enable MSBB
  Profile* profile_0 = GetProfile(0);
  std::unique_ptr<SyncServiceImplHarness> harness_0 =
      SyncServiceImplHarness::Create(
          profile_0, SyncServiceImplHarness::SigninType::FAKE_SIGNIN);
  ASSERT_TRUE(harness_0->SetupSync());
  profile_0->GetPrefs()->SetBoolean(
      unified_consent::prefs::kUrlKeyedAnonymizedDataCollectionEnabled, true);

  // Profile 1: Disable MSBB
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  base::FilePath user_data_dir;
  base::PathService::Get(chrome::DIR_USER_DATA, &user_data_dir);
  base::FilePath profile_path_1 = user_data_dir.Append(GetProfileBaseName(1));
  Profile& profile_1 =
      profiles::testing::CreateProfileSync(profile_manager, profile_path_1);

  std::unique_ptr<SyncServiceImplHarness> harness_1 =
      SyncServiceImplHarness::Create(
          &profile_1, SyncServiceImplHarness::SigninType::FAKE_SIGNIN);
  ASSERT_TRUE(harness_1->SetupSync());
  profile_1.GetPrefs()->SetBoolean(
      unified_consent::prefs::kUrlKeyedAnonymizedDataCollectionEnabled, false);

  EXPECT_FALSE(profile_0->GetPrefs()->GetBoolean(
      metrics::prefs::kAdvancedReportingProfileMigrationDone));
  EXPECT_FALSE(profile_1.GetPrefs()->GetBoolean(
      metrics::prefs::kAdvancedReportingProfileMigrationDone));
}

IN_PROC_BROWSER_TEST_F(MetricsConsentMigrationMultiProfileBrowserTest,
                       MigrateMultiProfile_OneDisabled) {
  metrics::test::MetricsConsentOverride consent(true);
  browser()->GetProfile()->GetPrefs()->SetBoolean(
      unified_consent::prefs::kUrlKeyedAnonymizedDataCollectionEnabled, true);

  ASSERT_TRUE(SetupClients());
  Profile* profile_0 = GetProfile(0);
  std::unique_ptr<SyncServiceImplHarness> harness_0 =
      SyncServiceImplHarness::Create(
          profile_0, SyncServiceImplHarness::SigninType::FAKE_SIGNIN);
  ASSERT_TRUE(harness_0->AwaitSyncTransportActive());

  // Migration should run on Profile 0.
  EXPECT_TRUE(profile_0->GetPrefs()->GetBoolean(
      metrics::prefs::kAdvancedReportingProfileMigrationDone));
  EXPECT_TRUE(profile_0->GetPrefs()->GetBoolean(
      metrics::prefs::kAdvancedReportingEnabled));

  // Verify that UKM is enabled because Profile 0 (the only opened profile) has
  // advanced metrics enabled.
  ukm::UkmTestHelper ukm_test_helper(
      g_browser_process->GetMetricsServicesManager()->GetUkmService());
  EXPECT_TRUE(ukm_test_helper.IsRecordingEnabled());

  // Now open Profile 1, which has already migrated to
  // `kAdvancedReportingEnabled = false` (as configured in the PRE_ test).
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  base::FilePath user_data_dir;
  base::PathService::Get(chrome::DIR_USER_DATA, &user_data_dir);
  base::FilePath profile_path_1 = user_data_dir.Append(GetProfileBaseName(1));
  Profile& profile_1 =
      profiles::testing::CreateProfileSync(profile_manager, profile_path_1);

  std::unique_ptr<SyncServiceImplHarness> harness_1 =
      SyncServiceImplHarness::Create(
          &profile_1, SyncServiceImplHarness::SigninType::FAKE_SIGNIN);
  ASSERT_TRUE(harness_1->AwaitSyncTransportActive());

  EXPECT_TRUE(profile_1.GetPrefs()->GetBoolean(
      metrics::prefs::kAdvancedReportingProfileMigrationDone));
  EXPECT_FALSE(profile_1.GetPrefs()->GetBoolean(
      metrics::prefs::kAdvancedReportingEnabled));

  // Since Profile 1 has advanced reporting false, UKM recording must be
  // DISABLED!
  EXPECT_FALSE(ukm_test_helper.IsRecordingEnabled());
}

// Multi-profile Rollback tests
class MetricsConsentMigrationRollbackMultiProfileBrowserTest : public SyncTest {
 public:
  MetricsConsentMigrationRollbackMultiProfileBrowserTest()
      : SyncTest(TWO_CLIENT) {
    metrics::EnabledStateProvider::SetIgnoreForceFieldTrialsForTesting(true);
    if (content::IsPreTest()) {
      scoped_feature_list_.InitAndEnableFeature(
          metrics::features::kRestructureMetricsConsentSettings);
    } else {
      scoped_feature_list_.InitAndDisableFeature(
          metrics::features::kRestructureMetricsConsentSettings);
    }
  }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(MetricsConsentMigrationRollbackMultiProfileBrowserTest,
                       PRE_MigrationRollback_MultiProfile_AllEnabled) {
  metrics::test::MetricsConsentOverride consent(true);
  browser()->GetProfile()->GetPrefs()->SetBoolean(
      unified_consent::prefs::kUrlKeyedAnonymizedDataCollectionEnabled, true);

  ASSERT_TRUE(SetupClients());

  // Profile 0: Enable MSBB
  Profile* profile_0 = GetProfile(0);
  std::unique_ptr<SyncServiceImplHarness> harness_0 =
      SyncServiceImplHarness::Create(
          profile_0, SyncServiceImplHarness::SigninType::FAKE_SIGNIN);
  ASSERT_TRUE(harness_0->SetupSync());
  profile_0->GetPrefs()->SetBoolean(
      unified_consent::prefs::kUrlKeyedAnonymizedDataCollectionEnabled, true);

  // Profile 1: Enable MSBB
  Profile* profile_1 = GetProfile(1);
  std::unique_ptr<SyncServiceImplHarness> harness_1 =
      SyncServiceImplHarness::Create(
          profile_1, SyncServiceImplHarness::SigninType::FAKE_SIGNIN);
  ASSERT_TRUE(harness_1->SetupSync());
  profile_1->GetPrefs()->SetBoolean(
      unified_consent::prefs::kUrlKeyedAnonymizedDataCollectionEnabled, true);
}

IN_PROC_BROWSER_TEST_F(MetricsConsentMigrationRollbackMultiProfileBrowserTest,
                       MigrationRollback_MultiProfile_AllEnabled) {
  metrics::test::MetricsConsentOverride consent(true);
  browser()->GetProfile()->GetPrefs()->SetBoolean(
      unified_consent::prefs::kUrlKeyedAnonymizedDataCollectionEnabled, true);

  ASSERT_TRUE(SetupClients());
  Profile* profile_0 = GetProfile(0);
  std::unique_ptr<SyncServiceImplHarness> harness_0 =
      SyncServiceImplHarness::Create(
          profile_0, SyncServiceImplHarness::SigninType::FAKE_SIGNIN);
  ASSERT_TRUE(harness_0->AwaitSyncTransportActive());

  Profile* profile_1 = GetProfile(1);
  std::unique_ptr<SyncServiceImplHarness> harness_1 =
      SyncServiceImplHarness::Create(
          profile_1, SyncServiceImplHarness::SigninType::FAKE_SIGNIN);
  ASSERT_TRUE(harness_1->AwaitSyncTransportActive());

  // Since feature flag is disabled, advanced reporting prefs don't matter.
  // UKM should be allowed because both profiles have MSBB enabled (under old
  // logic).
  ukm::UkmTestHelper ukm_test_helper(
      g_browser_process->GetMetricsServicesManager()->GetUkmService());
  EXPECT_TRUE(ukm_test_helper.IsRecordingEnabled());
}

IN_PROC_BROWSER_TEST_F(MetricsConsentMigrationRollbackMultiProfileBrowserTest,
                       PRE_MigrationRollback_MultiProfile_OneDisabled) {
  metrics::test::MetricsConsentOverride consent(true);
  browser()->GetProfile()->GetPrefs()->SetBoolean(
      unified_consent::prefs::kUrlKeyedAnonymizedDataCollectionEnabled, true);

  ASSERT_TRUE(SetupClients());

  // Profile 0: Enable MSBB
  Profile* profile_0 = GetProfile(0);
  std::unique_ptr<SyncServiceImplHarness> harness_0 =
      SyncServiceImplHarness::Create(
          profile_0, SyncServiceImplHarness::SigninType::FAKE_SIGNIN);
  ASSERT_TRUE(harness_0->SetupSync());
  profile_0->GetPrefs()->SetBoolean(
      unified_consent::prefs::kUrlKeyedAnonymizedDataCollectionEnabled, true);

  // Profile 1: Disable MSBB
  Profile* profile_1 = GetProfile(1);
  std::unique_ptr<SyncServiceImplHarness> harness_1 =
      SyncServiceImplHarness::Create(
          profile_1, SyncServiceImplHarness::SigninType::FAKE_SIGNIN);
  ASSERT_TRUE(harness_1->SetupSync());
  profile_1->GetPrefs()->SetBoolean(
      unified_consent::prefs::kUrlKeyedAnonymizedDataCollectionEnabled, false);
}

IN_PROC_BROWSER_TEST_F(MetricsConsentMigrationRollbackMultiProfileBrowserTest,
                       MigrationRollback_MultiProfile_OneDisabled) {
  metrics::test::MetricsConsentOverride consent(true);
  browser()->GetProfile()->GetPrefs()->SetBoolean(
      unified_consent::prefs::kUrlKeyedAnonymizedDataCollectionEnabled, true);

  ASSERT_TRUE(SetupClients());
  Profile* profile_0 = GetProfile(0);
  std::unique_ptr<SyncServiceImplHarness> harness_0 =
      SyncServiceImplHarness::Create(
          profile_0, SyncServiceImplHarness::SigninType::FAKE_SIGNIN);
  ASSERT_TRUE(harness_0->AwaitSyncTransportActive());

  Profile* profile_1 = GetProfile(1);
  std::unique_ptr<SyncServiceImplHarness> harness_1 =
      SyncServiceImplHarness::Create(
          profile_1, SyncServiceImplHarness::SigninType::FAKE_SIGNIN);
  ASSERT_TRUE(harness_1->AwaitSyncTransportActive());

  // Since Profile 1 has MSBB disabled, UKM must be disabled under old logic.
  ukm::UkmTestHelper ukm_test_helper(
      g_browser_process->GetMetricsServicesManager()->GetUkmService());
  EXPECT_FALSE(ukm_test_helper.IsRecordingEnabled());
}
