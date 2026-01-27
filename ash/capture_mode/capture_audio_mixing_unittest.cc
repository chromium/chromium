// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/capture_mode/capture_mode_controller.h"
#include "ash/capture_mode/capture_mode_menu_group.h"
#include "ash/capture_mode/capture_mode_session_test_api.h"
#include "ash/capture_mode/capture_mode_settings_test_api.h"
#include "ash/capture_mode/capture_mode_settings_view.h"
#include "ash/capture_mode/capture_mode_test_util.h"
#include "ash/capture_mode/capture_mode_types.h"
#include "ash/capture_mode/test_capture_mode_delegate.h"
#include "ash/public/cpp/capture_mode/capture_mode_test_api.h"
#include "ash/style/icon_button.h"
#include "ash/test/ash_test_base.h"
#include "ash/test/ash_test_util.h"
#include "base/test/metrics/histogram_tester.h"

namespace ash {

namespace {

CaptureModeController* StartSession() {
  return StartCaptureSession(CaptureModeSource::kFullscreen,
                             CaptureModeType::kVideo);
}

bool IsAudioOptionChecked(int option_id) {
  return CaptureModeSettingsTestApi().GetAudioInputMenuGroup()->IsOptionChecked(
      option_id);
}

}  // namespace

using CaptureAudioMixingTest = AshTestBase;

TEST_F(CaptureAudioMixingTest, AudioSettingsMenu) {
  auto* controller = StartSession();
  auto* event_generator = GetEventGenerator();

  // Open the settings menu, and check the currently selected audio mode.
  ClickOnView(GetSettingsButton(), event_generator);
  EXPECT_EQ(AudioRecordingMode::kOff,
            controller->GetEffectiveAudioRecordingMode());

  {
    CaptureModeSettingsTestApi test_api;
    views::View* microphone_option = test_api.GetMicrophoneOption();
    views::View* system_option = test_api.GetSystemAudioOption();
    views::View* system_and_microphone_option =
        test_api.GetSystemAndMicrophoneAudioOption();

    // All the options should exist, but only the "off" option should be
    // selected.
    EXPECT_TRUE(microphone_option);
    EXPECT_TRUE(system_option);
    EXPECT_TRUE(system_and_microphone_option);
    EXPECT_TRUE(IsAudioOptionChecked(kAudioOff));
    EXPECT_FALSE(IsAudioOptionChecked(kAudioMicrophone));
    EXPECT_FALSE(IsAudioOptionChecked(kAudioSystem));
    EXPECT_FALSE(IsAudioOptionChecked(kAudioSystemAndMicrophone));

    // Clicking on the system audio should select that option and update the
    // controller.
    ClickOnView(system_option, event_generator);
    EXPECT_FALSE(IsAudioOptionChecked(kAudioOff));
    EXPECT_FALSE(IsAudioOptionChecked(kAudioMicrophone));
    EXPECT_TRUE(IsAudioOptionChecked(kAudioSystem));
    EXPECT_FALSE(IsAudioOptionChecked(kAudioSystemAndMicrophone));
    EXPECT_EQ(AudioRecordingMode::kSystem,
              controller->GetEffectiveAudioRecordingMode());

    // Likewise for the system+microphone option.
    ClickOnView(system_and_microphone_option, event_generator);
    EXPECT_FALSE(IsAudioOptionChecked(kAudioOff));
    EXPECT_FALSE(IsAudioOptionChecked(kAudioMicrophone));
    EXPECT_FALSE(IsAudioOptionChecked(kAudioSystem));
    EXPECT_TRUE(IsAudioOptionChecked(kAudioSystemAndMicrophone));
    EXPECT_EQ(AudioRecordingMode::kSystemAndMicrophone,
              controller->GetEffectiveAudioRecordingMode());
  }

  // Exit the session, and start a new one. The most recent audio setting will
  // be remembered.
  SendKey(ui::VKEY_ESCAPE, event_generator);
  StartSession();
  ClickOnView(GetSettingsButton(), event_generator);
  EXPECT_FALSE(IsAudioOptionChecked(kAudioOff));
  EXPECT_FALSE(IsAudioOptionChecked(kAudioMicrophone));
  EXPECT_FALSE(IsAudioOptionChecked(kAudioSystem));
  EXPECT_TRUE(IsAudioOptionChecked(kAudioSystemAndMicrophone));
}

TEST_F(CaptureAudioMixingTest, KeyboardNavigation) {
  auto* controller = StartSession();
  using FocusGroup = CaptureModeSessionFocusCycler::FocusGroup;
  CaptureModeSessionTestApi test_api(controller->capture_mode_session());

  auto* event_generator = GetEventGenerator();
  SendKey(ui::VKEY_TAB, event_generator, ui::EF_NONE, /*count=*/6);
  EXPECT_EQ(test_api.GetCurrentFocusedView()->GetView(), GetSettingsButton());
  // Press the space bar, and the settings menu should open, ready for keyboard
  // navigation.
  SendKey(ui::VKEY_SPACE, event_generator);
  EXPECT_EQ(FocusGroup::kPendingSettings, test_api.GetCurrentFocusGroup());
  CaptureModeSettingsView* settings_menu =
      test_api.GetCaptureModeSettingsView();
  ASSERT_TRUE(settings_menu);

  CaptureModeSettingsTestApi settings_test_api;
  // Tab twice, the current focused view is the "Off" option.
  SendKey(ui::VKEY_TAB, event_generator, ui::EF_NONE, /*count=*/2);
  EXPECT_EQ(test_api.GetCurrentFocusedView()->GetView(),
            settings_test_api.GetAudioOffOption());
  // Next tabs will go through all the audio options.
  SendKey(ui::VKEY_TAB, event_generator);
  EXPECT_EQ(test_api.GetCurrentFocusedView()->GetView(),
            settings_test_api.GetSystemAudioOption());
  SendKey(ui::VKEY_TAB, event_generator);
  EXPECT_EQ(test_api.GetCurrentFocusedView()->GetView(),
            settings_test_api.GetMicrophoneOption());
  SendKey(ui::VKEY_TAB, event_generator);
  EXPECT_EQ(test_api.GetCurrentFocusedView()->GetView(),
            settings_test_api.GetSystemAndMicrophoneAudioOption());
}

TEST_F(CaptureAudioMixingTest, ServiceWillRecordAudio) {
  constexpr char kHistogramNameBase[] = "AudioRecordingMode";
  const std::string histogram_name = BuildHistogramName(
      kHistogramNameBase, /*behavior=*/nullptr, /*append_ui_mode_suffix=*/true);

  struct {
    const char* const scope_name;
    AudioRecordingMode audio_mode;
    int expected_number_of_audio_capturers;
  } kTestCases[] = {
      {"Off", AudioRecordingMode::kOff,
       /*expected_number_of_audio_capturers=*/0},
      {"Microphone", AudioRecordingMode::kMicrophone,
       /*expected_number_of_audio_capturers=*/1},
      {"System audio", AudioRecordingMode::kSystem,
       /*expected_number_of_audio_capturers=*/1},
      {"System and microphone audio", AudioRecordingMode::kSystemAndMicrophone,
       /*expected_number_of_audio_capturers=*/2},
  };

  for (const auto& test_case : kTestCases) {
    SCOPED_TRACE(test_case.scope_name);

    base::HistogramTester histogram_tester;
    histogram_tester.ExpectBucketCount(histogram_name, test_case.audio_mode, 0);

    auto* controller = StartSession();
    controller->SetAudioRecordingMode(test_case.audio_mode);

    StartVideoRecordingImmediately();

    EXPECT_TRUE(controller->is_recording_in_progress());
    auto* test_delegate = static_cast<TestCaptureModeDelegate*>(
        controller->delegate_for_testing());
    CaptureModeTestApi().FlushRecordingServiceForTesting();
    EXPECT_EQ(test_case.expected_number_of_audio_capturers,
              test_delegate->GetNumberOfAudioCapturers());
    controller->EndVideoRecording(EndRecordingReason::kStopRecordingButton);

    WaitForCaptureFileToBeSaved();

    histogram_tester.ExpectBucketCount(histogram_name, test_case.audio_mode, 1);
  }
}

// -----------------------------------------------------------------------------
// ProjectorAudioMixingTest:

class ProjectorAudioMixingTest : public CaptureAudioMixingTest {
 public:
  ProjectorAudioMixingTest() = default;
  ~ProjectorAudioMixingTest() override = default;

