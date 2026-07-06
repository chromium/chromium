// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/service/glic_onboarding_tracker.h"

#include "base/test/metrics/histogram_tester.h"
#include "base/test/metrics/user_action_tester.h"
#include "base/time/time.h"
#include "chrome/browser/glic/glic_pref_names.h"
#include "chrome/browser/glic/glic_pref_names_internal.h"
#include "chrome/browser/glic/public/glic_enabling.h"
#include "chrome/browser/glic/service/glic_onboarding_status.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace glic {

class GlicOnboardingTrackerTest : public testing::Test {
 public:
  void SetUp() override {
    ASSERT_TRUE(testing_profile_manager_.SetUp());
    profile_ = testing_profile_manager_.CreateTestingProfile("profile");
    enabling_ = GlicEnabling::CreateForTesting(profile_, nullptr);
  }

  void TearDown() override {
    enabling_.reset();
    profile_ = nullptr;
  }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  TestingProfileManager testing_profile_manager_{
      TestingBrowserProcess::GetGlobal()};
  raw_ptr<TestingProfile> profile_;
  std::unique_ptr<GlicEnabling> enabling_;
};

TEST_F(GlicOnboardingTrackerTest, GlicOnboardingStatusGetters) {
  GlicOnboardingStatus status(profile_->GetPrefs());
  EXPECT_EQ(status.GetStatus(), OnboardingStatus::kNoInteraction);
  EXPECT_FALSE(status.IsInvoked());
  EXPECT_FALSE(status.IsOptedIn());
  EXPECT_FALSE(status.HasPrompt());

  status.SetStatus(OnboardingStatus::kNotOptedInButInvoked);
  EXPECT_TRUE(status.IsInvoked());
  EXPECT_FALSE(status.IsOptedIn());
  EXPECT_FALSE(status.HasPrompt());

  status.SetStatus(OnboardingStatus::kOptedInAndInvoked);
  EXPECT_TRUE(status.IsInvoked());
  EXPECT_TRUE(status.IsOptedIn());
  EXPECT_FALSE(status.HasPrompt());

  status.SetStatus(OnboardingStatus::kPromptAndOptIn);
  EXPECT_TRUE(status.IsInvoked());
  EXPECT_TRUE(status.IsOptedIn());
  EXPECT_TRUE(status.HasPrompt());
}

TEST_F(GlicOnboardingTrackerTest, InitialState_NoInteraction) {
  GlicOnboardingTracker tracker(profile_, enabling_.get());
  EXPECT_EQ(tracker.GetStatus(), OnboardingStatus::kNoInteraction);
}

TEST_F(GlicOnboardingTrackerTest, InitialState_MigrationIncomplete) {
  profile_->GetPrefs()->SetInteger(
      prefs::kGlicCompletedFre,
      static_cast<int>(prefs::FreStatus::kIncomplete));
  GlicOnboardingTracker tracker(profile_, enabling_.get());
  EXPECT_EQ(tracker.GetStatus(), OnboardingStatus::kNotOptedInButInvoked);
}

TEST_F(GlicOnboardingTrackerTest, StateTransitions_StandardFlow) {
  base::HistogramTester histogram_tester;
  base::UserActionTester user_action_tester;
  GlicOnboardingTracker tracker(profile_, enabling_.get());

  tracker.OnInvoke();
  EXPECT_EQ(tracker.GetStatus(), OnboardingStatus::kNotOptedInButInvoked);
  EXPECT_FALSE(
      profile_->GetPrefs()->GetTime(prefs::kGlicLastInvokedTime).is_null());
  histogram_tester.ExpectUniqueSample("Glic.Onboarding.Invoked.Status",
                                      OnboardingStatus::kNoInteraction, 1);
  EXPECT_EQ(user_action_tester.GetActionCount("Glic.Onboarding.Invoked"), 1);

  profile_->GetPrefs()->SetInteger(
      prefs::kGlicCompletedFre, static_cast<int>(prefs::FreStatus::kCompleted));
  EXPECT_EQ(tracker.GetStatus(), OnboardingStatus::kOptedInAndInvoked);

  tracker.OnPrompt();
  EXPECT_EQ(tracker.GetStatus(), OnboardingStatus::kPromptAndOptIn);
  EXPECT_FALSE(
      profile_->GetPrefs()->GetTime(prefs::kGlicLastPromptTime).is_null());
  EXPECT_EQ(
      user_action_tester.GetActionCount("Glic.Onboarding.PromptSubmitted"), 1);
}

TEST_F(GlicOnboardingTrackerTest, StateTransitions_OptInFirst) {
  GlicOnboardingTracker tracker(profile_, enabling_.get());

  profile_->GetPrefs()->SetInteger(
      prefs::kGlicCompletedFre, static_cast<int>(prefs::FreStatus::kCompleted));
  EXPECT_EQ(tracker.GetStatus(), OnboardingStatus::kOptedInButNotInvoked);

  tracker.OnInvoke();
  EXPECT_EQ(tracker.GetStatus(), OnboardingStatus::kOptedInAndInvoked);
}

TEST_F(GlicOnboardingTrackerTest, StateTransitions_UnconsentedPrompt) {
  GlicOnboardingTracker tracker(profile_, enabling_.get());

  tracker.OnInvoke();
  EXPECT_EQ(tracker.GetStatus(), OnboardingStatus::kNotOptedInButInvoked);

  tracker.OnPrompt();
  EXPECT_EQ(tracker.GetStatus(), OnboardingStatus::kPromptWithNoOptIn);

  profile_->GetPrefs()->SetInteger(
      prefs::kGlicCompletedFre, static_cast<int>(prefs::FreStatus::kCompleted));
  EXPECT_EQ(tracker.GetStatus(), OnboardingStatus::kPromptAndOptIn);
}

}  // namespace glic
