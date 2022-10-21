// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/capture_mode/capture_mode_bar_view.h"
#include "ash/capture_mode/capture_mode_controller.h"
#include "ash/capture_mode/capture_mode_demo_tools_controller.h"
#include "ash/capture_mode/capture_mode_menu_toggle_button.h"
#include "ash/capture_mode/capture_mode_session_test_api.h"
#include "ash/capture_mode/capture_mode_settings_test_api.h"
#include "ash/capture_mode/capture_mode_settings_view.h"
#include "ash/capture_mode/capture_mode_test_util.h"
#include "ash/capture_mode/capture_mode_toggle_button.h"
#include "ash/capture_mode/capture_mode_types.h"
#include "ash/constants/ash_features.h"
#include "ash/test/ash_test_base.h"
#include "base/test/scoped_feature_list.h"
#include "ui/events/event_constants.h"
#include "ui/events/keycodes/keyboard_codes_posix.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/controls/button/toggle_button.h"

namespace ash {

class CaptureModeDemoToolsTest : public AshTestBase {
 public:
  CaptureModeDemoToolsTest() = default;
  CaptureModeDemoToolsTest(const CaptureModeDemoToolsTest&) = delete;
  CaptureModeDemoToolsTest& operator=(const CaptureModeDemoToolsTest&) = delete;
  ~CaptureModeDemoToolsTest() override = default;

  // AshTestBase:
  void SetUp() override {
    scoped_feature_list_.InitAndEnableFeature(features::kCaptureModeDemoTools);
    AshTestBase::SetUp();
    window_ = CreateTestWindow(gfx::Rect(100, 200, 500, 600));
  }

  void TearDown() override {
    window_.reset();
    AshTestBase::TearDown();
  }

  CaptureModeController* StartVideoRecordingWithGivenSource(
      CaptureModeSource source) {
    auto* controller = CaptureModeController::Get();
    const gfx::Rect capture_region(30, 50, 200, 500);
    controller->SetUserCaptureRegion(capture_region, true);
    StartCaptureSession(source, CaptureModeType::kVideo);

    if (source == CaptureModeSource::kWindow)
      GetEventGenerator()->MoveMouseToCenterOf(window_.get());

    StartVideoRecordingImmediately();
    return controller;
  }

  CaptureModeToggleButton* GetSettingsButton() const {
    return GetCaptureModeBarView()->settings_button();
  }

  views::Widget* GetCaptureModeSettingsWidget() const {
    auto* session = CaptureModeController::Get()->capture_mode_session();
    DCHECK(session);
    return CaptureModeSessionTestApi(session).GetCaptureModeSettingsWidget();
  }

