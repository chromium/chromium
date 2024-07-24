// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <vector>

#include "base/command_line.h"
#include "base/metrics/user_metrics.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "chrome/browser/metrics/desktop_session_duration/desktop_session_duration_tracker.h"
#include "chrome/browser/profiles/profile_activity_metrics_recorder.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/scoped_testing_local_state.h"
#include "chrome/test/base/test_browser_window.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

constexpr base::TimeDelta kInactivityTimeout = base::Minutes(5);
constexpr base::TimeDelta kLongTimeOfInactivity = base::Minutes(30);

}  // namespace

class ProfileActivityMetricsRecorderTest : public testing::Test {
 public:
  ProfileActivityMetricsRecorderTest()
      : task_environment_(base::test::TaskEnvironment::TimeSource::MOCK_TIME),
        profile_manager_(TestingBrowserProcess::GetGlobal()) {
    base::CommandLine::ForCurrentProcess()->AppendSwitch(switches::kNoFirstRun);
    base::SetRecordActionTaskRunner(
        task_environment_.GetMainThreadTaskRunner());
  }

  ProfileActivityMetricsRecorderTest(
      const ProfileActivityMetricsRecorderTest&) = delete;
  ProfileActivityMetricsRecorderTest& operator=(
      const ProfileActivityMetricsRecorderTest&) = delete;

  void SetUp() override {
    Test::SetUp();
    ASSERT_TRUE(profile_manager_.SetUp());

    metrics::DesktopSessionDurationTracker::Initialize();
    metrics::DesktopSessionDurationTracker::Get()
        ->SetInactivityTimeoutForTesting(kInactivityTimeout);
    ProfileActivityMetricsRecorder::Initialize();
  }

  void TearDown() override {
    // Clean up the global state, so it can be correctly initialized for the
    // next test case.
    ProfileActivityMetricsRecorder::CleanupForTesting();
    metrics::DesktopSessionDurationTracker::CleanupForTesting();
  }

  void ActivateBrowser(Profile* profile) {
    Browser::CreateParams browser_params(profile, false);
    browsers_.push_back(CreateBrowserWithTestWindowForParams(browser_params));

    // This triggers the recorder to post a task, wait until that's done.
    BrowserList::SetLastActive(browsers_.back().get());
    task_environment_.RunUntilIdle();
  }

  void ActivateIncognitoBrowser(Profile* profile) {
    ActivateBrowser(profile->GetPrimaryOTRProfile(/*create_if_needed=*/true));
  }

  void ActivateGuestBrowser(Profile* profile) {
    ActivateBrowser(profile->GetPrimaryOTRProfile(/*create_if_needed=*/true));
  }

  void SimulateUserEvent() {
    metrics::DesktopSessionDurationTracker::Get()->OnUserEvent();
  }

  // Method to test the recording of the Profile.UserAction.PerProfile
  // histogram.
  void SimulateUserActionAndExpectRecording(int bucket) {
    // A new |base::HistogramTester| has to be created, because other methods
    // could've already triggered user actions.
    base::HistogramTester histograms;
    base::RecordAction(base::UserMetricsAction("Test_Action"));
    histograms.ExpectBucketCount("Profile.UserAction.PerProfile", bucket,
                                 /*count=*/1);
  }

  TestingProfileManager* profile_manager() { return &profile_manager_; }
  base::HistogramTester* histograms() { return &histogram_tester_; }
  content::BrowserTaskEnvironment* task_environment() {
    return &task_environment_;
  }

 private:
  content::BrowserTaskEnvironment task_environment_;

  TestingProfileManager profile_manager_;
  base::HistogramTester histogram_tester_;

  std::vector<std::unique_ptr<Browser>> browsers_;
};

