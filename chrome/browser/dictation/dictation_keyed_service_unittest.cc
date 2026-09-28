// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/dictation/dictation_keyed_service.h"

#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/dictation/metrics.h"
#include "chrome/browser/dictation/reviewing_page_status_ui_controller.h"
#include "chrome/browser/dictation/target.h"
#include "chrome/browser/dictation/test_util.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_profile.h"
#include "components/prefs/pref_service.h"
#include "components/tabs/public/mock_tab_interface.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace dictation {

class DictationKeyedServiceTest : public testing::Test,
                                  public testing::WithParamInterface<bool> {
 public:
  DictationKeyedServiceTest()
      : scoped_feature_list_(CreateEnablingFeatureList(GetParam())),
        web_contents_(
            content::WebContentsTester::CreateTestWebContents(&profile_,
                                                              nullptr)),
        tab_weak_factory_(&tab_) {
    content::WebContentsTester::For(web_contents_.get())
        ->NavigateAndCommit(GURL("https://example.com/initial"));
    ON_CALL(tab_, GetWeakPtr())
        .WillByDefault(testing::Return(tab_weak_factory_.GetWeakPtr()));
    ON_CALL(tab_, GetContents())
        .WillByDefault(testing::Return(web_contents_.get()));
    profile_.GetPrefs()->SetBoolean(prefs::kPrefDictationOnboardingCompleted,
                                    true);
    service_ = std::make_unique<MockDictationKeyedService>(&profile_);
  }
  ~DictationKeyedServiceTest() override = default;

 protected:
  bool StartSessionAndGetReviewingPageStatus(tabs::TabInterface& tab) {
    service_->StartSessionForTesting(tab, EmptyTarget(),
                                     DictationSessionEntryPoint::kContextMenu);
    auto* controller = service_->session_controller();
    CHECK(controller);
    auto* mock_ui = static_cast<MockSessionUi*>(controller->ui_for_testing());
    CHECK(mock_ui);
    const bool shown = mock_ui->show_reviewing_page_status();
    service_->EndSession();
    return shown;
  }

  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  content::RenderViewHostTestEnabler rvh_test_enabler_;
  TestingProfile profile_;
  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<content::WebContents> web_contents_;
  tabs::MockTabInterface tab_;
  base::WeakPtrFactory<tabs::TabInterface> tab_weak_factory_;
  std::unique_ptr<MockDictationKeyedService> service_;
};

INSTANTIATE_TEST_SUITE_P(All, DictationKeyedServiceTest, testing::Bool());

// Ending a non-existent session should not crash.
TEST_P(DictationKeyedServiceTest, EndSessionDoesNotCrash) {
  ASSERT_EQ(service_->session_controller(), nullptr);
  service_->EndSession();
}

TEST_P(DictationKeyedServiceTest, StartSessionWithNullTarget) {
  ASSERT_EQ(service_->session_controller(), nullptr);
  service_->StartSessionForTesting(tab_, EmptyTarget(),
                                   DictationSessionEntryPoint::kContextMenu);
  EXPECT_NE(service_->session_controller(), nullptr);
}

TEST_P(DictationKeyedServiceTest, EndSessionRemovesController) {
  service_->StartSessionForTesting(tab_, EmptyTarget(),
                                   DictationSessionEntryPoint::kContextMenu);
  ASSERT_NE(service_->session_controller(), nullptr);
  service_->EndSession();
  EXPECT_EQ(service_->session_controller(), nullptr);
}

TEST_P(DictationKeyedServiceTest,
       RecordsMetricsOnInitializationAndStartSession) {
  base::HistogramTester histogram_tester;

  auto service = std::make_unique<MockDictationKeyedService>(&profile_);
  histogram_tester.ExpectUniqueSample(kIsEnabledOnProfileInitHistogramName,
                                      true, 1);

  service->StartSessionForTesting(tab_, EmptyTarget(),
                                  DictationSessionEntryPoint::kContextMenu);
  histogram_tester.ExpectUniqueSample(kSessionStartSourceHistogramName,
                                      DictationSessionEntryPoint::kContextMenu,
                                      1);
  histogram_tester.ExpectUniqueSample(
      kStreamStartTriggerHistogramName,
      DictationStreamStartTrigger::kSessionStart, 1);
}