  // CaptureAudioMixingTest:
  void SetUp() override {
    CaptureAudioMixingTest::SetUp();
    projector_helper_.SetUp();
  }

  void StartProjectorModeSession() {
    projector_helper_.StartProjectorModeSession();
  }

 private:
  ProjectorCaptureModeIntegrationHelper projector_helper_;
};

TEST_F(ProjectorAudioMixingTest, AudioSettingsMenu) {
  constexpr char kHistogramNameBase[] = "AudioRecordingMode";
  const std::string histogram_name = BuildHistogramName(
      kHistogramNameBase,
      CaptureModeTestApi().GetBehavior(BehaviorType::kProjector),
      /*append_ui_mode_suffix=*/true);
  base::HistogramTester histogram_tester;

  StartProjectorModeSession();
  auto* event_generator = GetEventGenerator();

  // Open the settings menu, and check that only microphone and
  // system+microphone options are available.
  ClickOnView(GetSettingsButton(), event_generator);

  CaptureModeSettingsTestApi test_api;
  views::View* off_option = test_api.GetAudioOffOption();
  views::View* microphone_option = test_api.GetMicrophoneOption();
  views::View* system_option = test_api.GetSystemAudioOption();
  views::View* system_and_microphone_option =
      test_api.GetSystemAndMicrophoneAudioOption();

  EXPECT_FALSE(off_option);
  EXPECT_FALSE(system_option);
  EXPECT_TRUE(microphone_option);
  EXPECT_TRUE(system_and_microphone_option);

  // Microphone should still be selected by default.
  EXPECT_TRUE(IsAudioOptionChecked(kAudioMicrophone));
  EXPECT_FALSE(IsAudioOptionChecked(kAudioSystemAndMicrophone));

  // End the session and expect the correct audio mode was recorded.
  auto* controller = CaptureModeController::Get();
  controller->Stop();
  histogram_tester.ExpectBucketCount(histogram_name,
                                     AudioRecordingMode::kMicrophone, 1);
  histogram_tester.ExpectBucketCount(
      histogram_name, AudioRecordingMode::kSystemAndMicrophone, 0);

  // Start a new session and select `kSystemAndMicrophone`, and expect the
  // correct metrics will be recorded when the session ends.
  StartProjectorModeSession();
  controller->SetAudioRecordingMode(AudioRecordingMode::kSystemAndMicrophone);
  controller->Stop();

  histogram_tester.ExpectBucketCount(histogram_name,
                                     AudioRecordingMode::kMicrophone, 1);
  histogram_tester.ExpectBucketCount(
      histogram_name, AudioRecordingMode::kSystemAndMicrophone, 1);
}

}  // namespace ash
