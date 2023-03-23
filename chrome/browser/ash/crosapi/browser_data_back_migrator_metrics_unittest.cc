// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crosapi/browser_data_back_migrator_metrics.h"

#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/test/metrics/histogram_tester.h"
#include "chrome/common/chrome_constants.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

namespace {

void SetUpProfileDirectories(const base::FilePath& lacros_dir) {
  // Set up the contents of lacros to mimic several secondary profile
  // directories and some other irrelevant files and directories.
  // |- user
  //     |- lacros
  //         |- Default
  //         |- Profile 3     /* directory */
  //         |- Profile 9     /* file */
  //         |- Profile 14
  //         |- Profile 824
  //         |- Profile w/o number
  //         |- Other directory

  std::string profile_dir_prefix = chrome::kMultiProfileDirPrefix;

  ASSERT_TRUE(
      base::CreateDirectory(lacros_dir.Append(profile_dir_prefix.append("3"))));
  ASSERT_TRUE(base::CreateDirectory(
      lacros_dir.Append(profile_dir_prefix.append("14"))));
  ASSERT_TRUE(base::CreateDirectory(
      lacros_dir.Append(profile_dir_prefix.append("824"))));
  ASSERT_TRUE(base::CreateDirectory(
      lacros_dir.Append(profile_dir_prefix.append("w/o number"))));
  ASSERT_TRUE(base::CreateDirectory(lacros_dir.Append("Other directory")));

  ASSERT_TRUE(
      base::WriteFile(lacros_dir.Append("Profile 9"), base::StringPiece()));
}

}  // namespace

class BrowserDataBackMigratorMetricsFixtureTest : public testing::Test {
 public:
  void SetUp() override {
    ASSERT_TRUE(user_data_dir_.CreateUniqueTempDir());

    ash_profile_dir_ = user_data_dir_.GetPath().Append("user");

    lacros_dir_ =
        ash_profile_dir_.Append(browser_data_migrator_util::kLacrosDir);
  }

  base::ScopedTempDir user_data_dir_;
  base::FilePath ash_profile_dir_;
  base::FilePath lacros_dir_;
};

TEST(BrowserDataBackMigratorMetricsTest, RecordFinalStatus) {
  base::HistogramTester histogram_tester;

  BrowserDataBackMigrator::TaskResult success = {
      BrowserDataBackMigrator::TaskStatus::kSucceeded};
  browser_data_back_migrator_metrics::RecordFinalStatus(success);

  histogram_tester.ExpectUniqueSample(
      browser_data_back_migrator_metrics::kFinalStatusUMA,
      static_cast<base::HistogramBase::Sample>(
          BrowserDataBackMigrator::TaskStatus::kSucceeded),
      1);
  histogram_tester.ExpectTotalCount(
      browser_data_back_migrator_metrics::kFinalStatusUMA, 1);

  BrowserDataBackMigrator::TaskResult failure = {
      BrowserDataBackMigrator::TaskStatus::kDeleteTmpDirDeleteFailed, EPERM};
  browser_data_back_migrator_metrics::RecordFinalStatus(failure);

  histogram_tester.ExpectBucketCount(
      browser_data_back_migrator_metrics::kFinalStatusUMA,
      static_cast<base::HistogramBase::Sample>(
          BrowserDataBackMigrator::TaskStatus::kDeleteTmpDirDeleteFailed),
      1);
  histogram_tester.ExpectTotalCount(
      browser_data_back_migrator_metrics::kFinalStatusUMA, 2);
}

TEST(BrowserDataBackMigratorMetricsTest, RecordPosixErrnoIfAvailable) {
  base::HistogramTester histogram_tester;
  auto task_status =
      BrowserDataBackMigrator::TaskStatus::kDeleteTmpDirDeleteFailed;
  std::string uma_name =
      browser_data_back_migrator_metrics::kPosixErrnoUMA +
      browser_data_back_migrator_metrics::TaskStatusToString(task_status);

  BrowserDataBackMigrator::TaskResult failure_without_errno = {task_status};
  browser_data_back_migrator_metrics::RecordPosixErrnoIfAvailable(
      failure_without_errno);
  histogram_tester.ExpectTotalCount(uma_name, 0);

  BrowserDataBackMigrator::TaskResult failure_with_errno = {task_status, EPERM};
  browser_data_back_migrator_metrics::RecordPosixErrnoIfAvailable(
      failure_with_errno);
  histogram_tester.ExpectTotalCount(uma_name, 1);
  histogram_tester.ExpectUniqueSample(uma_name, EPERM, 1);
}