  CaptureModeDemoToolsController* GetCaptureModeDemoToolsController() const {
    auto* recording_watcher =
        CaptureModeController::Get()->video_recording_watcher_for_testing();
    DCHECK(recording_watcher);
    return recording_watcher->demo_tools_controller_for_testing();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<aura::Window> window_;
};

// Tests that the key event is considered to generate the `demo_tools_widget_`
// or ignored otherwise in a correct way.
TEST_F(CaptureModeDemoToolsTest, ConsiderKeyEvent) {
  CaptureModeController* controller = StartCaptureSession(
      CaptureModeSource::kFullscreen, CaptureModeType::kVideo);
  auto* event_generator = GetEventGenerator();
  ClickOnView(GetSettingsButton(), event_generator);
  EXPECT_TRUE(GetCaptureModeSettingsWidget());
  views::ToggleButton* toggle_button = CaptureModeSettingsTestApi()
                                           .GetDemoToolsMenuToggleButton()
                                           ->toggle_button_for_testing();

  // The toggle button will be disabled by default, toggle the toggle button to
  // enable the demo tools feature.
  EXPECT_FALSE(toggle_button->GetIsOn());
  ClickOnView(toggle_button, event_generator);
  StartVideoRecordingImmediately();
  EXPECT_TRUE(controller->is_recording_in_progress());

  CaptureModeDemoToolsController* demo_tools_controller =
      GetCaptureModeDemoToolsController();
  EXPECT_TRUE(demo_tools_controller);

  // Press the 'A' key and the event will not be considered to generate a
  // corresponding key widget.
  event_generator->PressKey(ui::VKEY_A, ui::EF_NONE);
  EXPECT_FALSE(demo_tools_controller->demo_tools_widget_for_testing());
  event_generator->ReleaseKey(ui::VKEY_A, ui::EF_NONE);
  EXPECT_EQ(demo_tools_controller->modifiers_for_testing(), 0);
  EXPECT_EQ(demo_tools_controller->last_non_modifier_key_for_testing(),
            ui::VKEY_UNKNOWN);

  // Press Ctrl + A the key event will be considered to generate a
  // corresponding key widget.
  event_generator->PressKey(ui::VKEY_A, ui::EF_NONE);
  event_generator->PressKey(ui::VKEY_CONTROL, ui::EF_NONE);
  EXPECT_TRUE(demo_tools_controller->demo_tools_widget_for_testing());
  EXPECT_EQ(demo_tools_controller->modifiers_for_testing(),
            ui::EF_CONTROL_DOWN);
  EXPECT_EQ(demo_tools_controller->last_non_modifier_key_for_testing(),
            ui::VKEY_A);

  event_generator->ReleaseKey(ui::VKEY_A, ui::EF_NONE);
  event_generator->ReleaseKey(ui::VKEY_CONTROL, ui::EF_NONE);
  EXPECT_FALSE(demo_tools_controller->demo_tools_widget_for_testing());
  EXPECT_EQ(demo_tools_controller->modifiers_for_testing(), 0);
  EXPECT_EQ(demo_tools_controller->last_non_modifier_key_for_testing(),
            ui::VKEY_UNKNOWN);

  event_generator->PressKey(ui::VKEY_TAB, ui::EF_NONE);
  EXPECT_TRUE(demo_tools_controller->demo_tools_widget_for_testing());
  EXPECT_EQ(demo_tools_controller->modifiers_for_testing(), 0);
  EXPECT_EQ(demo_tools_controller->last_non_modifier_key_for_testing(),
            ui::VKEY_TAB);
}

// Tests that the capture mode demo tools feature will be enabled if the
// toggle button is enabled and disabled otherwise.
TEST_F(CaptureModeDemoToolsTest, EntryPointTest) {
  CaptureModeController* controller = StartCaptureSession(
      CaptureModeSource::kFullscreen, CaptureModeType::kVideo);
  auto* event_generator = GetEventGenerator();
  ClickOnView(GetSettingsButton(), event_generator);
  EXPECT_TRUE(GetCaptureModeSettingsWidget());
  views::ToggleButton* toggle_button = CaptureModeSettingsTestApi()
                                           .GetDemoToolsMenuToggleButton()
                                           ->toggle_button_for_testing();

  // The toggle button will be disabled by default.
  EXPECT_FALSE(toggle_button->GetIsOn());

  // Toggle the demo tools toggle button to enable the feature and start the
  // video recording. The modifier key down event will be handled and the key
  // combo viewer widget will be displayed.
  EXPECT_TRUE(GetCaptureModeSettingsWidget());
  ClickOnView(toggle_button, event_generator);
  EXPECT_TRUE(toggle_button->GetIsOn());
  StartVideoRecordingImmediately();
  EXPECT_TRUE(controller->is_recording_in_progress());
  event_generator->PressKey(ui::VKEY_CONTROL, ui::EF_NONE);
  CaptureModeDemoToolsController* demo_tools_controller =
      GetCaptureModeDemoToolsController();
  EXPECT_TRUE(demo_tools_controller);
  EXPECT_TRUE(demo_tools_controller->demo_tools_widget_for_testing());
  controller->EndVideoRecording(EndRecordingReason::kStopRecordingButton);
  WaitForCaptureFileToBeSaved();
  EXPECT_FALSE(controller->IsActive());

  // Start another capture mode session and the demo tools toggle button will be
  // enabled. Toggle the toggle button to disable the feature. The modifier key
  // down event will not be handled when video recording starts.
  controller = StartCaptureSession(CaptureModeSource::kFullscreen,
                                   CaptureModeType::kVideo);
  ClickOnView(GetSettingsButton(), event_generator);
  EXPECT_TRUE(GetCaptureModeSettingsWidget());
  toggle_button = CaptureModeSettingsTestApi()
                      .GetDemoToolsMenuToggleButton()
                      ->toggle_button_for_testing();
  EXPECT_TRUE(toggle_button->GetIsOn());
  ClickOnView(toggle_button, event_generator);
  StartVideoRecordingImmediately();
  EXPECT_TRUE(controller->is_recording_in_progress());
  event_generator->PressKey(ui::VKEY_CONTROL, ui::EF_NONE);
  EXPECT_FALSE(GetCaptureModeDemoToolsController());
}

}  // namespace ash