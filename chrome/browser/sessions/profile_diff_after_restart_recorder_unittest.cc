// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sessions/profile_diff_after_restart_recorder.h"

#include <optional>
#include <string_view>
#include <utility>

#include "base/memory/raw_ptr.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/values.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

constexpr std::string_view kNormalHistogram =
    ProfileDiffAfterRestartRecorder::kNormalHistogram;
constexpr std::string_view kAppHistogram =
    ProfileDiffAfterRestartRecorder::kAppHistogram;

class ProfileDiffAfterRestartRecorderTest : public testing::Test {
 public:
  void SetUp() override {
    ASSERT_TRUE(profile_manager_.SetUp());
    first_profile_ = profile_manager_.CreateTestingProfile("first_profile");
    second_profile_ = profile_manager_.CreateTestingProfile("second_profile");
  }

  void TearDown() override {
    // The recorder outlives individual tests, so make sure it is not left
    // observing session restore.
    recorder().ResetForTesting();

    first_profile_ = nullptr;
    second_profile_ = nullptr;
  }

 protected:
  ProfileDiffAfterRestartRecorder& recorder() {
    return ProfileDiffAfterRestartRecorder::GetInstance();
  }

  PrefService* local_state() {
    return TestingBrowserProcess::GetGlobal()->local_state();
  }

  // Writes the counts that a restart would have left behind.
  void SetPreRestartCounts(std::optional<size_t> normal_profiles,
                           std::optional<size_t> app_profiles) {
    ProfileDiffAfterRestartRecorder::SavePreRestartCounts(
        local_state(), normal_profiles.value_or(0), app_profiles.value_or(0));
  }

  bool HasPreRestartCounts() {
    return local_state()->HasPrefPath(prefs::kPreSmartRestartProfileCounts);
  }

  content::BrowserTaskEnvironment task_environment_;
  TestingProfileManager profile_manager_{TestingBrowserProcess::GetGlobal()};
  base::test::ScopedFeatureList feature_list_{
      features::kRecordTabWindowDiffOnRestart};
  base::HistogramTester histogram_tester_;

  raw_ptr<TestingProfile> first_profile_ = nullptr;
  raw_ptr<TestingProfile> second_profile_ = nullptr;
};

TEST_F(ProfileDiffAfterRestartRecorderTest,
       SavePreRestartCountsOverwritesAndClears) {
  ProfileDiffAfterRestartRecorder::SavePreRestartCounts(
      local_state(), /*normal_profiles=*/1, /*app_profiles=*/2);
  EXPECT_EQ(local_state()
                ->GetDict(prefs::kPreSmartRestartProfileCounts)
                .FindInt(ProfileDiffAfterRestartRecorder::kNormalProfilesKey),
            1);
  EXPECT_EQ(local_state()
                ->GetDict(prefs::kPreSmartRestartProfileCounts)
                .FindInt(ProfileDiffAfterRestartRecorder::kAppProfilesKey),
            2);

  // Saving only normal_profiles must wipe out the earlier app_profiles count.
  ProfileDiffAfterRestartRecorder::SavePreRestartCounts(
      local_state(), /*normal_profiles=*/1, /*app_profiles=*/0);
  EXPECT_EQ(local_state()
                ->GetDict(prefs::kPreSmartRestartProfileCounts)
                .FindInt(ProfileDiffAfterRestartRecorder::kNormalProfilesKey),
            1);
  EXPECT_FALSE(local_state()
                   ->GetDict(prefs::kPreSmartRestartProfileCounts)
                   .FindInt(ProfileDiffAfterRestartRecorder::kAppProfilesKey));

  // Saving zero for both clears the pref completely.
  ProfileDiffAfterRestartRecorder::SavePreRestartCounts(
      local_state(), /*normal_profiles=*/0, /*app_profiles=*/0);
  EXPECT_FALSE(HasPreRestartCounts());
}