TEST(BrowserDataBackMigratorMetricsTest, TaskStatusToString) {
  EXPECT_EQ(browser_data_back_migrator_metrics::TaskStatusToString(
                BrowserDataBackMigrator::TaskStatus::kSucceeded),
            "Succeeded");
}

TEST(BrowserDataBackMigratorMetricsTest, RecordMigrationTimeIfSuccessful) {
  base::HistogramTester histogram_tester;

  // No total time is recorded on failed migration.
  BrowserDataBackMigrator::TaskResult failure = {
      BrowserDataBackMigrator::TaskStatus::kDeleteTmpDirDeleteFailed, EPERM};
  browser_data_back_migrator_metrics::RecordMigrationTimeIfSuccessful(
      failure, base::TimeTicks::Now());
  histogram_tester.ExpectTotalCount(
      browser_data_back_migrator_metrics::kSuccessfulMigrationTimeUMA, 0);

  // When migration succeeds, total time is recorded.
  BrowserDataBackMigrator::TaskResult success = {
      BrowserDataBackMigrator::TaskStatus::kSucceeded};
  browser_data_back_migrator_metrics::RecordMigrationTimeIfSuccessful(
      success, base::TimeTicks::Now());
  histogram_tester.ExpectTotalCount(
      browser_data_back_migrator_metrics::kSuccessfulMigrationTimeUMA, 1);
}

TEST_F(BrowserDataBackMigratorMetricsFixtureTest,
       RecordNumberOfLacrosSecondaryProfiles) {
  base::HistogramTester histogram_tester;

  SetUpProfileDirectories(lacros_dir_);
  browser_data_back_migrator_metrics::RecordNumberOfLacrosSecondaryProfiles(
      ash_profile_dir_);

  histogram_tester.ExpectTotalCount(
      browser_data_back_migrator_metrics::kNumberOfLacrosSecondaryProfilesUMA,
      1);

  // Expect that the bucket for 3 secondary profiles has one record.
  histogram_tester.ExpectBucketCount(
      browser_data_back_migrator_metrics::kNumberOfLacrosSecondaryProfilesUMA,
      3, 1);
}

TEST(BrowserDataBackMigratorMetricsTest, IsSecondaryProfileDirectory) {
  EXPECT_TRUE(browser_data_back_migrator_metrics::IsSecondaryProfileDirectory(
      "Profile 3"));
}

TEST(BrowserDataBackMigratorMetricsTest,
     IsSecondaryProfileDirectoryDefaultsDirectory) {
  EXPECT_FALSE(browser_data_back_migrator_metrics::IsSecondaryProfileDirectory(
      "Default"));
}

TEST(BrowserDataBackMigratorMetricsTest,
     IsSecondaryProfileDirectoryProfileWithoutNumber) {
  EXPECT_FALSE(browser_data_back_migrator_metrics::IsSecondaryProfileDirectory(
      "Profile w/o number"));
}

TEST(BrowserDataBackMigratorMetricsTest,
     IsSecondaryProfileDirectoryRandomName) {
  EXPECT_FALSE(browser_data_back_migrator_metrics::IsSecondaryProfileDirectory(
      "Other directory"));
}

TEST(BrowserDataBackMigratorMetricsTest, IsSecondaryProfileDirectoryShortName) {
  EXPECT_FALSE(
      browser_data_back_migrator_metrics::IsSecondaryProfileDirectory("ash"));
}

TEST(BrowserDataBackMigratorMetricsTest,
     IsSecondaryProfileDirectoryEmptyString) {
  EXPECT_FALSE(
      browser_data_back_migrator_metrics::IsSecondaryProfileDirectory(""));
}

TEST(BrowserDataBackMigratorMetricsTest, RecordBackwardMigrationTimeDelta) {
  base::HistogramTester histogram_tester;

  browser_data_back_migrator_metrics::RecordBackwardMigrationTimeDelta(
      absl::nullopt);
  histogram_tester.ExpectTotalCount(
      browser_data_back_migrator_metrics::kElapsedTimeBetweenDataMigrations, 0);

  browser_data_back_migrator_metrics::RecordBackwardMigrationTimeDelta(
      base::Time::UnixEpoch());
  histogram_tester.ExpectTotalCount(
      browser_data_back_migrator_metrics::kElapsedTimeBetweenDataMigrations, 1);
}

}  // namespace ash
