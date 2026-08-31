// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/service/glic_onboarding_tracker.h"

#include "base/test/metrics/histogram_tester.h"
#include "base/test/metrics/user_action_tester.h"
#include "base/time/time.h"
#include "chrome/browser/glic/glic_pref_names.h"
#include "chrome/browser/glic/glic_pref_names_internal.h"
#include "chrome/browser/glic/host/glic.mojom.h"
#include "chrome/browser/glic/public/glic_enabling.h"
#include "chrome/browser/glic/service/glic_onboarding_status.h"
#include "chrome/browser/glic/service/metrics/metrics_types.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/prefs/pref_service.h"
#include "components/ukm/test_ukm_recorder.h"
#include "content/public/test/browser_task_environment.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
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

  tracker.OnInvoke(mojom::InvocationSource::kTopChromeButton,
                   ukm::kInvalidSourceId);
  EXPECT_EQ(tracker.GetStatus(), OnboardingStatus::kNotOptedInButInvoked);
  EXPECT_FALSE(
      profile_->GetPrefs()->GetTime(prefs::kGlicLastInvokedTime).is_null());
  histogram_tester.ExpectUniqueSample("Glic.Onboarding.Invoked.Status",
                                      OnboardingStatus::kNoInteraction, 1);
  EXPECT_EQ(user_action_tester.GetActionCount("Glic.Onboarding.Invoked"), 1);

  profile_->GetPrefs()->SetInteger(
      prefs::kGlicCompletedFre, static_cast<int>(prefs::FreStatus::kCompleted));
  EXPECT_EQ(tracker.GetStatus(), OnboardingStatus::kOptedInAndInvoked);

  tracker.OnPrompt(ukm::kInvalidSourceId);
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

  tracker.OnInvoke(mojom::InvocationSource::kTopChromeButton,
                   ukm::kInvalidSourceId);
  EXPECT_EQ(tracker.GetStatus(), OnboardingStatus::kOptedInAndInvoked);
}

TEST_F(GlicOnboardingTrackerTest, StateTransitions_UnconsentedPrompt) {
  GlicOnboardingTracker tracker(profile_, enabling_.get());

  tracker.OnInvoke(mojom::InvocationSource::kTopChromeButton,
                   ukm::kInvalidSourceId);
  EXPECT_EQ(tracker.GetStatus(), OnboardingStatus::kNotOptedInButInvoked);

  tracker.OnPrompt(ukm::kInvalidSourceId);
  EXPECT_EQ(tracker.GetStatus(), OnboardingStatus::kPromptWithNoOptIn);

  profile_->GetPrefs()->SetInteger(
      prefs::kGlicCompletedFre, static_cast<int>(prefs::FreStatus::kCompleted));
  EXPECT_EQ(tracker.GetStatus(), OnboardingStatus::kPromptAndOptIn);
}

