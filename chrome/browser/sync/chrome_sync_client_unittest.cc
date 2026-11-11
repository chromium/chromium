// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/chrome_sync_client.h"

#include "base/files/file_path.h"
#include "base/json/values_util.h"
#include "build/build_config.h"
#include "chrome/browser/sync/glue/extensions_activity_monitor.h"
#include "chrome/test/base/scoped_metrics_service_for_synthetic_trials.h"
#include "chrome/test/base/testing_browser_process.h"
#include "components/metrics/metrics_service.h"
#include "components/prefs/testing_pref_service.h"
#include "components/prefs/testing_pref_store.h"
#include "components/sync/base/pref_names.h"
#include "components/sync/protocol/nigori_specifics.pb.h"
#include "components/sync/service/sync_prefs.h"
#include "components/sync/service/trusted_vault_synthetic_field_trial.h"
#include "components/sync/test/test_data_type_store_service.h"
#include "components/sync_device_info/fake_device_info_sync_service.h"
#include "components/variations/active_field_trials.h"
#include "components/variations/synthetic_trial_registry.h"
#include "components/variations/variations_test_utils.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

syncer::TrustedVaultAutoUpgradeSyntheticFieldTrialGroup BuildExperimentGroup(
    int cohort,
    sync_pb::TrustedVaultAutoUpgradeExperimentGroup::Type type,
    int type_index) {
  sync_pb::TrustedVaultAutoUpgradeExperimentGroup proto;
  proto.set_cohort(cohort);
  proto.set_type(type);
  proto.set_type_index(type_index);
  return syncer::TrustedVaultAutoUpgradeSyntheticFieldTrialGroup::FromProto(
      proto);
}

class ChromeSyncClientTest : public testing::Test {
 protected:
  ChromeSyncClientTest() {
    syncer::SyncPrefs::RegisterProfilePrefs(pref_service_.registry());
  }
  ~ChromeSyncClientTest() override = default;

  std::unique_ptr<ChromeSyncClient> BuildClient(
      const base::FilePath& profile_base_name) {
    return std::make_unique<ChromeSyncClient>(
        profile_base_name, &pref_service_,
        /*identity_manager=*/nullptr,
        /*trusted_vault_service=*/nullptr,
        /*sync_invalidations_service=*/nullptr, &device_info_sync_service_,
        &data_type_store_service_,
        /*supervised_user_settings_service=*/nullptr,
        /*extensions_activity_monitor=*/nullptr);
  }

  std::vector<variations::ActiveGroupId> GetSyntheticFieldTrials() {
    return metrics_service_.Get()
        ->GetSyntheticTrialRegistry()
        ->GetCurrentSyntheticFieldTrialsForTest();
  }

  TestingPrefServiceSimple& pref_service() { return pref_service_; }

 private:
  content::BrowserTaskEnvironment task_environment_;
  TestingPrefServiceSimple pref_service_;
  ScopedMetricsServiceForSyntheticTrials metrics_service_{
      TestingBrowserProcess::GetGlobal()};
  syncer::FakeDeviceInfoSyncService device_info_sync_service_;
  syncer::TestDataTypeStoreService data_type_store_service_;
};

TEST_F(ChromeSyncClientTest, ShouldRegisterSyntheticFieldTrial) {
  std::unique_ptr<ChromeSyncClient> client =
      BuildClient(base::FilePath(FILE_PATH_LITERAL("profile1")));

  client->RegisterTrustedVaultAutoUpgradeSyntheticFieldTrial(
      BuildExperimentGroup(
          /*cohort=*/6,
          sync_pb::TrustedVaultAutoUpgradeExperimentGroup::CONTROL,
          /*type_index=*/0));

  EXPECT_TRUE(ContainsTrialAndGroupName(
      GetSyntheticFieldTrials(),
      syncer::kTrustedVaultAutoUpgradeSyntheticFieldTrialName,
      "Cohort6_Control"));
}