TEST_F(ProfileDiffAfterRestartRecorderTest, ClearPreRestartCountsDiscardsPref) {
  SetPreRestartCounts(/*normal_profiles=*/1, /*app_profiles=*/1);

  ProfileDiffAfterRestartRecorder::ClearPreRestartCounts(local_state());

  EXPECT_FALSE(HasPreRestartCounts());
}

TEST_F(ProfileDiffAfterRestartRecorderTest, DoesNotRecordWithoutSavedCounts) {
  recorder().MaybeStartRecording(local_state());
  ASSERT_FALSE(recorder().is_recording_for_testing());

  // Must be a no-op rather than a crash: startup always reports that launches
  // have finished, whether or not anything is being recorded.
  recorder().OnStartupLaunchesFinished();

  histogram_tester_.ExpectTotalCount(kNormalHistogram, 0);
  histogram_tester_.ExpectTotalCount(kAppHistogram, 0);
}

TEST_F(ProfileDiffAfterRestartRecorderTest, DoesNotRecordWhenFeatureDisabled) {
  base::test::ScopedFeatureList disabled_feature;
  disabled_feature.InitAndDisableFeature(
      features::kRecordTabWindowDiffOnRestart);
  SetPreRestartCounts(/*normal_profiles=*/1, /*app_profiles=*/1);

  recorder().MaybeStartRecording(local_state());

  EXPECT_FALSE(recorder().is_recording_for_testing());
  histogram_tester_.ExpectTotalCount(kNormalHistogram, 0);
  histogram_tester_.ExpectTotalCount(kAppHistogram, 0);
}

TEST_F(ProfileDiffAfterRestartRecorderTest, RecordsZeroDiffWhenNothingIsLost) {
  SetPreRestartCounts(/*normal_profiles=*/1, /*app_profiles=*/1);
  recorder().MaybeStartRecording(local_state());
  ASSERT_TRUE(recorder().is_recording_for_testing());

  recorder().OnProfileSessionRestored(first_profile_, /*normal_windows=*/2,
                                      /*app_windows=*/1);
  recorder().OnStartupLaunchesFinished();

  histogram_tester_.ExpectUniqueSample(kNormalHistogram, 0, 1);
  histogram_tester_.ExpectUniqueSample(kAppHistogram, 0, 1);
  EXPECT_FALSE(recorder().is_recording_for_testing());
  EXPECT_FALSE(HasPreRestartCounts());
}

TEST_F(ProfileDiffAfterRestartRecorderTest, RecordsLostProfiles) {
  SetPreRestartCounts(/*normal_profiles=*/3, /*app_profiles=*/2);
  recorder().MaybeStartRecording(local_state());

  // Only one of the profiles came back, and it restored no app windows.
  recorder().OnProfileSessionRestored(first_profile_, /*normal_windows=*/1,
                                      /*app_windows=*/0);
  recorder().OnStartupLaunchesFinished();

  histogram_tester_.ExpectUniqueSample(kNormalHistogram, 2, 1);
  histogram_tester_.ExpectUniqueSample(kAppHistogram, 2, 1);
}

TEST_F(ProfileDiffAfterRestartRecorderTest, RecordsGainedProfilesAsNegative) {
  SetPreRestartCounts(/*normal_profiles=*/1, /*app_profiles=*/std::nullopt);
  recorder().MaybeStartRecording(local_state());

  recorder().OnProfileSessionRestored(first_profile_, /*normal_windows=*/1,
                                      /*app_windows=*/0);
  recorder().OnProfileSessionRestored(second_profile_, /*normal_windows=*/1,
                                      /*app_windows=*/0);
  recorder().OnStartupLaunchesFinished();

  histogram_tester_.ExpectUniqueSample(kNormalHistogram, -1, 1);
}