TEST_F(GlicOnboardingTrackerTest, Ukm_StandardOnboardingFunnel) {
  ukm::TestAutoSetUkmRecorder ukm_recorder;
  GlicOnboardingTracker tracker(profile_, enabling_.get());
  const ukm::SourceId kSourceId1 =
      ukm::ConvertToSourceId(1234, ukm::SourceIdType::NAVIGATION_ID);
  const ukm::SourceId kSourceId2 =
      ukm::ConvertToSourceId(5678, ukm::SourceIdType::NAVIGATION_ID);

  // Initial invocation by unconsented user.
  tracker.OnInvoke(mojom::InvocationSource::kTopChromeButton, kSourceId1);
  {
    auto entries = ukm_recorder.GetEntriesByName(
        ukm::builders::Glic_Onboarding::kEntryName);
    ASSERT_EQ(entries.size(), 1u);
    EXPECT_EQ(entries[0]->source_id, kSourceId1);
    ukm_recorder.ExpectEntryMetric(
        entries[0], ukm::builders::Glic_Onboarding::kFunnelStepName,
        static_cast<int64_t>(OnboardingFunnelStep::kNewUserOpen));
    ukm_recorder.ExpectEntryMetric(
        entries[0], ukm::builders::Glic_Onboarding::kInvocationSourceName,
        static_cast<int64_t>(mojom::InvocationSource::kTopChromeButton));
  }

  // FRE Opt-in Dialog is rendered to the user.
  tracker.OnFreOptInShown(kSourceId1);
  {
    auto entries = ukm_recorder.GetEntriesByName(
        ukm::builders::Glic_Onboarding::kEntryName);
    ASSERT_EQ(entries.size(), 2u);
    EXPECT_EQ(entries[1]->source_id, kSourceId1);
    ukm_recorder.ExpectEntryMetric(
        entries[1], ukm::builders::Glic_Onboarding::kFunnelStepName,
        static_cast<int64_t>(OnboardingFunnelStep::kFreOptInShown));
    ukm_recorder.ExpectEntryMetric(
        entries[1], ukm::builders::Glic_Onboarding::kInvocationSourceName,
        static_cast<int64_t>(mojom::InvocationSource::kTopChromeButton));
  }

  // User accepts FRE consent.
  profile_->GetPrefs()->SetInteger(
      prefs::kGlicCompletedFre, static_cast<int>(prefs::FreStatus::kCompleted));
  {
    auto entries = ukm_recorder.GetEntriesByName(
        ukm::builders::Glic_Onboarding::kEntryName);
    ASSERT_EQ(entries.size(), 3u);
    EXPECT_EQ(entries[2]->source_id, kSourceId1);
    ukm_recorder.ExpectEntryMetric(
        entries[2], ukm::builders::Glic_Onboarding::kFunnelStepName,
        static_cast<int64_t>(OnboardingFunnelStep::kFreOptInAccepted));
    ukm_recorder.ExpectEntryMetric(
        entries[2], ukm::builders::Glic_Onboarding::kInvocationSourceName,
        static_cast<int64_t>(mojom::InvocationSource::kTopChromeButton));
  }

  // User submits prompt on a different tab.
  tracker.OnPrompt(kSourceId2);
  {
    auto entries = ukm_recorder.GetEntriesByName(
        ukm::builders::Glic_Onboarding::kEntryName);
    ASSERT_EQ(entries.size(), 4u);
    EXPECT_EQ(entries[3]->source_id, kSourceId2);
    ukm_recorder.ExpectEntryMetric(
        entries[3], ukm::builders::Glic_Onboarding::kFunnelStepName,
        static_cast<int64_t>(OnboardingFunnelStep::kFirstPromptSubmitted));
    ukm_recorder.ExpectEntryMetric(
        entries[3], ukm::builders::Glic_Onboarding::kInvocationSourceName,
        static_cast<int64_t>(mojom::InvocationSource::kTopChromeButton));
  }

  // Subsequent prompt submissions do NOT emit additional FirstPromptSubmitted
  // UKMs.
  tracker.OnPrompt(kSourceId2);
  {
    auto entries = ukm_recorder.GetEntriesByName(
        ukm::builders::Glic_Onboarding::kEntryName);
    EXPECT_EQ(entries.size(), 4u);
  }
}

TEST_F(GlicOnboardingTrackerTest, Ukm_MultipleUnconsentedEventsRecorded) {
  ukm::TestAutoSetUkmRecorder ukm_recorder;
  GlicOnboardingTracker tracker(profile_, enabling_.get());
  const ukm::SourceId kSourceId =
      ukm::ConvertToSourceId(1234, ukm::SourceIdType::NAVIGATION_ID);

  // Invoke before consent records kNewUserOpen.
  tracker.OnInvoke(mojom::InvocationSource::kTopChromeButton, kSourceId);
  // Repeated invoke before consent records kNewUserOpen again.
  tracker.OnInvoke(mojom::InvocationSource::kTopChromeButton, kSourceId);

  // FRE shown records kFreOptInShown.
  tracker.OnFreOptInShown(kSourceId);
  // Repeated FRE shown records another kFreOptInShown event.
  tracker.OnFreOptInShown(kSourceId);

  auto entries =
      ukm_recorder.GetEntriesByName(ukm::builders::Glic_Onboarding::kEntryName);
  ASSERT_EQ(entries.size(), 4u);
  ukm_recorder.ExpectEntryMetric(
      entries[0], ukm::builders::Glic_Onboarding::kFunnelStepName,
      static_cast<int64_t>(OnboardingFunnelStep::kNewUserOpen));
  ukm_recorder.ExpectEntryMetric(
      entries[1], ukm::builders::Glic_Onboarding::kFunnelStepName,
      static_cast<int64_t>(OnboardingFunnelStep::kNewUserOpen));
  ukm_recorder.ExpectEntryMetric(
      entries[2], ukm::builders::Glic_Onboarding::kFunnelStepName,
      static_cast<int64_t>(OnboardingFunnelStep::kFreOptInShown));
  ukm_recorder.ExpectEntryMetric(
      entries[3], ukm::builders::Glic_Onboarding::kFunnelStepName,
      static_cast<int64_t>(OnboardingFunnelStep::kFreOptInShown));
}

