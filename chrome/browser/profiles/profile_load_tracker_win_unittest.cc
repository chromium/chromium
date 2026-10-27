// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profiles/profile_load_tracker_win.h"

#include "base/files/file.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/strings/strcat.h"
#include "base/test/gmock_expected_support.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

std::optional<base::File> CreateTestFile(const base::FilePath& path) {
  base::File file(path,
                  base::File::FLAG_CREATE_ALWAYS | base::File::FLAG_WRITE);
  if (file.IsValid()) {
    return file;
  }
  return std::nullopt;
}

const char kHistogramSuffix[] = "Regular";

}  // namespace

class ProfileLoadTrackerWinTest : public testing::Test {
 protected:
  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    feature_list_.InitAndEnableFeature(features::kProfileLoadTracker);
  }

  base::FilePath GetProfilePath() { return temp_dir_.GetPath(); }

  base::ScopedTempDir temp_dir_;
  content::BrowserTaskEnvironment task_environment_;
  base::test::ScopedFeatureList feature_list_;
};

TEST_F(ProfileLoadTrackerWinTest, NoCrash) {
  base::HistogramTester histogram_tester;
  base::FilePath profile_path = GetProfilePath();

  {
    TestingProfile::Builder builder;
    builder.SetPath(profile_path);
    builder.EnableProfileLoadTracker();
    std::unique_ptr<TestingProfile> profile = builder.Build();

    histogram_tester.ExpectUniqueSample(
        base::StrCat(
            {"Profile.Windows.ProfilePreviousExitStatus.", kHistogramSuffix}),
        static_cast<int>(ProfilePreviousExitStatus::kNoCrash), 1);

    EXPECT_TRUE(base::PathExists(
        profile_path.Append(ProfileLoadTracker::kDirtyFileName)));
    EXPECT_TRUE(base::PathExists(
        profile_path.Append(ProfileLoadTracker::kLockFileName)));
  }

  // After destruction
  EXPECT_FALSE(base::PathExists(
      profile_path.Append(ProfileLoadTracker::kDirtyFileName)));
  EXPECT_FALSE(
      base::PathExists(profile_path.Append(ProfileLoadTracker::kLockFileName)));
}

TEST_F(ProfileLoadTrackerWinTest, Dirty) {
  base::FilePath profile_path = GetProfilePath();
  EXPECT_TRUE(
      CreateTestFile(profile_path.Append(ProfileLoadTracker::kDirtyFileName))
          .has_value());

  base::HistogramTester histogram_tester;
  {
    TestingProfile::Builder builder;
    builder.SetPath(profile_path);
    builder.EnableProfileLoadTracker();
    std::unique_ptr<TestingProfile> profile = builder.Build();

    histogram_tester.ExpectUniqueSample(
        base::StrCat(
            {"Profile.Windows.ProfilePreviousExitStatus.", kHistogramSuffix}),
        static_cast<int>(ProfilePreviousExitStatus::kDirty), 1);

    EXPECT_TRUE(base::PathExists(
        profile_path.Append(ProfileLoadTracker::kWaitingForCrashAckFileName)));
    EXPECT_TRUE(base::PathExists(
        profile_path.Append(ProfileLoadTracker::kDirtyFileName)));
  }

  EXPECT_FALSE(base::PathExists(
      profile_path.Append(ProfileLoadTracker::kDirtyFileName)));
  EXPECT_TRUE(base::PathExists(
      profile_path.Append(ProfileLoadTracker::kWaitingForCrashAckFileName)));
}

TEST_F(ProfileLoadTrackerWinTest, DirtyWaitingForAck) {
  base::FilePath profile_path = GetProfilePath();
  EXPECT_TRUE(
      CreateTestFile(profile_path.Append(ProfileLoadTracker::kDirtyFileName))
          .has_value());
  EXPECT_TRUE(
      CreateTestFile(
          profile_path.Append(ProfileLoadTracker::kWaitingForCrashAckFileName))
          .has_value());

  base::HistogramTester histogram_tester;
  {
    TestingProfile::Builder builder;
    builder.SetPath(profile_path);
    builder.EnableProfileLoadTracker();
    std::unique_ptr<TestingProfile> profile = builder.Build();

    histogram_tester.ExpectUniqueSample(
        base::StrCat(
            {"Profile.Windows.ProfilePreviousExitStatus.", kHistogramSuffix}),
        static_cast<int>(ProfilePreviousExitStatus::kDirtyDidNotAck), 1);
  }
  EXPECT_FALSE(base::PathExists(
      profile_path.Append(ProfileLoadTracker::kDirtyFileName)));
  EXPECT_TRUE(base::PathExists(
      profile_path.Append(ProfileLoadTracker::kWaitingForCrashAckFileName)));
}

