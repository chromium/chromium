// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/feature_engagement/feature_tracker.h"

#include <memory>

#include "base/files/file_util.h"
#include "base/run_loop.h"
#include "base/sequenced_task_runner.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "chrome/browser/feature_engagement/session_duration_updater.h"
#include "chrome/browser/first_run/first_run.h"
#include "chrome/browser/metrics/desktop_session_duration/desktop_session_duration_tracker.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/feature_engagement/public/event_constants.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace feature_engagement {

namespace {

constexpr int kTestTimeDeltaInMinutes = 100;
constexpr int kTestTimeSufficentInMinutes = 110;
constexpr int kTestTimeInsufficientInMinutes = 90;
constexpr char kTestProfileName[] = "test-profile";
constexpr char kTestObservedSessionTimeKey[] = "test_observed_session_time_key";

class TestFeatureTracker : public FeatureTracker {
 public:
  explicit TestFeatureTracker(Profile* profile)
      : FeatureTracker(profile,
                       &kIPHNewTabFeature,
                       kTestObservedSessionTimeKey,
                       base::TimeDelta::FromMinutes(kTestTimeDeltaInMinutes)),
        pref_service_(
            std::make_unique<sync_preferences::TestingPrefServiceSyncable>()) {
    SessionDurationUpdater::RegisterProfilePrefs(pref_service_->registry());
  }

  base::TimeDelta GetSessionTimeRequiredToShowWrapper() {
    return GetSessionTimeRequiredToShow();
  }

  bool IsNewUserWrapper() { return IsNewUser(); }

  void OnSessionTimeMet() override {}

 private:
  const std::unique_ptr<sync_preferences::TestingPrefServiceSyncable>
      pref_service_;
};

class MockTestFeatureTracker : public TestFeatureTracker {
 public:
  explicit MockTestFeatureTracker(Profile* profile)
      : TestFeatureTracker(profile) {}
  MOCK_METHOD0(OnSessionTimeMet, void());
};

class FeatureTrackerTest : public testing::Test {
 public:
  FeatureTrackerTest() = default;
  ~FeatureTrackerTest() override = default;

  // testing::Test:
  void SetUp() override {
    // Start the DesktopSessionDurationTracker to track active session time.
    metrics::DesktopSessionDurationTracker::Initialize();
    testing_profile_manager_ = std::make_unique<TestingProfileManager>(
        TestingBrowserProcess::GetGlobal());
    ASSERT_TRUE(testing_profile_manager_->SetUp());
    mock_feature_tracker_ =
        std::make_unique<testing::StrictMock<MockTestFeatureTracker>>(
            testing_profile_manager_->CreateTestingProfile(kTestProfileName));
  }

  void TearDown() override {
    // Need to invoke the reset method as TearDown is on the UI thread.
    testing_profile_manager_.reset();
    metrics::DesktopSessionDurationTracker::CleanupForTesting();
  }

 protected:
  std::unique_ptr<TestingProfileManager> testing_profile_manager_;
  std::unique_ptr<MockTestFeatureTracker> mock_feature_tracker_;

 private:
  content::BrowserTaskEnvironment task_environment_;

  DISALLOW_COPY_AND_ASSIGN(FeatureTrackerTest);
};

// If OnSessionEnded parameter is greater than HasEnoughSessionTimeElapsed
// then OnSessionTimeMet should be called.
//
// Note: in this case, RemoveSessionDurationObserver is called inside of
// OnSessionTimeMet, so it doesn't need to be called after the fact.
TEST_F(FeatureTrackerTest, TestExpectOnSessionTimeMet) {
  EXPECT_CALL(*mock_feature_tracker_, OnSessionTimeMet());
  mock_feature_tracker_.get()->OnSessionEnded(
      base::TimeDelta::FromMinutes(kTestTimeSufficentInMinutes));
}

// If OnSessionEnded parameter is less than than HasEnoughSessionTimeElapsed
// then OnSessionTimeMet should not be called.
TEST_F(FeatureTrackerTest, TestDontExpectOnSessionTimeMet) {
  mock_feature_tracker_.get()->OnSessionEnded(
      base::TimeDelta::FromMinutes(kTestTimeInsufficientInMinutes));
  mock_feature_tracker_.get()->RemoveSessionDurationObserver();
}

// The FeatureTracker should be observing sources until the
// RemoveSessionDurationObserver is called.
TEST_F(FeatureTrackerTest, TestAddAndRemoveObservers) {
  // AddSessionDurationObserver is called on initialization.
  ASSERT_TRUE(mock_feature_tracker_->IsObserving());

  mock_feature_tracker_.get()->RemoveSessionDurationObserver();

  EXPECT_FALSE(mock_feature_tracker_->IsObserving());
}

class FeatureTrackerParamsTest : public testing::Test {
 public:
  FeatureTrackerParamsTest() = default;
  ~FeatureTrackerParamsTest() override = default;