TEST_F(GlicOnboardingTrackerTest, Ukm_UnconsentedPromptFlow) {
  ukm::TestAutoSetUkmRecorder ukm_recorder;
  GlicOnboardingTracker tracker(profile_, enabling_.get());
  const ukm::SourceId kSourceId =
      ukm::ConvertToSourceId(1234, ukm::SourceIdType::NAVIGATION_ID);

  tracker.OnInvoke(mojom::InvocationSource::kAutoOpenedForPdf, kSourceId);
  tracker.OnPrompt(kSourceId);
  profile_->GetPrefs()->SetInteger(
      prefs::kGlicCompletedFre, static_cast<int>(prefs::FreStatus::kCompleted));

  auto entries =
      ukm_recorder.GetEntriesByName(ukm::builders::Glic_Onboarding::kEntryName);
  ASSERT_EQ(entries.size(), 3u);
  ukm_recorder.ExpectEntryMetric(
      entries[0], ukm::builders::Glic_Onboarding::kFunnelStepName,
      static_cast<int64_t>(OnboardingFunnelStep::kNewUserOpen));
  ukm_recorder.ExpectEntryMetric(
      entries[1], ukm::builders::Glic_Onboarding::kFunnelStepName,
      static_cast<int64_t>(OnboardingFunnelStep::kFirstPromptSubmitted));
  ukm_recorder.ExpectEntryMetric(
      entries[2], ukm::builders::Glic_Onboarding::kFunnelStepName,
      static_cast<int64_t>(OnboardingFunnelStep::kFreOptInAccepted));
}

TEST_F(GlicOnboardingTrackerTest, Ukm_MultipleUnconsentedPromptsRecorded) {
  ukm::TestAutoSetUkmRecorder ukm_recorder;
  GlicOnboardingTracker tracker(profile_, enabling_.get());
  const ukm::SourceId kSourceId =
      ukm::ConvertToSourceId(1234, ukm::SourceIdType::NAVIGATION_ID);

  // Invoke sets invocation source for the session.
  tracker.OnInvoke(mojom::InvocationSource::kTopChromeButton, kSourceId);

  // Unconsented prompt submissions: all are recorded.
  tracker.OnPrompt(kSourceId);
  tracker.OnPrompt(kSourceId);

  {
    auto entries = ukm_recorder.GetEntriesByName(
        ukm::builders::Glic_Onboarding::kEntryName);
    ASSERT_EQ(entries.size(), 3u);
    ukm_recorder.ExpectEntryMetric(
        entries[0], ukm::builders::Glic_Onboarding::kFunnelStepName,
        static_cast<int64_t>(OnboardingFunnelStep::kNewUserOpen));
    ukm_recorder.ExpectEntryMetric(
        entries[1], ukm::builders::Glic_Onboarding::kFunnelStepName,
        static_cast<int64_t>(OnboardingFunnelStep::kFirstPromptSubmitted));
    ukm_recorder.ExpectEntryMetric(
        entries[2], ukm::builders::Glic_Onboarding::kFunnelStepName,
        static_cast<int64_t>(OnboardingFunnelStep::kFirstPromptSubmitted));
  }

  // Grant consent.
  profile_->GetPrefs()->SetInteger(
      prefs::kGlicCompletedFre, static_cast<int>(prefs::FreStatus::kCompleted));

  // Subsequent consented prompt submissions: should not record since user is
  // consented and HasPrompt is true.
  tracker.OnPrompt(kSourceId);
  {
    auto entries = ukm_recorder.GetEntriesByName(
        ukm::builders::Glic_Onboarding::kEntryName);
    ASSERT_EQ(entries.size(), 4u);
  }
}

TEST_F(GlicOnboardingTrackerTest, Ukm_InvalidSourceIdFallsBackToNoURLSourceId) {
  ukm::TestAutoSetUkmRecorder ukm_recorder;
  GlicOnboardingTracker tracker(profile_, enabling_.get());

  tracker.OnInvoke(mojom::InvocationSource::kOsHotkey, ukm::kInvalidSourceId);

  auto entries =
      ukm_recorder.GetEntriesByName(ukm::builders::Glic_Onboarding::kEntryName);
  ASSERT_EQ(entries.size(), 1u);
  EXPECT_EQ(entries[0]->source_id, ukm::NoURLSourceId());
}

}  // namespace glic
