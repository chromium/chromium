// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/account_password_store_factory.h"

#include "base/test/metrics/histogram_tester.h"
#include "build/build_config.h"
#include "chrome/browser/prefs/browser_prefs.h"
#include "chrome/test/base/testing_profile.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "components/sync/base/data_type_histogram.h"
#include "components/sync/base/pref_names.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

#if !BUILDFLAG(IS_ANDROID)

namespace {

class AccountPasswordStoreFactoryTest : public testing::Test {
 public:
  void InitProfile(bool enable_migration_pref) {
    TestingProfile::Builder builder;
    builder.AddTestingFactory(
        AccountPasswordStoreFactory::GetInstance(),
        AccountPasswordStoreFactory::GetDefaultFactoryForTesting());

    auto pref_service =
        std::make_unique<sync_preferences::TestingPrefServiceSyncable>();
    RegisterUserProfilePrefs(pref_service->registry());
    pref_service->SetBoolean(
        syncer::prefs::kCleanUpStatsTableFromAccountPasswordStore,
        enable_migration_pref);
    builder.SetPrefService(std::move(pref_service));

    profile_ = builder.Build();
  }

  TestingProfile* profile() { return profile_.get(); }

 private:
  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<TestingProfile> profile_;
};

TEST_F(AccountPasswordStoreFactoryTest, ClearsStatsTableWhenPrefIsSet) {
  base::HistogramTester histogram_tester;

  InitProfile(true);

  // Initialize the AccountPasswordStore.
  scoped_refptr<password_manager::PasswordStoreInterface> store =
      AccountPasswordStoreFactory::GetForProfile(
          profile(), ServiceAccessType::EXPLICIT_ACCESS);
  ASSERT_TRUE(store);

  // The pref should still be true here, as the clearing task is async.
  // Wait, if it's async, we can check.
  // However, the histogram is recorded synchronously.

  histogram_tester.ExpectBucketCount(
      "Sync.SyncToSigninMigration.StatsTableCleanupStep",
      syncer::SyncToSigninMigrationStatsTableCleanupStep::kCleanupStarted, 1);

  // Verify that the pref is cleared.
  EXPECT_FALSE(profile()->GetPrefs()->GetBoolean(
      syncer::prefs::kCleanUpStatsTableFromAccountPasswordStore));
  histogram_tester.ExpectBucketCount(
      "Sync.SyncToSigninMigration.StatsTableCleanupStep",
      syncer::SyncToSigninMigrationStatsTableCleanupStep::
          kCleanupFinishedAndPrefCleared,
      1);
  histogram_tester.ExpectTotalCount(
      "Sync.SyncToSigninMigration.StatsTableCleanupStep", 2);
  EXPECT_THAT(
      histogram_tester.GetAllSamples(
          "Sync.SyncToSigninMigration.StatsTableCleanupStep"),
      ::testing::UnorderedElementsAre(
          base::Bucket(syncer::SyncToSigninMigrationStatsTableCleanupStep::
                           kCleanupStarted,
                       1),
          base::Bucket(syncer::SyncToSigninMigrationStatsTableCleanupStep::
                           kCleanupFinishedAndPrefCleared,
                       1)));
}

TEST_F(AccountPasswordStoreFactoryTest,
       DoesNotClearStatsTableWhenPrefIsNotSet) {
  base::HistogramTester histogram_tester;

  InitProfile(false);

  // Initialize the AccountPasswordStore.
  scoped_refptr<password_manager::PasswordStoreInterface> store =
      AccountPasswordStoreFactory::GetForProfile(
          profile(), ServiceAccessType::EXPLICIT_ACCESS);
  ASSERT_TRUE(store);

  // Verify that the pref remains false.
  EXPECT_FALSE(profile()->GetPrefs()->GetBoolean(
      syncer::prefs::kCleanUpStatsTableFromAccountPasswordStore));

  histogram_tester.ExpectTotalCount(
      "Sync.SyncToSigninMigration.StatsTableCleanupStep", 0);
}

}  // namespace

#endif  // !BUILDFLAG(IS_ANDROID)