TEST_F(ChromeSyncClientTest,
       ShouldIgnoreRepeatedRegistrationOfSameSyntheticFieldTrial) {
  const base::FilePath profile_base_name(FILE_PATH_LITERAL("profile1"));
  const syncer::TrustedVaultAutoUpgradeSyntheticFieldTrialGroup group =
      BuildExperimentGroup(
          /*cohort=*/6,
          sync_pb::TrustedVaultAutoUpgradeExperimentGroup::CONTROL,
          /*type_index=*/0);

  std::unique_ptr<ChromeSyncClient> client1 = BuildClient(profile_base_name);
  client1->RegisterTrustedVaultAutoUpgradeSyntheticFieldTrial(group);

  ASSERT_TRUE(ContainsTrialAndGroupName(
      GetSyntheticFieldTrials(),
      syncer::kTrustedVaultAutoUpgradeSyntheticFieldTrialName,
      "Cohort6_Control"));

  // Note that the second instance uses the same profile name.
  std::unique_ptr<ChromeSyncClient> client2 = BuildClient(profile_base_name);
  client2->RegisterTrustedVaultAutoUpgradeSyntheticFieldTrial(group);

  EXPECT_TRUE(ContainsTrialAndGroupName(
      GetSyntheticFieldTrials(),
      syncer::kTrustedVaultAutoUpgradeSyntheticFieldTrialName,
      "Cohort6_Control"));
}

TEST_F(ChromeSyncClientTest,
       ShouldReportSyntheticFieldTrialMultiProfileConflict) {
  const syncer::TrustedVaultAutoUpgradeSyntheticFieldTrialGroup group =
      BuildExperimentGroup(
          /*cohort=*/6,
          sync_pb::TrustedVaultAutoUpgradeExperimentGroup::CONTROL,
          /*type_index=*/0);

  BuildClient(base::FilePath(FILE_PATH_LITERAL("profile1")))
      ->RegisterTrustedVaultAutoUpgradeSyntheticFieldTrial(group);

  ASSERT_TRUE(ContainsTrialAndGroupName(
      GetSyntheticFieldTrials(),
      syncer::kTrustedVaultAutoUpgradeSyntheticFieldTrialName,
      "Cohort6_Control"));

  // If a different profile gets loaded that also registers a trial, it should
  // be interpreted as a conflict.
  BuildClient(base::FilePath(FILE_PATH_LITERAL("profile2")))
      ->RegisterTrustedVaultAutoUpgradeSyntheticFieldTrial(group);

  EXPECT_TRUE(ContainsTrialAndGroupName(
      GetSyntheticFieldTrials(),
      syncer::kTrustedVaultAutoUpgradeSyntheticFieldTrialName,
      "MultiProfileConflict"));
}

TEST_F(ChromeSyncClientTest,
       ShouldIgnoreSyntheticFieldTrialsOnceMultiProfileConflictDetected) {
  const syncer::TrustedVaultAutoUpgradeSyntheticFieldTrialGroup group =
      BuildExperimentGroup(
          /*cohort=*/6,
          sync_pb::TrustedVaultAutoUpgradeExperimentGroup::CONTROL,
          /*type_index=*/0);

  BuildClient(base::FilePath(FILE_PATH_LITERAL("profile1")))
      ->RegisterTrustedVaultAutoUpgradeSyntheticFieldTrial(group);
  BuildClient(base::FilePath(FILE_PATH_LITERAL("profile2")))
      ->RegisterTrustedVaultAutoUpgradeSyntheticFieldTrial(group);

  ASSERT_TRUE(ContainsTrialAndGroupName(
      GetSyntheticFieldTrials(),
      syncer::kTrustedVaultAutoUpgradeSyntheticFieldTrialName,
      "MultiProfileConflict"));

  // No matter what happens afterwards, the multi-profile conflict should remain
  // unmodified.
  BuildClient(base::FilePath(FILE_PATH_LITERAL("profile2")))
      ->RegisterTrustedVaultAutoUpgradeSyntheticFieldTrial(group);
  EXPECT_TRUE(ContainsTrialAndGroupName(
      GetSyntheticFieldTrials(),
      syncer::kTrustedVaultAutoUpgradeSyntheticFieldTrialName,
      "MultiProfileConflict"));

  BuildClient(base::FilePath(FILE_PATH_LITERAL("profile1")))
      ->RegisterTrustedVaultAutoUpgradeSyntheticFieldTrial(group);
  EXPECT_TRUE(ContainsTrialAndGroupName(
      GetSyntheticFieldTrials(),
      syncer::kTrustedVaultAutoUpgradeSyntheticFieldTrialName,
      "MultiProfileConflict"));

  BuildClient(base::FilePath(FILE_PATH_LITERAL("profile3")))
      ->RegisterTrustedVaultAutoUpgradeSyntheticFieldTrial(group);
  EXPECT_TRUE(ContainsTrialAndGroupName(
      GetSyntheticFieldTrials(),
      syncer::kTrustedVaultAutoUpgradeSyntheticFieldTrialName,
      "MultiProfileConflict"));
}