  // testing::Test:
  void SetUp() override {
    // Start the DesktopSessionDurationTracker to track active session time.
    metrics::DesktopSessionDurationTracker::Initialize();
    testing_profile_manager_ = std::make_unique<TestingProfileManager>(
        TestingBrowserProcess::GetGlobal());
    ASSERT_TRUE(testing_profile_manager_->SetUp());
  }

  void TearDown() override {
    // Need to invoke the rest method as TearDown is on the UI thread.
    testing_profile_manager_.reset();
    metrics::DesktopSessionDurationTracker::CleanupForTesting();
  }

  void SetFeatureParams(const base::Feature& feature,
                        const FieldTrialParams& params) {
    scoped_feature_list_.InitAndEnableFeatureWithParameters(kIPHNewTabFeature,
                                                            params);
  }

 protected:
  std::unique_ptr<TestingProfileManager> testing_profile_manager_;
  base::test::ScopedFeatureList scoped_feature_list_;

 private:
  content::BrowserTaskEnvironment task_environment_;

  DISALLOW_COPY_AND_ASSIGN(FeatureTrackerParamsTest);
};

// Test that session time defaults to the time in the constructor if there is no
// field param value.
TEST_F(FeatureTrackerParamsTest, TestSessionTimeWithNoFieldTrialValue) {
  std::unique_ptr<MockTestFeatureTracker> mock_feature_tracker =
      std::make_unique<testing::StrictMock<MockTestFeatureTracker>>(
          testing_profile_manager_->CreateTestingProfile(kTestProfileName));

  EXPECT_EQ(mock_feature_tracker->GetSessionTimeRequiredToShowWrapper(),
            base::TimeDelta::FromMinutes(kTestTimeDeltaInMinutes));

  mock_feature_tracker.get()->RemoveSessionDurationObserver();
}

// Test that session time defaults to the valid time from the field param value.
TEST_F(FeatureTrackerParamsTest, TestSessionTimeWithValidFieldTrialValue) {
  std::map<std::string, std::string> new_tab_params;
  new_tab_params["x_minutes"] = "1";
  SetFeatureParams(kIPHNewTabFeature, new_tab_params);

  std::unique_ptr<MockTestFeatureTracker> mock_feature_tracker =
      std::make_unique<testing::StrictMock<MockTestFeatureTracker>>(
          testing_profile_manager_->CreateTestingProfile(kTestProfileName));

  EXPECT_EQ(mock_feature_tracker->GetSessionTimeRequiredToShowWrapper(),
            base::TimeDelta::FromMinutes(1));

  mock_feature_tracker.get()->RemoveSessionDurationObserver();
}

// Test that session time defaults to the time in the constructor if the field
// param value is empty string.
TEST_F(FeatureTrackerParamsTest, TestSessionTimeWithEmptyFieldTrialValue) {
  std::map<std::string, std::string> new_tab_params;
  new_tab_params["x_minutes"] = "";
  SetFeatureParams(kIPHNewTabFeature, new_tab_params);

  std::unique_ptr<MockTestFeatureTracker> mock_feature_tracker =
      std::make_unique<testing::StrictMock<MockTestFeatureTracker>>(
          testing_profile_manager_->CreateTestingProfile(kTestProfileName));

  EXPECT_EQ(mock_feature_tracker->GetSessionTimeRequiredToShowWrapper(),
            base::TimeDelta::FromMinutes(kTestTimeDeltaInMinutes));

  mock_feature_tracker.get()->RemoveSessionDurationObserver();
}

// Test that session time defaults to the time in the constructor if the field
// param value is invalid.
TEST_F(FeatureTrackerParamsTest, TestSessionTimeWithInvalidFieldTrialValue) {
  std::map<std::string, std::string> new_tab_params;
  new_tab_params["x_minutes"] = "12g4";
  SetFeatureParams(kIPHNewTabFeature, new_tab_params);

  std::unique_ptr<MockTestFeatureTracker> mock_feature_tracker =
      std::make_unique<testing::StrictMock<MockTestFeatureTracker>>(
          testing_profile_manager_->CreateTestingProfile(kTestProfileName));

  EXPECT_EQ(mock_feature_tracker->GetSessionTimeRequiredToShowWrapper(),
            base::TimeDelta::FromMinutes(kTestTimeDeltaInMinutes));

  mock_feature_tracker.get()->RemoveSessionDurationObserver();
}

// Test that the user is new if the creation time of the first run sentinel is
// after the enabled time of the experiment.
TEST_F(FeatureTrackerParamsTest, TestIsNewUser_DefaultTime) {
  // Setting the experiment timestamp equal to the first run sentinel timestamp.
  std::map<std::string, std::string> new_tab_params;
  new_tab_params["x_date_released_in_seconds"] =
      base::NumberToString(static_cast<int64_t>(
          first_run::GetFirstRunSentinelCreationTime().ToDoubleT()));
  SetFeatureParams(kIPHNewTabFeature, new_tab_params);

  std::unique_ptr<MockTestFeatureTracker> mock_feature_tracker =
      std::make_unique<testing::StrictMock<MockTestFeatureTracker>>(
          testing_profile_manager_->CreateTestingProfile(kTestProfileName));

  EXPECT_TRUE(mock_feature_tracker->IsNewUserWrapper());
}

// Test that the user is not considered a new user if the creation time is more
// than 24 hours ago.
TEST_F(FeatureTrackerParamsTest, TestIsNotNewUser_DefaultTime) {
  // Setting the experiment timestamp equal to one second older than what is
  // considered a new user.
  std::map<std::string, std::string> new_tab_params;
  new_tab_params["x_date_released_in_seconds"] =
      base::NumberToString(static_cast<int64_t>(
          first_run::GetFirstRunSentinelCreationTime().ToDoubleT() +
          base::TimeDelta::FromHours(24).InSeconds() + 1));
  SetFeatureParams(kIPHNewTabFeature, new_tab_params);

  std::unique_ptr<MockTestFeatureTracker> mock_feature_tracker =
      std::make_unique<testing::StrictMock<MockTestFeatureTracker>>(
          testing_profile_manager_->CreateTestingProfile(kTestProfileName));

  EXPECT_FALSE(mock_feature_tracker->IsNewUserWrapper());
}

// Test that the user is not considered a new user if the creation time is more
// than the custom time threshold limit which in this case is 28 hours ago.
TEST_F(FeatureTrackerParamsTest, TestIsNewUser_CustomTime) {
  std::map<std::string, std::string> new_tab_params;
  new_tab_params["x_new_user_creation_time_threshold_in_seconds"] =
      base::NumberToString(base::TimeDelta::FromHours(28).InSeconds());

  // Setting the experiment timestamp equal to the limit of what is considered a
  // new user.
  new_tab_params["x_date_released_in_seconds"] =
      base::NumberToString(static_cast<int64_t>(
          first_run::GetFirstRunSentinelCreationTime().ToDoubleT()));
  SetFeatureParams(kIPHNewTabFeature, new_tab_params);

  std::unique_ptr<MockTestFeatureTracker> mock_feature_tracker =
      std::make_unique<testing::StrictMock<MockTestFeatureTracker>>(
          testing_profile_manager_->CreateTestingProfile(kTestProfileName));

  EXPECT_TRUE(mock_feature_tracker->IsNewUserWrapper());
}

// Test that the user is not considered a new user if the creation time is more
// than the custom time threshold limit which in this case is 28 hours ago.
TEST_F(FeatureTrackerParamsTest, TestIsNotNewUser_CustomTime) {
  std::map<std::string, std::string> new_tab_params;
  new_tab_params["x_new_user_creation_time_threshold_in_seconds"] =
      base::NumberToString(base::TimeDelta::FromHours(28).InSeconds());

  // Setting the experiment timestamp equal to one second older than what is
  // considered a new user.
  new_tab_params["x_date_released_in_seconds"] =
      base::NumberToString(static_cast<int64_t>(
          first_run::GetFirstRunSentinelCreationTime().ToDoubleT() +
          base::TimeDelta::FromHours(28).InSeconds() + 1));
  SetFeatureParams(kIPHNewTabFeature, new_tab_params);

  std::unique_ptr<MockTestFeatureTracker> mock_feature_tracker =
      std::make_unique<testing::StrictMock<MockTestFeatureTracker>>(
          testing_profile_manager_->CreateTestingProfile(kTestProfileName));

  EXPECT_FALSE(mock_feature_tracker->IsNewUserWrapper());
}

}  // namespace

}  // namespace feature_engagement
