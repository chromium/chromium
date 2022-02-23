// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crosapi/browser_data_migrator.h"

#include <string>

#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/version.h"
#include "chrome/browser/ash/crosapi/browser_data_migrator_util.h"
#include "chrome/browser/ash/crosapi/browser_util.h"
#include "chrome/browser/ash/crosapi/fake_migration_progress_tracker.h"
#include "components/prefs/testing_pref_service.h"
#include "components/version_info/version_info.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace ash {

namespace {
constexpr char kFirstRun[] = "First Run";
}  // namespace

class BrowserDataMigratorImplTest : public ::testing::Test {
 public:
  void SetUp() override {
    // Setup `user_data_dir_` as below.
    // ./                             /* user_data_dir_ */
    // |- user/                       /* from_dir_ */
    //     |- data

    ASSERT_TRUE(user_data_dir_.CreateUniqueTempDir());
    from_dir_ = user_data_dir_.GetPath().Append("user");

    ASSERT_TRUE(base::CreateDirectory(from_dir_));

    BrowserDataMigratorImpl::RegisterLocalStatePrefs(pref_service_.registry());
    crosapi::browser_util::RegisterLocalStatePrefs(pref_service_.registry());
  }

  void TearDown() override { EXPECT_TRUE(user_data_dir_.Delete()); }

  base::ScopedTempDir user_data_dir_;
  base::FilePath from_dir_;
  TestingPrefServiceSimple pref_service_;
};

TEST_F(BrowserDataMigratorImplTest, ManipulateMigrationAttemptCount) {
  const std::string user_id_hash = "user";

  EXPECT_EQ(BrowserDataMigratorImpl::GetMigrationAttemptCountForUser(
                &pref_service_, user_id_hash),
            0);
  BrowserDataMigratorImpl::UpdateMigrationAttemptCountForUser(&pref_service_,
                                                              user_id_hash);
  EXPECT_EQ(BrowserDataMigratorImpl::GetMigrationAttemptCountForUser(
                &pref_service_, user_id_hash),
            1);

  BrowserDataMigratorImpl::UpdateMigrationAttemptCountForUser(&pref_service_,
                                                              user_id_hash);
  EXPECT_EQ(BrowserDataMigratorImpl::GetMigrationAttemptCountForUser(
                &pref_service_, user_id_hash),
            2);

  BrowserDataMigratorImpl::ClearMigrationAttemptCountForUser(&pref_service_,
                                                             user_id_hash);
  EXPECT_EQ(BrowserDataMigratorImpl::GetMigrationAttemptCountForUser(
                &pref_service_, user_id_hash),
            0);
}

TEST_F(BrowserDataMigratorImplTest, Migrate) {
  base::test::TaskEnvironment task_environment;
  std::unique_ptr<MigrationProgressTracker> progress_tracker =
      std::make_unique<FakeMigrationProgressTracker>();
  const std::string user_id_hash = "abcd";
  BrowserDataMigratorImpl::SetMigrationStep(
      &pref_service_, BrowserDataMigratorImpl::MigrationStep::kRestartCalled);
  // Set migration attempt count to 1.
  BrowserDataMigratorImpl::UpdateMigrationAttemptCountForUser(&pref_service_,
                                                              user_id_hash);

  base::RunLoop run_loop;
  std::unique_ptr<BrowserDataMigratorImpl> migrator =
      std::make_unique<BrowserDataMigratorImpl>(
          from_dir_, user_id_hash, base::DoNothing(), &pref_service_);
  absl::optional<BrowserDataMigrator::Result> result;
  migrator->Migrate(base::BindLambdaForTesting(
      [&out_result = result, &run_loop](BrowserDataMigrator::Result result) {
        run_loop.Quit();
        out_result = result;
      }));
  run_loop.Run();

  const base::FilePath new_user_data_dir =
      from_dir_.Append(browser_data_migrator_util::kLacrosDir);
  const base::FilePath new_profile_data_dir =
      new_user_data_dir.Append("Default");
  // Check that `First Run` file is created inside the new data directory.
  EXPECT_TRUE(base::PathExists(new_user_data_dir.Append(kFirstRun)));
  // Check that migration is marked as completed for the user.
  EXPECT_TRUE(crosapi::browser_util::IsProfileMigrationCompletedForUser(
      &pref_service_, user_id_hash));
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(BrowserDataMigrator::ResultKind::kSucceeded, result->kind);
  EXPECT_EQ(BrowserDataMigratorImpl::GetMigrationStep(&pref_service_),
            BrowserDataMigratorImpl::MigrationStep::kEnded);
  // Successful migration should clear the migration attempt count.
  EXPECT_EQ(BrowserDataMigratorImpl::GetMigrationAttemptCountForUser(
                &pref_service_, user_id_hash),
            0);
  // Data version should be updated to the current version after a migration.
  EXPECT_EQ(crosapi::browser_util::GetDataVer(&pref_service_, user_id_hash),
            version_info::GetVersion());
}