TEST_F(ChromeSyncClientTest,
       GetLocalSyncBackendFolderOnlyHonorsPolicyAndCommandLine) {
  const base::FilePath profile_base_name(FILE_PATH_LITERAL("Default"));
  const base::FilePath configured_dir(FILE_PATH_LITERAL("/tmp/local_sync_dir"));
#if BUILDFLAG(IS_WIN)
  const base::FilePath expected_configured_path =
      configured_dir.Append(profile_base_name)
          .Append(FILE_PATH_LITERAL("profile.pb"));
#else
  const base::FilePath expected_configured_path = configured_dir;
#endif  // BUILDFLAG(IS_WIN)

  std::unique_ptr<ChromeSyncClient> client = BuildClient(profile_base_name);
  // On Windows, `GetLocalSyncBackendFolder()` falls back to
  // `DIR_ROAMING_USER_DATA` when `kLocalSyncBackendDir` is unconfigured rather
  // than returning an empty path.
  const base::FilePath default_folder = client->GetLocalSyncBackendFolder();
  ASSERT_NE(default_folder, expected_configured_path);
#if !BUILDFLAG(IS_WIN)
  ASSERT_TRUE(default_folder.empty());
#endif  // !BUILDFLAG(IS_WIN)

  // Values in user, recommended, extension, or supervised user stores should
  // be ignored.
  pref_service().SetUserPref(syncer::prefs::kLocalSyncBackendDir,
                             base::FilePathToValue(configured_dir));
  EXPECT_EQ(default_folder, client->GetLocalSyncBackendFolder());
  pref_service().RemoveUserPref(syncer::prefs::kLocalSyncBackendDir);

  pref_service().SetRecommendedPref(syncer::prefs::kLocalSyncBackendDir,
                                    base::FilePathToValue(configured_dir));
  EXPECT_EQ(default_folder, client->GetLocalSyncBackendFolder());
  pref_service().RemoveRecommendedPref(syncer::prefs::kLocalSyncBackendDir);

  pref_service().SetExtensionPref(syncer::prefs::kLocalSyncBackendDir,
                                  base::FilePathToValue(configured_dir));
  EXPECT_EQ(default_folder, client->GetLocalSyncBackendFolder());
  pref_service().RemoveExtensionPref(syncer::prefs::kLocalSyncBackendDir);

  pref_service().SetSupervisedUserPref(syncer::prefs::kLocalSyncBackendDir,
                                       base::FilePathToValue(configured_dir));
  EXPECT_EQ(default_folder, client->GetLocalSyncBackendFolder());
  pref_service().RemoveSupervisedUserPref(syncer::prefs::kLocalSyncBackendDir);

  // Command-line store should be honored.
  auto command_line_store = base::MakeRefCounted<TestingPrefStore>();
  command_line_store->SetInitializationCompleted();
  command_line_store->SetValue(syncer::prefs::kLocalSyncBackendDir,
                               base::FilePathToValue(configured_dir),
                               WriteablePrefStore::DEFAULT_PREF_WRITE_FLAGS);
  pref_service().UpdateCommandLinePrefStore(command_line_store);
  EXPECT_EQ(expected_configured_path, client->GetLocalSyncBackendFolder());
  pref_service().UpdateCommandLinePrefStore(
      base::MakeRefCounted<TestingPrefStore>());

  // Managed policy store should be honored.
  pref_service().SetManagedPref(syncer::prefs::kLocalSyncBackendDir,
                                base::FilePathToValue(configured_dir));
  EXPECT_EQ(expected_configured_path, client->GetLocalSyncBackendFolder());
}

}  // namespace