TEST_F(ProfileDiffAfterRestartRecorderTest, RecordsWindowTypesIndependently) {
  SetPreRestartCounts(/*normal_profiles=*/2, /*app_profiles=*/2);
  recorder().MaybeStartRecording(local_state());

  // One profile came back with only normal windows, the other with only app
  // windows, so exactly one profile is missing for each window type.
  recorder().OnProfileSessionRestored(first_profile_, /*normal_windows=*/1,
                                      /*app_windows=*/0);
  recorder().OnProfileSessionRestored(second_profile_, /*normal_windows=*/0,
                                      /*app_windows=*/3);
  recorder().OnStartupLaunchesFinished();

  histogram_tester_.ExpectUniqueSample(kNormalHistogram, 1, 1);
  histogram_tester_.ExpectUniqueSample(kAppHistogram, 1, 1);
}

TEST_F(ProfileDiffAfterRestartRecorderTest, OnlyRecordsMetricsThatWereSaved) {
  // The previous session had app windows but no normal windows.
  SetPreRestartCounts(/*normal_profiles=*/std::nullopt, /*app_profiles=*/1);
  recorder().MaybeStartRecording(local_state());

  recorder().OnProfileSessionRestored(first_profile_, /*normal_windows=*/1,
                                      /*app_windows=*/1);
  recorder().OnStartupLaunchesFinished();

  histogram_tester_.ExpectTotalCount(kNormalHistogram, 0);
  histogram_tester_.ExpectUniqueSample(kAppHistogram, 0, 1);
}

TEST_F(ProfileDiffAfterRestartRecorderTest, IgnoresZeroSavedCounts) {
  // SavePreRestartCounts() never saves zero, so write the pref directly to
  // simulate corrupted local state.
  base::DictValue profile_dict;
  profile_dict.Set(ProfileDiffAfterRestartRecorder::kNormalProfilesKey, 0);
  profile_dict.Set(ProfileDiffAfterRestartRecorder::kAppProfilesKey, 1);
  local_state()->SetDict(prefs::kPreSmartRestartProfileCounts,
                         std::move(profile_dict));
  recorder().MaybeStartRecording(local_state());

  recorder().OnProfileSessionRestored(first_profile_, /*normal_windows=*/1,
                                      /*app_windows=*/1);
  recorder().OnStartupLaunchesFinished();

  histogram_tester_.ExpectTotalCount(kNormalHistogram, 0);
  histogram_tester_.ExpectUniqueSample(kAppHistogram, 0, 1);
}

TEST_F(ProfileDiffAfterRestartRecorderTest, ClampsLargeDiffs) {
  SetPreRestartCounts(/*normal_profiles=*/60, /*app_profiles=*/std::nullopt);
  recorder().MaybeStartRecording(local_state());

  // None of the 60 profiles came back, but the sample is capped at 50.
  recorder().OnStartupLaunchesFinished();

  histogram_tester_.ExpectUniqueSample(kNormalHistogram, 50, 1);
}

TEST_F(ProfileDiffAfterRestartRecorderTest, CountsEachProfileOnce) {
  SetPreRestartCounts(/*normal_profiles=*/2, /*app_profiles=*/std::nullopt);
  recorder().MaybeStartRecording(local_state());

  // A profile can restore browser and app windows in separate passes.
  recorder().OnProfileSessionRestored(first_profile_, /*normal_windows=*/1,
                                      /*app_windows=*/0);
  recorder().OnProfileSessionRestored(first_profile_, /*normal_windows=*/2,
                                      /*app_windows=*/0);
  recorder().OnStartupLaunchesFinished();

  histogram_tester_.ExpectUniqueSample(kNormalHistogram, 1, 1);
}

TEST_F(ProfileDiffAfterRestartRecorderTest, IgnoresProfilesWithoutWindows) {
  SetPreRestartCounts(/*normal_profiles=*/1, /*app_profiles=*/1);
  recorder().MaybeStartRecording(local_state());

  // A restore that produced nothing does not count as a restored profile.
  recorder().OnProfileSessionRestored(first_profile_, /*normal_windows=*/0,
                                      /*app_windows=*/0);
  recorder().OnStartupLaunchesFinished();

  histogram_tester_.ExpectUniqueSample(kNormalHistogram, 1, 1);
  histogram_tester_.ExpectUniqueSample(kAppHistogram, 1, 1);
}