TEST_P(DictationKeyedServiceTest, RecordsMetricsForStartButton) {
  if (GetParam()) {
    GTEST_SKIP()
        << "Multiple streams per session are not possible in this config.";
  }

  base::HistogramTester histogram_tester;

  auto service = std::make_unique<MockDictationKeyedService>(&profile_);
  service->StartSessionForTesting(tab_, EmptyTarget(),
                                  DictationSessionEntryPoint::kContextMenu);
  histogram_tester.ExpectBucketCount(kStreamStartTriggerHistogramName,
                                     DictationStreamStartTrigger::kSessionStart,
                                     1);

  auto* controller = service->session_controller();
  ASSERT_NE(controller, nullptr);
  auto* stream_provider = controller->attached_stream_provider();
  ASSERT_NE(stream_provider, nullptr);

  controller->UiRequestEndActiveStream();

  EXPECT_CALL(static_cast<MockStreamProvider&>(*stream_provider), GetState())
      .WillRepeatedly(testing::Return(StreamProvider::StreamState::kComplete));
  controller->DidUpdateStreamProviderState(
      *stream_provider, StreamProvider::StreamState::kInitializing);
  EXPECT_TRUE(base::test::RunUntil(
      [&]() { return controller->GetState() == SessionState::kInactive; }));

  controller->UiRequestStartStream();

  histogram_tester.ExpectBucketCount(kStreamStartTriggerHistogramName,
                                     DictationStreamStartTrigger::kStartButton,
                                     1);
  histogram_tester.ExpectTotalCount(kStreamStartTriggerHistogramName, 2);
}

TEST_P(DictationKeyedServiceTest, UpdateAudioLevelPropagatesToController) {
  service_->StartSessionForTesting(tab_, EmptyTargetId(),
                                   DictationSessionEntryPoint::kContextMenu);
  auto* controller = service_->session_controller();
  ASSERT_NE(controller, nullptr);

  auto* mock_ui = static_cast<MockSessionUi*>(controller->ui_for_testing());
  ASSERT_NE(mock_ui, nullptr);

  EXPECT_CALL(*mock_ui, UpdateAudioLevel(0.5f));
  service_->UpdateAudioLevel(0.5f);
}

TEST_P(DictationKeyedServiceTest, HotkeyIgnoredIfNoActiveBrowser) {
  ASSERT_EQ(service_->session_controller(), nullptr);
  service_->ToggleHotkeyHandler();
  EXPECT_EQ(service_->session_controller(), nullptr);
}

TEST_P(DictationKeyedServiceTest, HotkeyManagerLifecycle) {
  EXPECT_NE(service_->local_hotkey_manager_for_testing(), nullptr);

  profile_.GetPrefs()->SetInteger(prefs::kVoiceTypingSettings, 2);
  EXPECT_EQ(service_->local_hotkey_manager_for_testing(), nullptr);

  profile_.GetPrefs()->SetInteger(prefs::kVoiceTypingSettings, 0);
  EXPECT_NE(service_->local_hotkey_manager_for_testing(), nullptr);
}

TEST_P(DictationKeyedServiceTest, TabChangedCallbackNotified) {
  int callback_count = 0;
  base::CallbackListSubscription subscription =
      service_->AddDictationTabChangedCallback(base::BindLambdaForTesting(
          [&callback_count](tabs::TabInterface* tab) { callback_count++; }));

  EXPECT_EQ(callback_count, 1);
  EXPECT_EQ(service_->GetActiveDictationTab(), nullptr);

  service_->StartSessionForTesting(tab_, EmptyTarget(),
                                   DictationSessionEntryPoint::kContextMenu);
  EXPECT_EQ(callback_count, 2);
  EXPECT_EQ(service_->GetActiveDictationTab(), &tab_);

  service_->EndSession();
  EXPECT_EQ(callback_count, 3);
  EXPECT_EQ(service_->GetActiveDictationTab(), nullptr);
}

