// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "ash/capture_mode/capture_mode_bar_view.h"
#include "ash/capture_mode/capture_mode_constants.h"
#include "ash/capture_mode/capture_mode_controller.h"
#include "ash/capture_mode/capture_mode_demo_tools_controller.h"
#include "ash/capture_mode/capture_mode_demo_tools_test_api.h"
#include "ash/capture_mode/capture_mode_menu_toggle_button.h"
#include "ash/capture_mode/capture_mode_session_test_api.h"
#include "ash/capture_mode/capture_mode_settings_test_api.h"
#include "ash/capture_mode/capture_mode_settings_view.h"
#include "ash/capture_mode/capture_mode_test_util.h"
#include "ash/capture_mode/capture_mode_types.h"
#include "ash/capture_mode/key_combo_view.h"
#include "ash/constants/ash_features.h"
#include "ash/style/icon_button.h"
#include "ash/test/ash_test_base.h"
#include "base/test/scoped_feature_list.h"
#include "base/timer/timer.h"
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
    window_ = CreateTestWindow(gfx::Rect(20, 30, 601, 300));
  }

  void TearDown() override {
    window_.reset();
    AshTestBase::TearDown();
  }

  aura::Window* window() const { return window_.get(); }

  IconButton* GetSettingsButton() const {
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
  CaptureModeDemoToolsTestApi capture_mode_demo_tools_test_api(
      demo_tools_controller);
  EXPECT_FALSE(capture_mode_demo_tools_test_api.GetDemoToolsWidget());
  event_generator->ReleaseKey(ui::VKEY_A, ui::EF_NONE);
  EXPECT_EQ(capture_mode_demo_tools_test_api.GetCurrentModifiersFlags(), 0);
  EXPECT_EQ(capture_mode_demo_tools_test_api.GetLastNonModifierKey(),
            ui::VKEY_UNKNOWN);

  // Press 'Ctrl' + 'A' and the key event will be considered to generate a
  // corresponding key widget.
  event_generator->PressKey(ui::VKEY_A, ui::EF_NONE);
  event_generator->PressKey(ui::VKEY_CONTROL, ui::EF_NONE);
  EXPECT_TRUE(capture_mode_demo_tools_test_api.GetDemoToolsWidget());
  EXPECT_EQ(capture_mode_demo_tools_test_api.GetCurrentModifiersFlags(),
            ui::EF_CONTROL_DOWN);
  EXPECT_EQ(capture_mode_demo_tools_test_api.GetLastNonModifierKey(),
            ui::VKEY_A);

  event_generator->ReleaseKey(ui::VKEY_A, ui::EF_NONE);
  base::OneShotTimer* hide_timer =
      capture_mode_demo_tools_test_api.GetKeyComboHideTimer();
  EXPECT_TRUE(hide_timer->IsRunning());
  hide_timer->FireNow();
  event_generator->ReleaseKey(ui::VKEY_CONTROL, ui::EF_NONE);
  EXPECT_FALSE(capture_mode_demo_tools_test_api.GetDemoToolsWidget());
  EXPECT_EQ(capture_mode_demo_tools_test_api.GetCurrentModifiersFlags(), 0);
  EXPECT_EQ(capture_mode_demo_tools_test_api.GetLastNonModifierKey(),
            ui::VKEY_UNKNOWN);

  event_generator->PressKey(ui::VKEY_TAB, ui::EF_NONE);
  EXPECT_TRUE(capture_mode_demo_tools_test_api.GetDemoToolsWidget());
  EXPECT_EQ(capture_mode_demo_tools_test_api.GetCurrentModifiersFlags(), 0);
  EXPECT_EQ(capture_mode_demo_tools_test_api.GetLastNonModifierKey(),
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
  CaptureModeDemoToolsTestApi capture_mode_demo_tools_test_api(
      demo_tools_controller);
  EXPECT_TRUE(capture_mode_demo_tools_test_api.GetDemoToolsWidget());
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

// Tests that the key combo viewer widget displays the expected contents on key
// event and the modifier key should always be displayed before the non-modifier
// key. With no modifier keys or no non-modifier key that can be displayed
// independently, the key combo widget will not be displayed.
TEST_F(CaptureModeDemoToolsTest, KeyComboWidgetTest) {
  CaptureModeController* controller = StartCaptureSession(
      CaptureModeSource::kFullscreen, CaptureModeType::kVideo);
  controller->EnableDemoTools(true);
  StartVideoRecordingImmediately();
  EXPECT_TRUE(controller->is_recording_in_progress());
  auto* event_generator = GetEventGenerator();
  event_generator->PressKey(ui::VKEY_CONTROL, ui::EF_NONE);
  event_generator->PressKey(ui::VKEY_C, ui::EF_NONE);
  CaptureModeDemoToolsController* demo_tools_controller =
      GetCaptureModeDemoToolsController();
  EXPECT_TRUE(demo_tools_controller);
  CaptureModeDemoToolsTestApi capture_mode_demo_tools_test_api(
      demo_tools_controller);
  EXPECT_TRUE(capture_mode_demo_tools_test_api.GetDemoToolsWidget());
  EXPECT_TRUE(capture_mode_demo_tools_test_api.GetKeyComboView());
  std::vector<ui::KeyboardCode> expected_modifier_key_vector = {
      ui::VKEY_CONTROL};
  EXPECT_EQ(capture_mode_demo_tools_test_api.GetShownModifiersKeyCodes(),
            expected_modifier_key_vector);
  EXPECT_EQ(capture_mode_demo_tools_test_api.GetShownNonModifierKeyCode(),
            ui::VKEY_C);

  // Press the key 'Shift' at last, but it will still show before the 'C' key.
  event_generator->PressKey(ui::VKEY_SHIFT, ui::EF_NONE);
  expected_modifier_key_vector = {ui::VKEY_CONTROL, ui::VKEY_SHIFT};
  EXPECT_TRUE(capture_mode_demo_tools_test_api.GetShownModifiersKeyCodes() ==
              expected_modifier_key_vector);
  EXPECT_EQ(capture_mode_demo_tools_test_api.GetShownNonModifierKeyCode(),
            ui::VKEY_C);

  // Release the modifier keys, and the key combo view will not be displayed.
  event_generator->ReleaseKey(ui::VKEY_SHIFT, ui::EF_NONE);
  event_generator->ReleaseKey(ui::VKEY_CONTROL, ui::EF_NONE);
  EXPECT_FALSE(capture_mode_demo_tools_test_api.GetDemoToolsWidget());
}

// Tests the hide timer behaviors for the key combo view:
// 1. The hide timer will be triggered on key up of the non-modifier key, the
// key combo view will be hidden after the timer expires;
// 2. If there is another key down event happens before the timer expires, the
// hide timer stops and the key combo view will be updated to match the current
// keys pressed;
// 3. On key up of the modifier key while the hide timer is still running, the
// key combo view will stay visible until the timer expires.
TEST_F(CaptureModeDemoToolsTest, DemoToolsHideTimerTest) {
  CaptureModeController* controller = StartCaptureSession(
      CaptureModeSource::kFullscreen, CaptureModeType::kVideo);
  controller->EnableDemoTools(true);
  StartVideoRecordingImmediately();
  EXPECT_TRUE(controller->is_recording_in_progress());
  CaptureModeDemoToolsController* demo_tools_controller =
      GetCaptureModeDemoToolsController();
  EXPECT_TRUE(demo_tools_controller);
  CaptureModeDemoToolsTestApi capture_mode_demo_tools_test_api(
      demo_tools_controller);

  // Press the 'Ctrl' + 'A' and verify the shown key widgets.
  auto* event_generator = GetEventGenerator();
  event_generator->PressKey(ui::VKEY_CONTROL, ui::EF_NONE);
  event_generator->PressKey(ui::VKEY_A, ui::EF_NONE);
  EXPECT_TRUE(capture_mode_demo_tools_test_api.GetDemoToolsWidget());
  KeyComboView* key_combo_view =
      capture_mode_demo_tools_test_api.GetKeyComboView();
  EXPECT_TRUE(key_combo_view);
  std::vector<ui::KeyboardCode> expected_modifier_key_vector = {
      ui::VKEY_CONTROL};
  EXPECT_EQ(capture_mode_demo_tools_test_api.GetShownModifiersKeyCodes(),
            expected_modifier_key_vector);
  EXPECT_EQ(capture_mode_demo_tools_test_api.GetShownNonModifierKeyCode(),
            ui::VKEY_A);

  // Release the non-modifier key and the hide timer will be triggered, the key
  // combo view will hide when the timer expires.
  event_generator->ReleaseKey(ui::VKEY_A, ui::EF_NONE);
  base::OneShotTimer* hide_timer =
      capture_mode_demo_tools_test_api.GetKeyComboHideTimer();
  EXPECT_TRUE(hide_timer->IsRunning());
  EXPECT_EQ(hide_timer->GetCurrentDelay(),
            capture_mode::kDelayToHideKeyComboDuration);

  auto fire_hide_timer_and_verify_widget = [&]() {
    key_combo_view = capture_mode_demo_tools_test_api.GetKeyComboView();
    ViewVisibilityChangeWaiter waiter(key_combo_view);
    hide_timer->FireNow();
    waiter.Wait();
    EXPECT_FALSE(capture_mode_demo_tools_test_api.GetDemoToolsWidget());
    EXPECT_FALSE(capture_mode_demo_tools_test_api.GetKeyComboView());
  };

  fire_hide_timer_and_verify_widget();

  // Press 'Ctrl' + 'Shift' + 'A', then release 'A', the timer will be
  // triggered. Press 'B' and the timer will stop and the key combo view will be
  // updated accordingly, i.e. 'Ctrl' + 'Shift' + 'B'.
  event_generator->PressKey(ui::VKEY_CONTROL, ui::EF_NONE);
  event_generator->PressKey(ui::VKEY_SHIFT, ui::EF_NONE);
  event_generator->PressKey(ui::VKEY_A, ui::EF_NONE);
  EXPECT_TRUE(capture_mode_demo_tools_test_api.GetDemoToolsWidget());
  expected_modifier_key_vector = {ui::VKEY_CONTROL, ui::VKEY_SHIFT};
  EXPECT_EQ(capture_mode_demo_tools_test_api.GetShownModifiersKeyCodes(),
            expected_modifier_key_vector);
  EXPECT_EQ(capture_mode_demo_tools_test_api.GetShownNonModifierKeyCode(),
            ui::VKEY_A);
  event_generator->ReleaseKey(ui::VKEY_A, ui::EF_NONE);
  EXPECT_TRUE(hide_timer->IsRunning());
  event_generator->PressKey(ui::VKEY_B, ui::EF_NONE);
  EXPECT_FALSE(hide_timer->IsRunning());
  EXPECT_EQ(capture_mode_demo_tools_test_api.GetShownNonModifierKeyCode(),
            ui::VKEY_B);

  // Release 'B', the timer will be triggered. Release 'Ctrl' will not hide
  // the 'Ctrl' key combo view on display immediately. Similarly for releasing
  // the 'Shift' key. The entire key combo view will hide after the timer
  // expires.
  event_generator->ReleaseKey(ui::VKEY_B, ui::EF_NONE);
  EXPECT_TRUE(hide_timer->IsRunning());
  EXPECT_EQ(hide_timer->GetCurrentDelay(),
            capture_mode::kDelayToHideKeyComboDuration);
  event_generator->ReleaseKey(ui::VKEY_CONTROL, ui::EF_NONE);
  EXPECT_TRUE(hide_timer->IsRunning());
  expected_modifier_key_vector = {ui::VKEY_CONTROL, ui::VKEY_SHIFT};
  EXPECT_EQ(capture_mode_demo_tools_test_api.GetShownModifiersKeyCodes(),
            expected_modifier_key_vector);
  EXPECT_EQ(capture_mode_demo_tools_test_api.GetShownNonModifierKeyCode(),
            ui::VKEY_B);

  event_generator->ReleaseKey(ui::VKEY_SHIFT, ui::EF_NONE);
  EXPECT_TRUE(hide_timer->IsRunning());

  // The contents of the widget remains the same before the timer expires.
  EXPECT_EQ(capture_mode_demo_tools_test_api.GetShownModifiersKeyCodes(),
            expected_modifier_key_vector);
  EXPECT_EQ(capture_mode_demo_tools_test_api.GetShownNonModifierKeyCode(),
            ui::VKEY_B);

  // The state the controller has been updated.
  EXPECT_EQ(capture_mode_demo_tools_test_api.GetCurrentModifiersFlags(), 0);
  EXPECT_EQ(capture_mode_demo_tools_test_api.GetLastNonModifierKey(),
            ui::VKEY_UNKNOWN);

  fire_hide_timer_and_verify_widget();
}

class CaptureModeDemoToolsTestWithAllSources
    : public CaptureModeDemoToolsTest,
      public testing::WithParamInterface<CaptureModeSource> {
 public:
  CaptureModeDemoToolsTestWithAllSources() = default;
  CaptureModeDemoToolsTestWithAllSources(
      const CaptureModeDemoToolsTestWithAllSources&) = delete;
  CaptureModeDemoToolsTestWithAllSources& operator=(
      const CaptureModeDemoToolsTestWithAllSources&) = delete;
  ~CaptureModeDemoToolsTestWithAllSources() override = default;

  CaptureModeController* StartDemoToolsEnabledVideoRecordingWithParam() {
    auto* controller = CaptureModeController::Get();
    const gfx::Rect capture_region(100, 200, 300, 400);
    controller->SetUserCaptureRegion(capture_region, /*by_user=*/true);

    StartCaptureSession(GetParam(), CaptureModeType::kVideo);
    controller->EnableDemoTools(true);

    if (GetParam() == CaptureModeSource::kWindow)
      GetEventGenerator()->MoveMouseToCenterOf(window());

    StartVideoRecordingImmediately();
    EXPECT_TRUE(controller->is_recording_in_progress());
    return controller;
  }
};

// Tests that the key combo viewer widget should be centered within its confined
// bounds.
TEST_P(CaptureModeDemoToolsTestWithAllSources,
       KeyComboViewerShouldBeCenteredTest) {
  auto* controller = StartDemoToolsEnabledVideoRecordingWithParam();
  auto* demo_tools_controller = GetCaptureModeDemoToolsController();
  EXPECT_TRUE(demo_tools_controller);

  auto* recording_watcher =
      CaptureModeController::Get()->video_recording_watcher_for_testing();
  gfx::Rect confined_bounds_in_screen =
      recording_watcher->GetCaptureSurfaceConfineBounds();

  // Converts the bounds if it is in the window's coordinate to screen
  // coordinate.
  if (GetParam() == CaptureModeSource::kWindow) {
    auto window_bounds = window()->GetBoundsInScreen();
    confined_bounds_in_screen.Offset(window_bounds.x(), window_bounds.y());
  }

  // Verifies that the `demo_tools_widget` is positioned in the middle
  // horizontally within the confined bounds.
  auto verify_demo_tools_been_centered = [&]() {
    CaptureModeDemoToolsTestApi capture_mode_demo_tools_test_api(
        demo_tools_controller);
    auto* demo_tools_widget =
        capture_mode_demo_tools_test_api.GetDemoToolsWidget();
    ASSERT_TRUE(demo_tools_widget);
    const gfx::Rect demo_tools_widget_bounds =
        demo_tools_widget->GetWindowBoundsInScreen();
    EXPECT_TRUE(abs(confined_bounds_in_screen.CenterPoint().x() -
                    demo_tools_widget_bounds.CenterPoint().x()) <= 1);
  };

  auto* event_generator = GetEventGenerator();
  event_generator->PressKey(ui::VKEY_CONTROL, ui::EF_NONE);
  verify_demo_tools_been_centered();

  event_generator->PressKey(ui::VKEY_SHIFT, ui::EF_NONE);
  verify_demo_tools_been_centered();

  event_generator->PressKey(ui::VKEY_A, ui::EF_NONE);
  verify_demo_tools_been_centered();

  controller->EndVideoRecording(EndRecordingReason::kStopRecordingButton);
  WaitForCaptureFileToBeSaved();
  EXPECT_FALSE(controller->IsActive());
}

INSTANTIATE_TEST_SUITE_P(All,
                         CaptureModeDemoToolsTestWithAllSources,
                         testing::Values(CaptureModeSource::kFullscreen,
                                         CaptureModeSource::kRegion,
                                         CaptureModeSource::kWindow));

}  // namespace ash
