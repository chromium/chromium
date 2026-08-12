// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/dictation/dictation_keyed_service.h"

#include "base/test/metrics/histogram_tester.h"
#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/dictation/metrics.h"
#include "chrome/browser/dictation/target.h"
#include "chrome/browser/dictation/test_util.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_profile.h"
#include "components/prefs/pref_service.h"
#include "components/tabs/public/mock_tab_interface.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace dictation {

class DictationKeyedServiceTest : public testing::Test {
 public:
  DictationKeyedServiceTest()
      : scoped_feature_list_(CreateEnablingFeatureList()) {
    profile_.GetPrefs()->SetBoolean(prefs::kPrefDictationOnboardingCompleted,
                                    true);
    service_ = std::make_unique<MockDictationKeyedService>(&profile_);
  }
  ~DictationKeyedServiceTest() override = default;

 protected:
  content::BrowserTaskEnvironment task_environment_;
  TestingProfile profile_;
  base::test::ScopedFeatureList scoped_feature_list_;
  tabs::MockTabInterface tab_;
  std::unique_ptr<MockDictationKeyedService> service_;
};

// Ending a non-existent session should not crash.
TEST_F(DictationKeyedServiceTest, EndSessionDoesNotCrash) {
  ASSERT_EQ(service_->session_controller(), nullptr);
  service_->EndSession();
}

TEST_F(DictationKeyedServiceTest, StartSessionWithNullTarget) {
  ASSERT_EQ(service_->session_controller(), nullptr);
  service_->StartSessionForTesting(tab_, EmptyTarget(),
                                   DictationSessionEntryPoint::kContextMenu);
  EXPECT_NE(service_->session_controller(), nullptr);
}

TEST_F(DictationKeyedServiceTest, EndSessionRemovesController) {
  service_->StartSessionForTesting(tab_, EmptyTarget(),
                                   DictationSessionEntryPoint::kContextMenu);
  ASSERT_NE(service_->session_controller(), nullptr);
  service_->EndSession();
  EXPECT_EQ(service_->session_controller(), nullptr);
}

TEST_F(DictationKeyedServiceTest,
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

TEST_F(DictationKeyedServiceTest, RecordsMetricsForStartButton) {
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

TEST_F(DictationKeyedServiceTest, UpdateAudioLevelPropagatesToController) {
  service_->StartSessionForTesting(tab_, EmptyTargetId(),
                                   DictationSessionEntryPoint::kContextMenu);
  auto* controller = service_->session_controller();
  ASSERT_NE(controller, nullptr);

  auto* mock_ui = static_cast<MockSessionUi*>(controller->ui_for_testing());
  ASSERT_NE(mock_ui, nullptr);

  EXPECT_CALL(*mock_ui, UpdateAudioLevel(0.5f));
  service_->UpdateAudioLevel(0.5f);
}

TEST_F(DictationKeyedServiceTest, HotkeyIgnoredIfNoActiveBrowser) {
  ASSERT_EQ(service_->session_controller(), nullptr);
  service_->ToggleHotkeyHandler();
  EXPECT_EQ(service_->session_controller(), nullptr);
}

TEST_F(DictationKeyedServiceTest, HotkeyManagerLifecycle) {
  EXPECT_NE(service_->local_hotkey_manager_for_testing(), nullptr);

  profile_.GetPrefs()->SetInteger(prefs::kVoiceTypingSettings, 2);
  EXPECT_EQ(service_->local_hotkey_manager_for_testing(), nullptr);

  profile_.GetPrefs()->SetInteger(prefs::kVoiceTypingSettings, 0);
  EXPECT_NE(service_->local_hotkey_manager_for_testing(), nullptr);
}

}  // namespace dictation