TEST_F(ProfileActivityMetricsRecorderTest, GuestProfile) {
  Profile* regular_profile = profile_manager()->CreateTestingProfile("p1");
  Profile* guest_profile = profile_manager()->CreateGuestProfile();
  histograms()->ExpectTotalCount("Profile.BrowserActive.PerProfile", 0);

  // Check whether the regular profile is counted in bucket 1. (Bucket 0 is
  // reserved for the guest profile.)
  ActivateBrowser(regular_profile);
  histograms()->ExpectBucketCount("Profile.BrowserActive.PerProfile",
                                  /*bucket=*/1, /*count=*/1);
  SimulateUserActionAndExpectRecording(/*bucket=*/1);
  histograms()->ExpectTotalCount("Profile.NumberOfProfilesAtProfileSwitch",
                                 /*count=*/0);

  ActivateGuestBrowser(guest_profile);
  histograms()->ExpectBucketCount("Profile.BrowserActive.PerProfile",
                                  /*bucket=*/0, /*count=*/1);
  SimulateUserActionAndExpectRecording(/*bucket=*/0);
  histograms()->ExpectUniqueSample("Profile.NumberOfProfilesAtProfileSwitch",
                                   /*bucket=*/1, /*count=*/1);

  histograms()->ExpectTotalCount("Profile.BrowserActive.PerProfile", 2);
}

TEST_F(ProfileActivityMetricsRecorderTest, IncognitoProfile) {
  Profile* regular_profile = profile_manager()->CreateTestingProfile("p1");
  histograms()->ExpectTotalCount("Profile.BrowserActive.PerProfile", 0);

  ActivateBrowser(regular_profile);
  histograms()->ExpectBucketCount("Profile.BrowserActive.PerProfile",
                                  /*bucket=*/1, /*count=*/1);

  ActivateIncognitoBrowser(regular_profile);
  histograms()->ExpectBucketCount("Profile.BrowserActive.PerProfile",
                                  /*bucket=*/1, /*count=*/2);

  histograms()->ExpectTotalCount("Profile.BrowserActive.PerProfile", 2);
  histograms()->ExpectTotalCount("Profile.NumberOfProfilesAtProfileSwitch",
                                 /*count=*/0);
}

TEST_F(ProfileActivityMetricsRecorderTest, MultipleProfiles) {
  // Profile 1: Profile is created. This does not affect the histogram.
  Profile* profile1 = profile_manager()->CreateTestingProfile("p1");
  // Profile 2: Profile is created. This does not affect the histogram.
  Profile* profile2 = profile_manager()->CreateTestingProfile("p2");
  histograms()->ExpectTotalCount("Profile.BrowserActive.PerProfile", 0);

  // Profile 1: Browser is activated for the first time. The profile is assigned
  // bucket 1.
  ActivateBrowser(profile1);
  histograms()->ExpectBucketCount("Profile.BrowserActive.PerProfile",
                                  /*bucket=*/1, /*count=*/1);
  SimulateUserActionAndExpectRecording(/*bucket=*/1);

  // Profile 3: Profile is created. This does not affect the histogram.
  Profile* profile3 = profile_manager()->CreateTestingProfile("p3");
  histograms()->ExpectTotalCount("Profile.BrowserActive.PerProfile", 1);

  // Profile 1: Another browser is activated.
  ActivateBrowser(profile1);
  histograms()->ExpectBucketCount("Profile.BrowserActive.PerProfile",
                                  /*bucket=*/1, /*count=*/2);
  SimulateUserActionAndExpectRecording(/*bucket=*/1);
  // No profile switch, so far.
  histograms()->ExpectTotalCount("Profile.NumberOfProfilesAtProfileSwitch",
                                 /*count=*/0);

  // Profile 1: Session lasts 2 minutes.
  task_environment()->FastForwardBy(base::Minutes(2));

  // Profile 3: Browser is activated for the first time. The profile is assigned
  // bucket 2.
  ActivateBrowser(profile3);
  histograms()->ExpectBucketCount("Profile.BrowserActive.PerProfile",
                                  /*bucket=*/2, /*count=*/1);
  SimulateUserActionAndExpectRecording(/*bucket=*/2);
  histograms()->ExpectUniqueSample("Profile.NumberOfProfilesAtProfileSwitch",
                                   /*bucket=*/3, /*count=*/1);

  // Profile 1: Session ended. The duration(2 minutes) is recorded.
  histograms()->ExpectBucketCount("Profile.SessionDuration.PerProfile",
                                  /*bucket=*/1, /*count=*/2);

  // Profile 3: Session lasts 2 minutes.
  task_environment()->FastForwardBy(base::Minutes(2));

  // Profile 2: Browser is activated for the first time. The profile is assigned
  // bucket 3.
  ActivateBrowser(profile2);
  histograms()->ExpectBucketCount("Profile.BrowserActive.PerProfile",
                                  /*bucket=*/3, /*count=*/1);
  SimulateUserActionAndExpectRecording(/*bucket=*/3);
  histograms()->ExpectUniqueSample("Profile.NumberOfProfilesAtProfileSwitch",
                                   /*bucket=*/3, /*count=*/2);

  // Profile 3: Session ended. The duration(2 minutes) is recorded.
  histograms()->ExpectBucketCount("Profile.SessionDuration.PerProfile",
                                  /*bucket=*/2, /*count=*/2);

  histograms()->ExpectTotalCount("Profile.BrowserActive.PerProfile", 4);
}