TEST_F(BrowserDataMigratorImplTest, MigrateCancelled) {
  base::test::TaskEnvironment task_environment;
  std::unique_ptr<MigrationProgressTracker> progress_tracker =
      std::make_unique<FakeMigrationProgressTracker>();
  const std::string user_id_hash = "abcd";
  BrowserDataMigratorImpl::SetMigrationStep(
      &pref_service_, BrowserDataMigratorImpl::MigrationStep::kRestartCalled);
  // Set migration attempt count to 1.
  BrowserDataMigratorImpl::UpdateMigrationAttemptCountForUser(&pref_service_,
                                                              user_id_hash);

  base::RunLoop run_loop;
  std::unique_ptr<BrowserDataMigratorImpl> migrator =
      std::make_unique<BrowserDataMigratorImpl>(
          from_dir_, user_id_hash, base::DoNothing(), &pref_service_);
  absl::optional<BrowserDataMigrator::Result> result;
  migrator->Migrate(base::BindLambdaForTesting(
      [&out_result = result, &run_loop](BrowserDataMigrator::Result result) {
        run_loop.Quit();
        out_result = result;
      }));
  migrator->Cancel();
  run_loop.Run();

  const base::FilePath new_user_data_dir =
      from_dir_.Append(browser_data_migrator_util::kLacrosDir);
  const base::FilePath new_profile_data_dir =
      new_user_data_dir.Append("Default");
  EXPECT_FALSE(base::PathExists(new_user_data_dir.Append(kFirstRun)));
  EXPECT_FALSE(crosapi::browser_util::IsProfileMigrationCompletedForUser(
      &pref_service_, user_id_hash));
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(BrowserDataMigrator::ResultKind::kCancelled, result->kind);
  EXPECT_EQ(BrowserDataMigratorImpl::GetMigrationStep(&pref_service_),
            BrowserDataMigratorImpl::MigrationStep::kEnded);
  // If migration fails, migration attempt count should not be cleared thus
  // should remain as 1.
  EXPECT_EQ(BrowserDataMigratorImpl::GetMigrationAttemptCountForUser(
                &pref_service_, user_id_hash),
            1);
  // Even if migration is cancelled, lacros data dir is cleared and thus data
  // version should be updated.
  EXPECT_EQ(crosapi::browser_util::GetDataVer(&pref_service_, user_id_hash),
            version_info::GetVersion());
}

TEST_F(BrowserDataMigratorImplTest, MigrateOutOfDiskForCopy) {
  // Emulate the situation of out-of-disk.
  browser_data_migrator_util::ScopedExtraBytesRequiredToBeFreedForTesting
      scoped_extra_bytes(100);

  base::test::TaskEnvironment task_environment;
  const std::string user_id_hash = "abcd";
  BrowserDataMigratorImpl::SetMigrationStep(
      &pref_service_, BrowserDataMigratorImpl::MigrationStep::kRestartCalled);
  // Set migration attempt count to 1.
  BrowserDataMigratorImpl::UpdateMigrationAttemptCountForUser(&pref_service_,
                                                              user_id_hash);

  base::RunLoop run_loop;
  std::unique_ptr<BrowserDataMigratorImpl> migrator =
      std::make_unique<BrowserDataMigratorImpl>(
          from_dir_, user_id_hash, base::DoNothing(), &pref_service_);
  absl::optional<BrowserDataMigrator::Result> result;
  migrator->Migrate(base::BindLambdaForTesting(
      [&out_result = result, &run_loop](BrowserDataMigrator::Result result) {
        run_loop.Quit();
        out_result = result;
      }));
  run_loop.Run();

  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(BrowserDataMigrator::ResultKind::kFailed, result->kind);
  // |required_size| should carry the data.
  EXPECT_EQ(100u, result->required_size);
}

TEST_F(BrowserDataMigratorImplTest, MigrateOutOfDiskForMove) {
  base::test::ScopedFeatureList feature_list(kLacrosMoveProfileMigration);

  // Emulate the situation of out-of-disk.
  browser_data_migrator_util::ScopedExtraBytesRequiredToBeFreedForTesting
      scoped_extra_bytes(100);

  base::test::TaskEnvironment task_environment;
  const std::string user_id_hash = "abcd";
  BrowserDataMigratorImpl::SetMigrationStep(
      &pref_service_, BrowserDataMigratorImpl::MigrationStep::kRestartCalled);
  // Set migration attempt count to 1.
  BrowserDataMigratorImpl::UpdateMigrationAttemptCountForUser(&pref_service_,
                                                              user_id_hash);

  base::RunLoop run_loop;
  std::unique_ptr<BrowserDataMigratorImpl> migrator =
      std::make_unique<BrowserDataMigratorImpl>(
          from_dir_, user_id_hash, base::DoNothing(), &pref_service_);
  absl::optional<BrowserDataMigrator::Result> result;
  migrator->Migrate(base::BindLambdaForTesting(
      [&out_result = result, &run_loop](BrowserDataMigrator::Result result) {
        run_loop.Quit();
        out_result = result;
      }));
  run_loop.Run();

  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(BrowserDataMigrator::ResultKind::kFailed, result->kind);
  // |required_size| should carry the data.
  EXPECT_EQ(100u, result->required_size);
}

}  // namespace ash
