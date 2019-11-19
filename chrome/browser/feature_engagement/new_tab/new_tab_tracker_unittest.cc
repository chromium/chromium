// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/feature_engagement/new_tab/new_tab_tracker.h"

#include <memory>

#include "base/run_loop.h"
#include "base/sequenced_task_runner.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/feature_engagement/feature_tracker.h"
#include "chrome/browser/feature_engagement/session_duration_updater.h"
#include "chrome/browser/first_run/first_run.h"
#include "chrome/browser/metrics/desktop_session_duration/desktop_session_duration_tracker.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/feature_engagement/public/event_constants.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/feature_engagement/public/tracker.h"
#include "components/feature_engagement/test/mock_tracker.h"
#include "components/feature_engagement/test/test_tracker.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace feature_engagement {

namespace {

constexpr char kGroupName[] = "Enabled";
constexpr char kNewTabTrialName[] = "NewTabTrial";
constexpr char kTestProfileName[] = "test-profile";

class FakeNewTabTracker : public NewTabTracker {
 public:
  FakeNewTabTracker(Tracker* feature_tracker, Profile* profile)
      : NewTabTracker(profile),
        feature_tracker_(feature_tracker),
        pref_service_(
            std::make_unique<sync_preferences::TestingPrefServiceSyncable>()) {
    SessionDurationUpdater::RegisterProfilePrefs(pref_service_->registry());
  }

  PrefService* GetPrefs() { return pref_service_.get(); }

  // feature_engagement::NewTabTracker:
  Tracker* GetTracker() const override { return feature_tracker_; }

 private:
  Tracker* const feature_tracker_;
  const std::unique_ptr<sync_preferences::TestingPrefServiceSyncable>
      pref_service_;
};

class NewTabTrackerEventTest : public testing::Test {
 public:
  NewTabTrackerEventTest() = default;
  ~NewTabTrackerEventTest() override = default;

  // testing::Test:
  void SetUp() override {
    // Start the DesktopSessionDurationTracker to track active session time.
    metrics::DesktopSessionDurationTracker::Initialize();
    testing_profile_manager_ = std::make_unique<TestingProfileManager>(
        TestingBrowserProcess::GetGlobal());
    ASSERT_TRUE(testing_profile_manager_->SetUp());
    mock_tracker_ = std::make_unique<testing::StrictMock<test::MockTracker>>();
    new_tab_tracker_ = std::make_unique<FakeNewTabTracker>(
        mock_tracker_.get(),
        testing_profile_manager_->CreateTestingProfile(kTestProfileName));
  }

  void TearDown() override {
    new_tab_tracker_->RemoveSessionDurationObserver();
    testing_profile_manager_.reset();
    metrics::DesktopSessionDurationTracker::CleanupForTesting();
  }

 protected:
  std::unique_ptr<TestingProfileManager> testing_profile_manager_;
  std::unique_ptr<test::MockTracker> mock_tracker_;
  std::unique_ptr<FakeNewTabTracker> new_tab_tracker_;

 private:
  content::BrowserTaskEnvironment task_environment_;

  DISALLOW_COPY_AND_ASSIGN(NewTabTrackerEventTest);
};

}  // namespace

// Tests to verify feature_engagement::Tracker API boundary expectations:

// If OnNewTabOpened() is called, the feature_engagement::Tracker
// receives the kNewTabOpenedEvent.
TEST_F(NewTabTrackerEventTest, TestOnNewTabOpened) {
  EXPECT_CALL(*mock_tracker_, NotifyEvent(events::kNewTabOpened));
  new_tab_tracker_->OnNewTabOpened();
}

// If OnOmniboxNavigation() is called, the feature_engagement::Tracker
// receives the kOmniboxInteraction event.
TEST_F(NewTabTrackerEventTest, TestOnOmniboxNavigation) {
  EXPECT_CALL(*mock_tracker_, NotifyEvent(events::kOmniboxInteraction));
  new_tab_tracker_->OnOmniboxNavigation();
}

// If OnSessionTimeMet() is called, the feature_engagement::Tracker
// receives the kSessionTime event.
TEST_F(NewTabTrackerEventTest, TestOnSessionTimeMet) {
  EXPECT_CALL(*mock_tracker_, NotifyEvent(events::kNewTabSessionTimeMet));
  new_tab_tracker_->OnSessionTimeMet();
}