TEST_F(ProfileActivityMetricsRecorderTest, SessionInactivityNotRecorded) {
  Profile* profile = profile_manager()->CreateTestingProfile("p1");

  ActivateBrowser(profile);
  histograms()->ExpectBucketCount("Profile.BrowserActive.PerProfile",
                                  /*bucket=*/1, /*count=*/1);

  // Wait 2 minutes before doing another user interaction.
  task_environment()->FastForwardBy(base::Minutes(2));
  SimulateUserEvent();

  // Stay inactive so the session ends.
  task_environment()->FastForwardBy(kInactivityTimeout * 2);

  // The inactive time is not recorded.
  histograms()->ExpectBucketCount("Profile.SessionDuration.PerProfile",
                                  /*bucket=*/1, /*count=*/2);
}

TEST_F(ProfileActivityMetricsRecorderTest, ProfileState) {
  Profile* regular_profile = profile_manager()->CreateTestingProfile("p1");
  Profile* guest_profile = profile_manager()->CreateGuestProfile();
  histograms()->ExpectTotalCount("Profile.State.LastUsed_All", 0);

  ActivateBrowser(regular_profile);
  histograms()->ExpectTotalCount("Profile.State.LastUsed_All", 1);
  // This is somehow important for the session to end later in the test.
  SimulateUserEvent();

  // Repeating the same thing immediately has no impact.
  ActivateBrowser(regular_profile);
  histograms()->ExpectTotalCount("Profile.State.LastUsed_All", 1);

  // Repeating the same thing immediately has no impact (neither for any other
  // profile).
  ActivateGuestBrowser(guest_profile);
  histograms()->ExpectTotalCount("Profile.State.LastUsed_All", 1);

  // Stay inactive so the session ends and stay inactive long after that.
  task_environment()->FastForwardBy(kInactivityTimeout * 2 +
                                    kLongTimeOfInactivity);

  // Now we get another record (no matter which profile triggers that).
  ActivateBrowser(regular_profile);
  histograms()->ExpectTotalCount("Profile.State.LastUsed_All", 2);

  // Repeating the same thing immediately has no impact.
  ActivateBrowser(regular_profile);
  histograms()->ExpectTotalCount("Profile.State.LastUsed_All", 2);
}

TEST_F(ProfileActivityMetricsRecorderTest, AccountMetrics) {
  Profile* regular_profile = profile_manager()->CreateTestingProfile("p1");
  Profile* guest_profile = profile_manager()->CreateGuestProfile();
  histograms()->ExpectTotalCount("Profile.AllAccounts.Names", 0);

  ActivateBrowser(regular_profile);
  histograms()->ExpectTotalCount("Profile.AllAccounts.Names", 1);

  // Repeating the same thing records the metric again.
  ActivateBrowser(regular_profile);
  histograms()->ExpectTotalCount("Profile.AllAccounts.Names", 2);

  // We don't record for the guest profile.
  ActivateGuestBrowser(guest_profile);
  histograms()->ExpectTotalCount("Profile.AllAccounts.Names", 2);
}