TEST_F(ProfileLoadTrackerWinTest, LoadedFineWaitingForAck) {
  base::FilePath profile_path = GetProfilePath();
  EXPECT_TRUE(
      CreateTestFile(
          profile_path.Append(ProfileLoadTracker::kWaitingForCrashAckFileName))
          .has_value());

  base::HistogramTester histogram_tester;
  {
    TestingProfile::Builder builder;
    builder.SetPath(profile_path);
    builder.EnableProfileLoadTracker();
    std::unique_ptr<TestingProfile> profile = builder.Build();

    histogram_tester.ExpectUniqueSample(
        base::StrCat(
            {"Profile.Windows.ProfilePreviousExitStatus.", kHistogramSuffix}),
        static_cast<int>(ProfilePreviousExitStatus::kNoCrashDidNotAck), 1);

    EXPECT_TRUE(base::PathExists(
        profile_path.Append(ProfileLoadTracker::kDirtyFileName)));
  }
  EXPECT_FALSE(base::PathExists(
      profile_path.Append(ProfileLoadTracker::kDirtyFileName)));
  EXPECT_TRUE(base::PathExists(
      profile_path.Append(ProfileLoadTracker::kWaitingForCrashAckFileName)));
}

TEST_F(ProfileLoadTrackerWinTest, Acquired) {
  base::HistogramTester histogram_tester;
  {
    TestingProfile::Builder builder;
    builder.SetPath(GetProfilePath());
    builder.EnableProfileLoadTracker();
    std::unique_ptr<TestingProfile> profile = builder.Build();

    histogram_tester.ExpectUniqueSample(
        base::StrCat({"Profile.Windows.ProfileLockStatus.", kHistogramSuffix}),
        static_cast<int>(ProfileLockStatus::kAcquired), 1);
  }
}

TEST_F(ProfileLoadTrackerWinTest, AcquiredSystemCrash) {
  base::FilePath profile_path = GetProfilePath();
  CreateTestFile(profile_path.Append(ProfileLoadTracker::kLockFileName));

  base::HistogramTester histogram_tester;
  {
    TestingProfile::Builder builder;
    builder.SetPath(profile_path);
    builder.EnableProfileLoadTracker();
    std::unique_ptr<TestingProfile> profile = builder.Build();

    histogram_tester.ExpectUniqueSample(
        base::StrCat({"Profile.Windows.ProfileLockStatus.", kHistogramSuffix}),
        static_cast<int>(ProfileLockStatus::kAcquiredSystemCrash), 1);
  }
}

TEST_F(ProfileLoadTrackerWinTest, FailedInUse) {
  base::FilePath profile_path = GetProfilePath();
  std::optional<base::File> lock_file =
      CreateTestFile(profile_path.Append(ProfileLoadTracker::kLockFileName));
  EXPECT_TRUE(lock_file.has_value());

  base::HistogramTester histogram_tester;
  {
    TestingProfile::Builder builder;
    builder.SetPath(profile_path);
    builder.EnableProfileLoadTracker();
    std::unique_ptr<TestingProfile> profile = builder.Build();

    histogram_tester.ExpectUniqueSample(
        base::StrCat({"Profile.Windows.ProfileLockStatus.", kHistogramSuffix}),
        static_cast<int>(ProfileLockStatus::kFailedInUse), 1);
  }
}

TEST_F(ProfileLoadTrackerWinTest, FailedUnknown) {
  base::FilePath profile_path = GetProfilePath();
  ASSERT_TRUE(base::CreateDirectory(
      profile_path.Append(ProfileLoadTracker::kLockFileName)));

  base::HistogramTester histogram_tester;
  {
    TestingProfile::Builder builder;
    builder.SetPath(profile_path);
    builder.EnableProfileLoadTracker();
    std::unique_ptr<TestingProfile> profile = builder.Build();

    histogram_tester.ExpectUniqueSample(
        base::StrCat({"Profile.Windows.ProfileLockStatus.", kHistogramSuffix}),
        static_cast<int>(ProfileLockStatus::kFailedUnknown), 1);
  }
}

TEST(ProfileLoadTrackerTest, FeatureDisable) {
  base::ScopedTempDir temp_dir_;
  content::BrowserTaskEnvironment task_environment_;
  base::test::ScopedFeatureList feature_list;
  ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
  feature_list.InitAndDisableFeature(features::kProfileLoadTracker);

  base::FilePath profile_path = temp_dir_.GetPath();
  base::HistogramTester histogram_tester;

  TestingProfile::Builder builder;
  builder.SetPath(profile_path);
  builder.EnableProfileLoadTracker();
  std::unique_ptr<TestingProfile> profile = builder.Build();

  histogram_tester.ExpectUniqueSample(
      base::StrCat(
          {"Profile.Windows.ProfilePreviousExitStatus.", kHistogramSuffix}),
      static_cast<int>(ProfilePreviousExitStatus::kNoCrash), 0);

  EXPECT_FALSE(base::PathExists(
      profile_path.Append(ProfileLoadTracker::kDirtyFileName)));
  EXPECT_FALSE(
      base::PathExists(profile_path.Append(ProfileLoadTracker::kLockFileName)));
}