namespace {

class NewTabTrackerTest : public testing::Test {
 public:
  NewTabTrackerTest() {
    base::FieldTrialParams new_tab_params;
    new_tab_params["event_new_tab_opened"] =
        "name:new_tab_opened;comparator:==0;window:3650;storage:3650";
    new_tab_params["event_omnibox_used"] =
        "name:omnibox_used;comparator:>=1;window:3650;storage:3650";
    new_tab_params["event_new_tab_session_time_met"] =
        "name:new_tab_session_time_met;comparator:>=1;window:3650;storage:3650";
    new_tab_params["event_trigger"] =
        "name:new_tab_trigger;comparator:any;window:3650;storage:3650";
    new_tab_params["event_used"] =
        "name:new_tab_clicked;comparator:any;window:3650;storage:3650";
    new_tab_params["session_rate"] = "<=3";
    new_tab_params["availability"] = "any";
    new_tab_params["x_date_released_in_seconds"] =
        base::NumberToString(static_cast<int64_t>(
            first_run::GetFirstRunSentinelCreationTime().ToDoubleT()));

    scoped_feature_list_.InitAndEnableFeatureWithParameters(kIPHNewTabFeature,
                                                            new_tab_params);
  }
  ~NewTabTrackerTest() override = default;

  // testing::Test:
  void SetUp() override {
    // Start the DesktopSessionDurationTracker to track active session time.
    metrics::DesktopSessionDurationTracker::Initialize();

    testing_profile_manager_ = std::make_unique<TestingProfileManager>(
        TestingBrowserProcess::GetGlobal());
    ASSERT_TRUE(testing_profile_manager_->SetUp());

    feature_engagement_tracker_ = CreateTestTracker();

    new_tab_tracker_ = std::make_unique<FakeNewTabTracker>(
        feature_engagement_tracker_.get(),
        testing_profile_manager_->CreateTestingProfile(kTestProfileName));

    // The feature engagement tracker does async initialization.
    base::RunLoop().RunUntilIdle();
    ASSERT_TRUE(feature_engagement_tracker_->IsInitialized());
  }

  void TearDown() override {
    new_tab_tracker_->RemoveSessionDurationObserver();
    testing_profile_manager_->DeleteTestingProfile(kTestProfileName);
    metrics::DesktopSessionDurationTracker::CleanupForTesting();
  }

 protected:
  std::unique_ptr<FakeNewTabTracker> new_tab_tracker_;
  std::unique_ptr<Tracker> feature_engagement_tracker_;

 private:
  std::unique_ptr<TestingProfileManager> testing_profile_manager_;
  base::test::ScopedFeatureList scoped_feature_list_;
  content::BrowserTaskEnvironment task_environment_;

  DISALLOW_COPY_AND_ASSIGN(NewTabTrackerTest);
};

}  // namespace

// Tests to verify NewTabTracker functional expectations:

// Test that a promo is not shown if the user uses a New Tab.
// If OnNewTabOpened() is called, the ShouldShowPromo() should return false.
TEST_F(NewTabTrackerTest, TestShouldNotShowPromo) {
  EXPECT_FALSE(new_tab_tracker_->ShouldShowPromo());

  new_tab_tracker_->OnSessionTimeMet();
  new_tab_tracker_->OnOmniboxNavigation();

  EXPECT_TRUE(new_tab_tracker_->ShouldShowPromo());

  new_tab_tracker_->OnNewTabOpened();

  EXPECT_FALSE(new_tab_tracker_->ShouldShowPromo());
}

// Test that a promo is shown if the session time is met and an omnibox
// navigation occurs. If OnSessionTimeMet() and OnOmniboxNavigation()
// are called, ShouldShowPromo() should return true.
TEST_F(NewTabTrackerTest, TestShouldShowPromo) {
  EXPECT_FALSE(new_tab_tracker_->ShouldShowPromo());

  new_tab_tracker_->OnSessionTimeMet();
  new_tab_tracker_->OnOmniboxNavigation();

  EXPECT_TRUE(new_tab_tracker_->ShouldShowPromo());
}

}  // namespace feature_engagement