TEST_F(ProfileDiffAfterRestartRecorderTest, DoesNotRecordBeforeLaunchesFinish) {
  SetPreRestartCounts(/*normal_profiles=*/1, /*app_profiles=*/std::nullopt);
  recorder().MaybeStartRecording(local_state());
  // The pref is consumed immediately when recording starts.
  EXPECT_FALSE(HasPreRestartCounts());

  // More profiles may still be launched, so a completed restore is not enough.
  recorder().OnProfileSessionRestored(first_profile_, /*normal_windows=*/1,
                                      /*app_windows=*/0);

  histogram_tester_.ExpectTotalCount(kNormalHistogram, 0);
  EXPECT_TRUE(recorder().is_recording_for_testing());
}

TEST_F(ProfileDiffAfterRestartRecorderTest,
       ClearPreRestartCountsDoesNotClobberInFlightRecording) {
  SetPreRestartCounts(/*normal_profiles=*/1, /*app_profiles=*/1);
  recorder().MaybeStartRecording(local_state());
  ASSERT_TRUE(recorder().is_recording_for_testing());

  // Simulate a second StartupBrowserCreator wave clearing local state while a
  // session restore from the initial restart is still in flight.
  ProfileDiffAfterRestartRecorder::ClearPreRestartCounts(local_state());

  recorder().OnProfileSessionRestored(first_profile_, /*normal_windows=*/1,
                                      /*app_windows=*/1);
  recorder().OnStartupLaunchesFinished();

  histogram_tester_.ExpectUniqueSample(kNormalHistogram, 0, 1);
  histogram_tester_.ExpectUniqueSample(kAppHistogram, 0, 1);
}

TEST_F(ProfileDiffAfterRestartRecorderTest, RecordsOnlyOnce) {
  SetPreRestartCounts(/*normal_profiles=*/1, /*app_profiles=*/std::nullopt);
  recorder().MaybeStartRecording(local_state());

  recorder().OnProfileSessionRestored(first_profile_, /*normal_windows=*/1,
                                      /*app_windows=*/0);
  recorder().OnStartupLaunchesFinished();
  // A second launch wave in the same process must not record again.
  recorder().OnStartupLaunchesFinished();

  histogram_tester_.ExpectUniqueSample(kNormalHistogram, 0, 1);
}

TEST_F(ProfileDiffAfterRestartRecorderTest, AbandonRecordingStopsRecording) {
  SetPreRestartCounts(/*normal_profiles=*/2, /*app_profiles=*/1);
  recorder().MaybeStartRecording(local_state());
  ASSERT_TRUE(recorder().is_recording_for_testing());
  EXPECT_FALSE(HasPreRestartCounts());

  // Startup showed the profile picker instead of launching profiles.
  recorder().AbandonRecording();
  recorder().OnStartupLaunchesFinished();

  histogram_tester_.ExpectTotalCount(kNormalHistogram, 0);
  histogram_tester_.ExpectTotalCount(kAppHistogram, 0);
  EXPECT_FALSE(recorder().is_recording_for_testing());
}

TEST_F(ProfileDiffAfterRestartRecorderTest,
       IgnoresRestoreNotificationsWhenNotRecording) {
  ASSERT_FALSE(recorder().is_recording_for_testing());

  // A restore notification while not recording is ignored rather than
  // crashing session restore.
  recorder().OnProfileSessionRestored(first_profile_, /*normal_windows=*/1,
                                      /*app_windows=*/0);

  histogram_tester_.ExpectTotalCount(kNormalHistogram, 0);
  histogram_tester_.ExpectTotalCount(kAppHistogram, 0);
  EXPECT_FALSE(recorder().is_recording_for_testing());
}

}  // namespace