TEST_P(DictationKeyedServiceTest,
       ReviewingPageStatusExpiresAfter60MinutesOnSameDocument) {
  // First invocation on the open document shows the disclaimer.
  EXPECT_TRUE(StartSessionAndGetReviewingPageStatus(tab_));

  // Subsequent invocation before 60 minutes have elapsed is suppressed and
  // does not reset the 60-minute expiration timer.
  task_environment_.FastForwardBy(base::Minutes(20));
  EXPECT_FALSE(StartSessionAndGetReviewingPageStatus(tab_));

  // Once the 60-minute cooldown has elapsed with the same page left open,
  // the disclaimer is shown again and starts a new 60-minute cooldown.
  task_environment_.FastForwardBy(
      ReviewingPageStatusUiController::kCooldownPeriod - base::Minutes(20));
  EXPECT_TRUE(StartSessionAndGetReviewingPageStatus(tab_));
  EXPECT_FALSE(StartSessionAndGetReviewingPageStatus(tab_));
}

TEST_P(DictationKeyedServiceTest,
       ReviewingPageStatusShownOnCrossDocumentButNotSameDocumentNavigation) {
  EXPECT_TRUE(StartSessionAndGetReviewingPageStatus(tab_));

  // A same-document navigation (e.g. fragment change or SPA pushState) keeps
  // the same content::Page, so the PageUserData flag suppresses the disclaimer.
  content::NavigationSimulator::CreateRendererInitiated(
      GURL("https://example.com/initial#section"),
      web_contents_->GetPrimaryMainFrame())
      ->CommitSameDocument();
  EXPECT_FALSE(StartSessionAndGetReviewingPageStatus(tab_));

  // A cross-document navigation creates a new content::Page, so the disclaimer
  // is shown immediately.
  content::WebContentsTester::For(web_contents_.get())
      ->NavigateAndCommit(GURL("https://example.com/new-page"));
  EXPECT_TRUE(StartSessionAndGetReviewingPageStatus(tab_));
}

TEST_P(DictationKeyedServiceTest,
       ReviewingPageStatusTrackedIndependentlyPerDocument) {
  std::unique_ptr<content::WebContents> web_contents_2 =
      content::WebContentsTester::CreateTestWebContents(&profile_, nullptr);
  content::WebContentsTester::For(web_contents_2.get())
      ->NavigateAndCommit(GURL("https://example.com/initial"));

  tabs::MockTabInterface tab_2;
  base::WeakPtrFactory<tabs::TabInterface> tab_2_weak_factory(&tab_2);
  ON_CALL(tab_2, GetWeakPtr())
      .WillByDefault(testing::Return(tab_2_weak_factory.GetWeakPtr()));
  ON_CALL(tab_2, GetContents())
      .WillByDefault(testing::Return(web_contents_2.get()));

  // First invocation in Tab 1 on https://example.com/initial shows the
  // disclaimer and puts Tab 1's document on cooldown.
  EXPECT_TRUE(StartSessionAndGetReviewingPageStatus(tab_));

  // Opening a second tab to the same URL (https://example.com/initial) is a
  // separate document instance, so the disclaimer is shown there too.
  task_environment_.FastForwardBy(base::Minutes(5));
  EXPECT_TRUE(StartSessionAndGetReviewingPageStatus(tab_2));

  // Subsequent invocation in Tab 2 within its 60-minute cooldown is suppressed.
  EXPECT_FALSE(StartSessionAndGetReviewingPageStatus(tab_2));

  // After 55 more minutes (60 minutes total for Tab 1, 55 minutes for Tab 2),
  // Tab 1's cooldown has expired while Tab 2's cooldown is still active.
  task_environment_.FastForwardBy(
      ReviewingPageStatusUiController::kCooldownPeriod - base::Minutes(5));
  EXPECT_TRUE(StartSessionAndGetReviewingPageStatus(tab_));
  EXPECT_FALSE(StartSessionAndGetReviewingPageStatus(tab_2));
}

}  // namespace dictation
