// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <vector>

#include "ash/accelerators/keyboard_code_util.h"
#include "ash/accessibility/magnifier/docked_magnifier_controller.h"
#include "ash/capture_mode/capture_mode_bar_view.h"
#include "ash/capture_mode/capture_mode_constants.h"
#include "ash/capture_mode/capture_mode_controller.h"
#include "ash/capture_mode/capture_mode_demo_tools_controller.h"
#include "ash/capture_mode/capture_mode_demo_tools_test_api.h"
#include "ash/capture_mode/capture_mode_menu_toggle_button.h"
#include "ash/capture_mode/capture_mode_metrics.h"
#include "ash/capture_mode/capture_mode_session.h"
#include "ash/capture_mode/capture_mode_session_test_api.h"
#include "ash/capture_mode/capture_mode_settings_test_api.h"
#include "ash/capture_mode/capture_mode_test_util.h"
#include "ash/capture_mode/capture_mode_types.h"
#include "ash/capture_mode/capture_mode_util.h"
#include "ash/capture_mode/key_combo_view.h"
#include "ash/capture_mode/pointer_highlight_layer.h"
#include "ash/capture_mode/video_recording_watcher.h"
#include "ash/constants/ash_features.h"
#include "ash/display/window_tree_host_manager.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/shell.h"
#include "ash/style/icon_button.h"
#include "ash/test/ash_test_base.h"
#include "ash/wm/splitview/split_view_controller.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/timer/timer.h"
#include "ui/aura/window_tree_host.h"
#include "ui/base/ime/fake_text_input_client.h"
#include "ui/base/ime/text_input_type.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"
#include "ui/events/event_constants.h"
#include "ui/events/keycodes/keyboard_codes_posix.h"
#include "ui/events/pointer_details.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/vector2d.h"
#include "ui/views/controls/button/toggle_button.h"
#include "ui/views/controls/image_view.h"
#include "ui/wm/core/coordinate_conversion.h"

namespace ash {

namespace {

constexpr ui::KeyboardCode kIconKeyCodes[] = {ui::VKEY_BROWSER_BACK,
                                              ui::VKEY_BROWSER_FORWARD,
                                              ui::VKEY_BROWSER_REFRESH,
                                              ui::VKEY_ZOOM,
                                              ui::VKEY_MEDIA_LAUNCH_APP1,
                                              ui::VKEY_BRIGHTNESS_DOWN,
                                              ui::VKEY_BRIGHTNESS_UP,
                                              ui::VKEY_VOLUME_MUTE,
                                              ui::VKEY_VOLUME_DOWN,
                                              ui::VKEY_VOLUME_UP,
                                              ui::VKEY_UP,
                                              ui::VKEY_DOWN,
                                              ui::VKEY_LEFT,
                                              ui::VKEY_RIGHT};

}  // namespace

class CaptureModeDemoToolsTest : public AshTestBase {
 public:
  CaptureModeDemoToolsTest() {
    scoped_feature_list_.InitAndEnableFeature(features::kCaptureModeDemoTools);
  }
  CaptureModeDemoToolsTest(const CaptureModeDemoToolsTest&) = delete;
  CaptureModeDemoToolsTest& operator=(const CaptureModeDemoToolsTest&) = delete;
  ~CaptureModeDemoToolsTest() override = default;

  // AshTestBase:
  void SetUp() override {
    AshTestBase::SetUp();
    window_ = CreateTestWindow(gfx::Rect(20, 30, 601, 300));

    // Focus on non-input-text field at beginning.
    fake_text_input_client_ =
        std::make_unique<ui::FakeTextInputClient>(ui::TEXT_INPUT_TYPE_NONE);
  }

  void TearDown() override {
    window_.reset();
    AshTestBase::TearDown();
  }

  aura::Window* window() const { return window_.get(); }

  gfx::Rect GetConfinedBoundsInScreenCoordinates() {
    auto* recording_watcher =
        CaptureModeController::Get()->video_recording_watcher_for_testing();
    gfx::Rect confined_bounds_in_screen =
        recording_watcher->GetCaptureSurfaceConfineBounds();
    wm::ConvertRectToScreen(recording_watcher->window_being_recorded(),
                            &confined_bounds_in_screen);
    return confined_bounds_in_screen;
  }

  // Verifies that the `key_combo_widget` is positioned in the middle
  // horizontally within the confined bounds and that the distance between the
  // bottom of the widget and the bottom of the confined bounds is always equal
  // to `capture_mode::kKeyWidgetDistanceFromBottom`.
  void VerifyKeyComboWidgetPosition() {
    CaptureModeDemoToolsTestApi demo_tools_test_api(
        GetCaptureModeDemoToolsController());
    auto* key_combo_widget = demo_tools_test_api.GetKeyComboWidget();
    ASSERT_TRUE(key_combo_widget);
    auto confined_bounds_in_screen = GetConfinedBoundsInScreenCoordinates();
    const gfx::Rect key_combo_widget_bounds =
        key_combo_widget->GetWindowBoundsInScreen();
    EXPECT_NEAR(confined_bounds_in_screen.CenterPoint().x(),
                key_combo_widget_bounds.CenterPoint().x(), /*abs_error=*/1);
    EXPECT_EQ(
        confined_bounds_in_screen.bottom() - key_combo_widget_bounds.bottom(),
        capture_mode::kKeyWidgetDistanceFromBottom);
  }

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

  void WaitForMouseHighlightAnimationCompleted() {
    base::RunLoop run_loop;
    CaptureModeDemoToolsController* demo_tools_controller =
        GetCaptureModeDemoToolsController();
    DCHECK(demo_tools_controller);
    CaptureModeDemoToolsTestApi capture_mode_demo_tools_test_api(
        demo_tools_controller);
    capture_mode_demo_tools_test_api.SetOnMouseHighlightAnimationEndedCallback(
        run_loop.QuitClosure());
    run_loop.Run();
  }

  // Fires the key combo viewer timer and verifies the existence of the widget
  // after the timer expires.
  void FireTimerAndVerifyWidget(bool should_hide_view) {
    auto* demo_tools_controller = GetCaptureModeDemoToolsController();
    DCHECK(demo_tools_controller);
    CaptureModeDemoToolsTestApi capture_mode_demo_tools_test_api(
        demo_tools_controller);
    auto* timer = capture_mode_demo_tools_test_api.GetRefreshKeyComboTimer();
    EXPECT_TRUE(timer->IsRunning());
    EXPECT_EQ(timer->GetCurrentDelay(),
              should_hide_view
                  ? capture_mode::kRefreshKeyComboWidgetLongDelay
                  : capture_mode::kRefreshKeyComboWidgetShortDelay);
    KeyComboView* key_combo_view =
        capture_mode_demo_tools_test_api.GetKeyComboView();
    ViewVisibilityChangeWaiter waiter(key_combo_view);
    timer->FireNow();

    if (should_hide_view) {
      waiter.Wait();
      EXPECT_FALSE(capture_mode_demo_tools_test_api.GetKeyComboWidget());
      EXPECT_FALSE(capture_mode_demo_tools_test_api.GetKeyComboView());
    }
  }

  void EnableTextInputFocus(ui::TextInputType input_type) {
    fake_text_input_client_->set_text_input_type(input_type);
    Shell::Get()
        ->window_tree_host_manager()
        ->input_method()
        ->SetFocusedTextInputClient(fake_text_input_client_.get());
  }

  void DisableTextInputFocus() {
    fake_text_input_client_->set_text_input_type(ui::TEXT_INPUT_TYPE_NONE);
    Shell::Get()
        ->window_tree_host_manager()
        ->input_method()
        ->SetFocusedTextInputClient(nullptr);
  }

  void DragTouchAndVerifyHighlight(const ui::PointerId& touch_id,
                                   const gfx::Point& touch_point,
                                   const gfx::Vector2d& drag_offset) {
    auto* event_generator = GetEventGenerator();
    event_generator->PressTouchId(touch_id, touch_point);
    CaptureModeDemoToolsTestApi demo_tools_test_api(
        GetCaptureModeDemoToolsController());
    const auto& touch_highlight_map =
        demo_tools_test_api.GetTouchIdToHighlightLayerMap();
    const auto iter =
        touch_highlight_map.find(static_cast<ui::PointerId>(touch_id));
    ASSERT_TRUE(iter != touch_highlight_map.end());
    const auto* touch_highlight = iter->second.get();
    auto original_touch_highlight_bounds = touch_highlight->layer()->bounds();
    auto* recording_watcher =
        CaptureModeController::Get()->video_recording_watcher_for_testing();
    wm::ConvertRectToScreen(recording_watcher->window_being_recorded(),
                            &original_touch_highlight_bounds);
    event_generator->MoveTouchBy(drag_offset.x(), drag_offset.y());
    gfx::PointF updated_event_location{
        event_generator->current_screen_location()};
    const auto expected_touch_highlight_layer_bounds =
        capture_mode_util::CalculateHighlightLayerBounds(
            updated_event_location, capture_mode::kHighlightLayerRadius);
    auto actual_touch_highlight_layer_bounds = original_touch_highlight_bounds;
    actual_touch_highlight_layer_bounds.Offset(drag_offset.x(),
                                               drag_offset.y());
    EXPECT_EQ(expected_touch_highlight_layer_bounds,
              actual_touch_highlight_layer_bounds);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<aura::Window> window_;
  std::unique_ptr<ui::FakeTextInputClient> fake_text_input_client_;
};

// Tests that the key event is considered to generate the `key_combo_widget_`
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
  CaptureModeDemoToolsTestApi demo_tools_test_api(demo_tools_controller);
  EXPECT_FALSE(demo_tools_test_api.GetKeyComboWidget());
  event_generator->ReleaseKey(ui::VKEY_A, ui::EF_NONE);
  EXPECT_EQ(demo_tools_test_api.GetCurrentModifiersFlags(), 0);
  EXPECT_EQ(demo_tools_test_api.GetLastNonModifierKey(), ui::VKEY_UNKNOWN);

  // Press 'Ctrl' + 'A' and the key event will be considered to generate a
  // corresponding key widget.
  event_generator->PressKey(ui::VKEY_A, ui::EF_NONE);
  event_generator->PressKey(ui::VKEY_CONTROL, ui::EF_NONE);
  EXPECT_TRUE(demo_tools_test_api.GetKeyComboWidget());
  EXPECT_EQ(demo_tools_test_api.GetCurrentModifiersFlags(),
            ui::EF_CONTROL_DOWN);
  EXPECT_EQ(demo_tools_test_api.GetLastNonModifierKey(), ui::VKEY_A);

  event_generator->ReleaseKey(ui::VKEY_CONTROL, ui::EF_NONE);
  base::OneShotTimer* timer = demo_tools_test_api.GetRefreshKeyComboTimer();
  EXPECT_TRUE(timer->IsRunning());
  timer->FireNow();
  EXPECT_FALSE(demo_tools_test_api.GetKeyComboWidget());
  EXPECT_EQ(demo_tools_test_api.GetCurrentModifiersFlags(), 0);
  event_generator->ReleaseKey(ui::VKEY_A, ui::EF_NONE);

  event_generator->PressKey(ui::VKEY_TAB, ui::EF_NONE);
  EXPECT_TRUE(demo_tools_test_api.GetKeyComboWidget());
  EXPECT_EQ(demo_tools_test_api.GetCurrentModifiersFlags(), 0);
  EXPECT_EQ(demo_tools_test_api.GetLastNonModifierKey(), ui::VKEY_TAB);
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
  CaptureModeDemoToolsTestApi demo_tools_test_api(demo_tools_controller);
  EXPECT_TRUE(demo_tools_test_api.GetKeyComboWidget());
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
  CaptureModeDemoToolsController* demo_tools_controller =
      GetCaptureModeDemoToolsController();
  EXPECT_TRUE(demo_tools_controller);
  CaptureModeDemoToolsTestApi demo_tools_test_api(demo_tools_controller);

  auto* event_generator = GetEventGenerator();
  event_generator->PressKey(ui::VKEY_CONTROL, ui::EF_NONE);
  event_generator->PressKey(ui::VKEY_C, ui::EF_NONE);
  EXPECT_TRUE(demo_tools_test_api.GetKeyComboWidget());
  EXPECT_TRUE(demo_tools_test_api.GetKeyComboView());
  std::vector<ui::KeyboardCode> expected_modifier_key_vector = {
      ui::VKEY_CONTROL};
  EXPECT_EQ(demo_tools_test_api.GetShownModifiersKeyCodes(),
            expected_modifier_key_vector);
  EXPECT_EQ(demo_tools_test_api.GetShownNonModifierKeyCode(), ui::VKEY_C);

  // Press the key 'Shift' at last, but it will still show before the 'C' key.
  event_generator->PressKey(ui::VKEY_SHIFT, ui::EF_NONE);
  expected_modifier_key_vector = {ui::VKEY_CONTROL, ui::VKEY_SHIFT};
  EXPECT_TRUE(demo_tools_test_api.GetShownModifiersKeyCodes() ==
              expected_modifier_key_vector);
  EXPECT_EQ(demo_tools_test_api.GetShownNonModifierKeyCode(), ui::VKEY_C);

  // Release the modifier keys, and the key combo view will hide after the
  // refresh timer expires.
  event_generator->ReleaseKey(ui::VKEY_SHIFT, ui::EF_NONE);
  event_generator->ReleaseKey(ui::VKEY_CONTROL, ui::EF_NONE);
  FireTimerAndVerifyWidget(/*should_hide_view=*/true);
  EXPECT_FALSE(demo_tools_test_api.GetKeyComboWidget());
}

// Tests the timer behaviors for the key combo view:
// 1. The refresh timer will be triggered on key up of the non-modifier key with
// no modifier keys pressed, the key combo view will hide after the timer
// expires;
// 2. The refresh timer will also be triggered on key up of the last modifier
// key with no non-modifier key that can be displayed independently pressed. The
// key combo view will hide after the timer expires;
// 3. If there is another key down event happens before the timer expires, the
// refresh timer stops and the key combo view will be updated to match the
// current keys pressed;
// 4. On key up while the refresh timer is still running, the key combo view
// will stay visible even the key states have been updated until the timer
// expires.
TEST_F(CaptureModeDemoToolsTest, DemoToolsTimerTest) {
  CaptureModeController* controller = StartCaptureSession(
      CaptureModeSource::kFullscreen, CaptureModeType::kVideo);
  controller->EnableDemoTools(true);
  StartVideoRecordingImmediately();
  EXPECT_TRUE(controller->is_recording_in_progress());
  CaptureModeDemoToolsController* demo_tools_controller =
      GetCaptureModeDemoToolsController();
  EXPECT_TRUE(demo_tools_controller);
  CaptureModeDemoToolsTestApi demo_tools_test_api(demo_tools_controller);

  // Press the 'Ctrl' + 'A' and verify the shown key widgets.
  auto* event_generator = GetEventGenerator();
  event_generator->PressKey(ui::VKEY_CONTROL, ui::EF_NONE);
  event_generator->PressKey(ui::VKEY_A, ui::EF_NONE);
  EXPECT_TRUE(demo_tools_test_api.GetKeyComboWidget());
  KeyComboView* key_combo_view = demo_tools_test_api.GetKeyComboView();
  EXPECT_TRUE(key_combo_view);
  std::vector<ui::KeyboardCode> expected_modifier_key_vector = {
      ui::VKEY_CONTROL};
  EXPECT_EQ(demo_tools_test_api.GetShownModifiersKeyCodes(),
            expected_modifier_key_vector);
  EXPECT_EQ(demo_tools_test_api.GetShownNonModifierKeyCode(), ui::VKEY_A);

  // Release the non-modifier key and the timer with a delay of
  // `capture_mode::kRefreshKeyComboWidgetShortDelay` will be triggered, the key
  // combo view will be updated to show 'Ctrl'.
  event_generator->ReleaseKey(ui::VKEY_A, ui::EF_NONE);
  FireTimerAndVerifyWidget(/*should_hide_view=*/false);
  EXPECT_EQ(demo_tools_test_api.GetShownModifiersKeyCodes(),
            expected_modifier_key_vector);
  EXPECT_EQ(demo_tools_test_api.GetShownNonModifierKeyCode(), ui::VKEY_UNKNOWN);

  // Release the non-modifier key with no modifier keys pressed and the hide
  // timer will be triggered.
  event_generator->PressKey(ui::VKEY_A, ui::EF_NONE);
  event_generator->ReleaseKey(ui::VKEY_CONTROL, ui::EF_NONE);
  event_generator->ReleaseKey(ui::VKEY_A, ui::EF_NONE);
  FireTimerAndVerifyWidget(/*should_hide_view=*/true);

  // Press 'Ctrl' + 'A' and release the only modifier key 'Ctrl' and
  // the refresh timer will be triggered. The entire key combo viewer will hide
  // after the refresh timer expires.
  event_generator->PressKey(ui::VKEY_CONTROL, ui::EF_NONE);
  event_generator->PressKey(ui::VKEY_A, ui::EF_NONE);
  event_generator->ReleaseKey(ui::VKEY_CONTROL, ui::EF_NONE);
  FireTimerAndVerifyWidget(/*should_hide_view=*/true);
  event_generator->ReleaseKey(ui::VKEY_A, ui::EF_NONE);

  // Press 'Ctrl' + 'Shift' + 'A', then release 'A', the timer with a delay of
  // `capture_mode::kRefreshKeyComboWidgetShortDelay` will be triggered. Press
  // 'B' and the key combo view will be updated accordingly, i.e. 'Ctrl' +
  // 'Shift' + 'B'.
  event_generator->PressKey(ui::VKEY_CONTROL, ui::EF_NONE);
  event_generator->PressKey(ui::VKEY_SHIFT, ui::EF_NONE);
  event_generator->PressKey(ui::VKEY_A, ui::EF_NONE);
  EXPECT_TRUE(demo_tools_test_api.GetKeyComboWidget());
  expected_modifier_key_vector = {ui::VKEY_CONTROL, ui::VKEY_SHIFT};
  EXPECT_EQ(demo_tools_test_api.GetShownModifiersKeyCodes(),
            expected_modifier_key_vector);
  EXPECT_EQ(demo_tools_test_api.GetShownNonModifierKeyCode(), ui::VKEY_A);
  event_generator->ReleaseKey(ui::VKEY_A, ui::EF_NONE);
  base::OneShotTimer* timer = demo_tools_test_api.GetRefreshKeyComboTimer();
  EXPECT_TRUE(timer->IsRunning());
  EXPECT_EQ(timer->GetCurrentDelay(),
            capture_mode::kRefreshKeyComboWidgetShortDelay);
  event_generator->PressKey(ui::VKEY_B, ui::EF_NONE);
  EXPECT_FALSE(timer->IsRunning());
  EXPECT_EQ(timer->GetCurrentDelay(),
            capture_mode::kRefreshKeyComboWidgetShortDelay);
  EXPECT_EQ(demo_tools_test_api.GetShownNonModifierKeyCode(), ui::VKEY_B);

  // Release the 'Ctrl' key, the timer with a delay of
  // `capture_mode::kRefreshKeyComboWidgetShortDelay` will be triggered. Then
  // release the 'Shift' key and the refresh timer will be triggered The entire
  // key combo view will hide after the timer expires.
  event_generator->ReleaseKey(ui::VKEY_CONTROL, ui::EF_NONE);
  FireTimerAndVerifyWidget(/*should_hide_view=*/false);
  expected_modifier_key_vector = {ui::VKEY_SHIFT};
  EXPECT_EQ(demo_tools_test_api.GetShownModifiersKeyCodes(),
            expected_modifier_key_vector);
  EXPECT_EQ(demo_tools_test_api.GetShownNonModifierKeyCode(), ui::VKEY_B);

  event_generator->ReleaseKey(ui::VKEY_SHIFT, ui::EF_NONE);
  EXPECT_TRUE(timer->IsRunning());
  EXPECT_EQ(timer->GetCurrentDelay(),
            capture_mode::kRefreshKeyComboWidgetLongDelay);
  event_generator->ReleaseKey(ui::VKEY_B, ui::EF_NONE);

  // The contents of the widget remains the same before the timer expires.
  EXPECT_EQ(demo_tools_test_api.GetShownModifiersKeyCodes(),
            expected_modifier_key_vector);
  EXPECT_EQ(demo_tools_test_api.GetShownNonModifierKeyCode(), ui::VKEY_B);

  // The state the controller has been updated.
  EXPECT_EQ(demo_tools_test_api.GetCurrentModifiersFlags(), 0);
  EXPECT_EQ(demo_tools_test_api.GetLastNonModifierKey(), ui::VKEY_UNKNOWN);

  FireTimerAndVerifyWidget(/*should_hide_view=*/true);
}

// Tests that all the non-modifier keys with the icon are displayed
// independently and correctly.
TEST_F(CaptureModeDemoToolsTest, AllIconKeysTest) {
  CaptureModeController* controller = StartCaptureSession(
      CaptureModeSource::kFullscreen, CaptureModeType::kVideo);
  controller->EnableDemoTools(true);
  StartVideoRecordingImmediately();
  EXPECT_TRUE(controller->is_recording_in_progress());
  CaptureModeDemoToolsController* demo_tools_controller =
      GetCaptureModeDemoToolsController();
  CaptureModeDemoToolsTestApi demo_tools_test_api(demo_tools_controller);
  auto* event_generator = GetEventGenerator();

  for (const auto key_code : kIconKeyCodes) {
    event_generator->PressKey(key_code, ui::EF_NONE);
    EXPECT_EQ(demo_tools_test_api.GetShownNonModifierKeyCode(), key_code);
    views::ImageView* icon = demo_tools_test_api.GetNonModifierKeyItemIcon();
    ASSERT_TRUE(icon);
    const auto image_model = icon->GetImageModel();
    const gfx::VectorIcon* vector_icon = GetVectorIconForKeyboardCode(key_code);
    EXPECT_EQ(std::string(vector_icon->name),
              std::string(image_model.GetVectorIcon().vector_icon()->name));
    event_generator->ReleaseKey(key_code, ui::EF_NONE);
  }
}

// Tests that the key combo viewer widget will not show if the input
// field is currently focused and will display in a normal way when the focus is
// detached.
TEST_F(CaptureModeDemoToolsTest, DoNotShowKeyComboViewerInInputField) {
  for (const auto input_type :
       {ui::TEXT_INPUT_TYPE_TEXT, ui::TEXT_INPUT_TYPE_PASSWORD,
        ui::TEXT_INPUT_TYPE_SEARCH, ui::TEXT_INPUT_TYPE_EMAIL,
        ui::TEXT_INPUT_TYPE_NUMBER, ui::TEXT_INPUT_TYPE_TELEPHONE,
        ui::TEXT_INPUT_TYPE_URL, ui::TEXT_INPUT_TYPE_DATE,
        ui::TEXT_INPUT_TYPE_DATE_TIME, ui::TEXT_INPUT_TYPE_DATE_TIME_LOCAL,
        ui::TEXT_INPUT_TYPE_MONTH, ui::TEXT_INPUT_TYPE_TIME,
        ui::TEXT_INPUT_TYPE_WEEK, ui::TEXT_INPUT_TYPE_TEXT_AREA,
        ui::TEXT_INPUT_TYPE_CONTENT_EDITABLE,
        ui::TEXT_INPUT_TYPE_DATE_TIME_FIELD, ui::TEXT_INPUT_TYPE_NULL}) {
    EnableTextInputFocus(input_type);
    CaptureModeController* controller = StartCaptureSession(
        CaptureModeSource::kFullscreen, CaptureModeType::kVideo);
    controller->EnableDemoTools(true);
    StartVideoRecordingImmediately();
    EXPECT_TRUE(controller->is_recording_in_progress());
    CaptureModeDemoToolsController* demo_tools_controller =
        GetCaptureModeDemoToolsController();
    CaptureModeDemoToolsTestApi demo_tools_test_api(demo_tools_controller);
    auto* event_generator = GetEventGenerator();

    // With the input text focus enabled before the video recording, the
    // key combo viewer will not display when pressing 'Ctrl' and 'T'.
    event_generator->PressKey(ui::VKEY_CONTROL, ui::EF_NONE);
    event_generator->PressKey(ui::VKEY_T, ui::EF_NONE);
    EXPECT_FALSE(demo_tools_test_api.GetKeyComboWidget());
    EXPECT_FALSE(demo_tools_test_api.GetKeyComboView());
    event_generator->ReleaseKey(ui::VKEY_T, ui::EF_NONE);
    event_generator->ReleaseKey(ui::VKEY_CONTROL, ui::EF_NONE);

    // Disable the input text focus, the key combo viewer will show when
    // pressing 'Ctrl' and 'T' in a non-input-text field.
    DisableTextInputFocus();
    event_generator->PressKey(ui::VKEY_CONTROL, ui::EF_NONE);
    event_generator->PressKey(ui::VKEY_T, ui::EF_NONE);
    EXPECT_TRUE(demo_tools_test_api.GetKeyComboWidget());
    EXPECT_TRUE(demo_tools_test_api.GetKeyComboView());
    event_generator->ReleaseKey(ui::VKEY_T, ui::EF_NONE);
    event_generator->ReleaseKey(ui::VKEY_CONTROL, ui::EF_NONE);
    FireTimerAndVerifyWidget(/*should_hide_view=*/true);

    // Enable the text input focus during the recording, the key combo
    // viewer will not display when pressing 'Ctrl' and 'T'.
    EnableTextInputFocus(input_type);
    event_generator->PressKey(ui::VKEY_CONTROL, ui::EF_NONE);
    event_generator->PressKey(ui::VKEY_T, ui::EF_NONE);
    EXPECT_FALSE(demo_tools_test_api.GetKeyComboWidget());
    EXPECT_FALSE(demo_tools_test_api.GetKeyComboView());
    event_generator->ReleaseKey(ui::VKEY_T, ui::EF_NONE);
    event_generator->ReleaseKey(ui::VKEY_CONTROL, ui::EF_NONE);

    controller->EndVideoRecording(EndRecordingReason::kStopRecordingButton);
    WaitForCaptureFileToBeSaved();
  }
}

// verifies that after any key release, if the remaining pressed keys are no
// longer displayable, the widget will be scheduled to hide after
// `capture_mode::kRefreshKeyComboWidgetLongDelay`.
TEST_F(CaptureModeDemoToolsTest, ReleaseAllKeysConsistencyTest) {
  CaptureModeController* controller = StartCaptureSession(
      CaptureModeSource::kFullscreen, CaptureModeType::kVideo);
  controller->EnableDemoTools(true);
  StartVideoRecordingImmediately();
  EXPECT_TRUE(controller->is_recording_in_progress());
  CaptureModeDemoToolsController* demo_tools_controller =
      GetCaptureModeDemoToolsController();
  EXPECT_TRUE(demo_tools_controller);
  CaptureModeDemoToolsTestApi demo_tools_test_api(demo_tools_controller);

  auto* event_generator = GetEventGenerator();
  auto key_combo_generator = [&]() {
    event_generator->PressKey(ui::VKEY_CONTROL, ui::EF_NONE);
    event_generator->PressKey(ui::VKEY_SHIFT, ui::EF_NONE);
    event_generator->PressKey(ui::VKEY_C, ui::EF_NONE);
  };

  key_combo_generator();
  KeyComboView* key_combo_view = demo_tools_test_api.GetKeyComboView();
  EXPECT_TRUE(key_combo_view);

  // Release the modifier key 'Ctrl' to trigger the timer with a delay
  // of `capture_mode::kRefreshKeyComboWidgetShortDelay`.
  event_generator->ReleaseKey(ui::VKEY_CONTROL, ui::EF_NONE);

  base::OneShotTimer* timer = demo_tools_test_api.GetRefreshKeyComboTimer();
  EXPECT_TRUE(timer->IsRunning());
  EXPECT_EQ(timer->GetCurrentDelay(),
            capture_mode::kRefreshKeyComboWidgetShortDelay);

  std::vector<ui::KeyboardCode> expected_modifier_key_vector = {
      ui::VKEY_CONTROL, ui::VKEY_SHIFT};
  EXPECT_EQ(demo_tools_test_api.GetShownModifiersKeyCodes(),
            expected_modifier_key_vector);
  EXPECT_EQ(demo_tools_test_api.GetShownNonModifierKeyCode(), ui::VKEY_C);

  // Release the modifier key 'Shift' and the refresh timer will be triggered.
  event_generator->ReleaseKey(ui::VKEY_SHIFT, ui::EF_NONE);
  EXPECT_TRUE(timer->IsRunning());
  EXPECT_EQ(timer->GetCurrentDelay(),
            capture_mode::kRefreshKeyComboWidgetLongDelay);
  EXPECT_EQ(demo_tools_test_api.GetShownModifiersKeyCodes(),
            expected_modifier_key_vector);
  EXPECT_EQ(demo_tools_test_api.GetShownNonModifierKeyCode(), ui::VKEY_C);

  FireTimerAndVerifyWidget(/*should_hide_view=*/true);

  // Key combo viewer update test.
  key_combo_generator();
  event_generator->ReleaseKey(ui::VKEY_CONTROL, ui::EF_NONE);
  EXPECT_TRUE(timer->IsRunning());
  EXPECT_EQ(timer->GetCurrentDelay(),
            capture_mode::kRefreshKeyComboWidgetShortDelay);
  timer->FireNow();
  expected_modifier_key_vector = {ui::VKEY_SHIFT};
  EXPECT_EQ(demo_tools_test_api.GetShownModifiersKeyCodes(),
            expected_modifier_key_vector);
  EXPECT_EQ(demo_tools_test_api.GetShownNonModifierKeyCode(), ui::VKEY_C);
}

// Tests that when the key combo is 'modifier key' + 'non-modifier key that can
// be shown independently', on key up of either key, the key combo viewer should
// be updated to show the other key. When both keys are released, the refresh
// timer will be triggered.
TEST_F(CaptureModeDemoToolsTest,
       ModifierAndIndependentlyShownNonModifierKeyComboTest) {
  CaptureModeController* controller = StartCaptureSession(
      CaptureModeSource::kFullscreen, CaptureModeType::kVideo);
  controller->EnableDemoTools(true);
  StartVideoRecordingImmediately();
  EXPECT_TRUE(controller->is_recording_in_progress());
  CaptureModeDemoToolsController* demo_tools_controller =
      GetCaptureModeDemoToolsController();
  CaptureModeDemoToolsTestApi demo_tools_test_api(demo_tools_controller);
  auto* event_generator = GetEventGenerator();
  event_generator->PressKey(ui::VKEY_CONTROL, ui::EF_NONE);
  event_generator->PressKey(kIconKeyCodes[0], ui::EF_NONE);
  EXPECT_EQ(demo_tools_test_api.GetShownModifiersKeyCodes(),
            std::vector<ui::KeyboardCode>{ui::VKEY_CONTROL});
  EXPECT_EQ(demo_tools_test_api.GetShownNonModifierKeyCode(), kIconKeyCodes[0]);

  event_generator->ReleaseKey(ui::VKEY_CONTROL, ui::EF_NONE);
  FireTimerAndVerifyWidget(/*should_hide_view=*/false);
  EXPECT_TRUE(demo_tools_test_api.GetShownModifiersKeyCodes().empty());
  EXPECT_EQ(demo_tools_test_api.GetShownNonModifierKeyCode(), kIconKeyCodes[0]);
  event_generator->PressKey(ui::VKEY_CONTROL, ui::EF_NONE);

  event_generator->ReleaseKey(kIconKeyCodes[0], ui::EF_NONE);
  FireTimerAndVerifyWidget(/*should_hide_view=*/false);
  EXPECT_EQ(demo_tools_test_api.GetShownModifiersKeyCodes(),
            std::vector<ui::KeyboardCode>{ui::VKEY_CONTROL});
  EXPECT_EQ(demo_tools_test_api.GetShownNonModifierKeyCode(), ui::VKEY_UNKNOWN);

  event_generator->ReleaseKey(ui::VKEY_CONTROL, ui::EF_NONE);
  FireTimerAndVerifyWidget(/*should_hide_view=*/true);
}

// Tests that if the width of the confined bounds is smaller than that of the
// preferred size of the key combo widget, the key combo widget will be shifted
// to the left. But the right edge of the key combo widget will always be to the
// left of the right edge of the capture surface confined bounds.
TEST_F(CaptureModeDemoToolsTest,
       ConfinedBoundsSizeSmallerThanPreferredSizeTest) {
  auto* controller = CaptureModeController::Get();
  const gfx::Rect capture_region(100, 200, 200, 50);
  controller->SetUserCaptureRegion(capture_region, /*by_user=*/true);
  StartCaptureSession(CaptureModeSource::kRegion, CaptureModeType::kVideo);
  controller->EnableDemoTools(true);
  StartVideoRecordingImmediately();

  auto* event_generator = GetEventGenerator();
  event_generator->PressKey(ui::VKEY_CONTROL, ui::EF_NONE);
  event_generator->PressKey(ui::VKEY_SHIFT, ui::EF_NONE);
  event_generator->PressKey(ui::VKEY_C, ui::EF_NONE);

  auto* demo_tools_controller = GetCaptureModeDemoToolsController();
  DCHECK(demo_tools_controller);
  CaptureModeDemoToolsTestApi demo_tools_test_api(demo_tools_controller);
  KeyComboView* key_combo_view = demo_tools_test_api.GetKeyComboView();
  EXPECT_TRUE(key_combo_view);
  const auto confine_bounds = controller->GetCaptureSurfaceConfineBounds();
  EXPECT_LT(confine_bounds.width(),
            key_combo_view->GetBoundsInScreen().width());
  EXPECT_GT(confine_bounds.right(),
            key_combo_view->GetBoundsInScreen().right());
}

// Tests that the key combo widget will be re-posisioned correctly on capture
// window bounds change.
TEST_F(CaptureModeDemoToolsTest, CaptureBoundsChangeTest) {
  UpdateDisplay("800x700");
  const auto window = CreateTestWindow(gfx::Rect(100, 150, 300, 500));
  auto* split_view_controller =
      SplitViewController::Get(Shell::GetPrimaryRootWindow());
  EXPECT_EQ(split_view_controller->state(),
            SplitViewController::State::kNoSnap);

  auto* capture_mode_controller =
      StartCaptureSession(CaptureModeSource::kWindow, CaptureModeType::kVideo);
  auto* event_generator = GetEventGenerator();
  event_generator->MoveMouseToCenterOf(window.get());

  capture_mode_controller->EnableDemoTools(true);
  StartVideoRecordingImmediately();
  EXPECT_TRUE(capture_mode_controller->is_recording_in_progress());
  auto* demo_tools_controller = GetCaptureModeDemoToolsController();
  DCHECK(demo_tools_controller);

  event_generator->PressKey(ui::VKEY_CONTROL, ui::EF_NONE);
  event_generator->PressKey(ui::VKEY_C, ui::EF_NONE);
  VerifyKeyComboWidgetPosition();

  // Snap the `window` which will result in window bounds change and the key
  // combo widget will still be centered horizontally.
  split_view_controller->SnapWindow(
      window.get(), SplitViewController::SnapPosition::kPrimary);
  EXPECT_EQ(split_view_controller->primary_window(), window.get());
  VerifyKeyComboWidgetPosition();
}

// Tests that there is no crash when work area changed after starting a video
// recording with demo tools enabled. Docked mananifier is used as an example to
// trigger the work area change.
TEST_F(CaptureModeDemoToolsTest, WorkAreaChangeTest) {
  CaptureModeController* controller = StartCaptureSession(
      CaptureModeSource::kFullscreen, CaptureModeType::kVideo);
  controller->EnableDemoTools(true);
  StartVideoRecordingImmediately();
  EXPECT_TRUE(controller->is_recording_in_progress());
  CaptureModeDemoToolsController* demo_tools_controller =
      GetCaptureModeDemoToolsController();
  EXPECT_TRUE(demo_tools_controller);
  CaptureModeDemoToolsTestApi demo_tools_test_api(demo_tools_controller);

  auto* docked_magnifier_controller =
      Shell::Get()->docked_magnifier_controller();
  docked_magnifier_controller->SetEnabled(/*enabled=*/true);
  controller->EndVideoRecording(EndRecordingReason::kStopRecordingButton);
}

// Tests that the metrics that record if a recording starts with demo tools
// feature enabled are recorded correctly in a capture session both in clamshell
// and tablet mode.
TEST_F(CaptureModeDemoToolsTest,
       DemoToolsEnabledOnRecordingStartHistogramTest) {
  base::HistogramTester histogram_tester;
  constexpr char kHistogramNameBase[] =
      "Ash.CaptureModeController.DemoToolsEnabledOnRecordingStart";

  struct {
    bool enable_tablet_mode;
    bool enable_demo_tools;
  } kTestCases[]{
      {/*enable_tablet_mode=*/false, /*enable_demo_tools=*/false},
      {/*enable_tablet_mode=*/false, /*enable_demo_tools=*/true},
      {/*enable_tablet_mode=*/true, /*enable_demo_tools=*/false},
      {/*enable_tablet_mode=*/true, /*enable_demo_tools=*/true},
  };

  for (const auto test_case : kTestCases) {
    if (test_case.enable_tablet_mode) {
      SwitchToTabletMode();
      EXPECT_TRUE(Shell::Get()->IsInTabletMode());
    } else {
      EXPECT_FALSE(Shell::Get()->IsInTabletMode());
    }

    const auto histogram_name = GetCaptureModeHistogramName(kHistogramNameBase);
    histogram_tester.ExpectBucketCount(histogram_name,
                                       test_case.enable_demo_tools, 0);
    auto* controller = StartCaptureSession(CaptureModeSource::kFullscreen,
                                           CaptureModeType::kVideo);
    controller->EnableDemoTools(test_case.enable_demo_tools);
    StartVideoRecordingImmediately();
    EXPECT_TRUE(controller->is_recording_in_progress());
    controller->EndVideoRecording(EndRecordingReason::kStopRecordingButton);
    WaitForCaptureFileToBeSaved();
    histogram_tester.ExpectBucketCount(histogram_name,
                                       test_case.enable_demo_tools, 1);
  }
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

  auto* event_generator = GetEventGenerator();
  const auto kKeyCodes = {ui::VKEY_CONTROL, ui::VKEY_SHIFT, ui::VKEY_A};
  for (const auto key_code : kKeyCodes) {
    event_generator->PressKey(key_code, ui::EF_NONE);
    VerifyKeyComboWidgetPosition();
  }

  controller->EndVideoRecording(EndRecordingReason::kStopRecordingButton);
  WaitForCaptureFileToBeSaved();
  EXPECT_FALSE(controller->IsActive());
}

// Tests that the mouse highlight layer will be created on mouse down and
// will disappear after the animation.
TEST_P(CaptureModeDemoToolsTestWithAllSources, MouseHighlightTest) {
  ui::ScopedAnimationDurationScaleMode normal_animation(
      ui::ScopedAnimationDurationScaleMode::NORMAL_DURATION);
  StartDemoToolsEnabledVideoRecordingWithParam();
  auto* demo_tools_controller = GetCaptureModeDemoToolsController();
  EXPECT_TRUE(demo_tools_controller);
  CaptureModeDemoToolsTestApi demo_tools_test_api(demo_tools_controller);

  gfx::Rect confined_bounds_in_screen = GetConfinedBoundsInScreenCoordinates();
  auto* event_generator = GetEventGenerator();
  event_generator->MoveMouseTo(confined_bounds_in_screen.CenterPoint());
  event_generator->PressLeftButton();
  event_generator->ReleaseLeftButton();
  const MouseHighlightLayers& highlight_layers =
      demo_tools_test_api.GetMouseHighlightLayers();
  EXPECT_FALSE(highlight_layers.empty());
  EXPECT_EQ(highlight_layers.size(), 1u);
  WaitForMouseHighlightAnimationCompleted();
  EXPECT_TRUE(highlight_layers.empty());
}

// Tests that multiple mouse highlight layers will be visible on consecutive
// mouse press events when the whole duration are within the expiration of the
// first animation expiration. It also tests that each mouse highlight layer
// will be centered on its mouse event location.
TEST_P(CaptureModeDemoToolsTestWithAllSources,
       MouseHighlightShouldBeCenteredWithMouseClick) {
  ui::ScopedAnimationDurationScaleMode normal_animation(
      ui::ScopedAnimationDurationScaleMode::NORMAL_DURATION);
  StartDemoToolsEnabledVideoRecordingWithParam();
  auto* recording_watcher =
      CaptureModeController::Get()->video_recording_watcher_for_testing();
  auto* window_being_recorded = recording_watcher->window_being_recorded();
  auto* demo_tools_controller = GetCaptureModeDemoToolsController();
  EXPECT_TRUE(demo_tools_controller);
  CaptureModeDemoToolsTestApi demo_tools_test_api =
      CaptureModeDemoToolsTestApi(demo_tools_controller);

  gfx::Rect inner_rect = GetConfinedBoundsInScreenCoordinates();
  inner_rect.Inset(5);

  const auto& layers_vector = demo_tools_test_api.GetMouseHighlightLayers();
  auto* event_generator = GetEventGenerator();

  for (auto point : {inner_rect.CenterPoint(), inner_rect.origin(),
                     inner_rect.bottom_right()}) {
    event_generator->MoveMouseTo(point);
    event_generator->PressLeftButton();
    event_generator->ReleaseLeftButton();
    auto* highlight_layer = layers_vector.back().get();
    auto highlight_center_point =
        highlight_layer->layer()->bounds().CenterPoint();

    // Convert the highlight layer center pointer to screen coordinates.
    wm::ConvertPointToScreen(window_being_recorded, &highlight_center_point);

    EXPECT_EQ(highlight_center_point, point);
  }

  EXPECT_EQ(layers_vector.size(), 3u);
}

// Tests that the key combo viewer is positioned correctly on device
// scale factor change.
TEST_P(CaptureModeDemoToolsTestWithAllSources, DeviceScaleFactorTest) {
  StartDemoToolsEnabledVideoRecordingWithParam();
  auto* demo_tools_controller = GetCaptureModeDemoToolsController();
  EXPECT_TRUE(demo_tools_controller);

  auto* event_generator = GetEventGenerator();
  event_generator->PressKey(ui::VKEY_CONTROL, ui::EF_NONE);
  event_generator->PressKey(ui::VKEY_SHIFT, ui::EF_NONE);
  event_generator->PressKey(ui::VKEY_A, ui::EF_NONE);

  const float kDeviceScaleFactors[] = {0.5f, 1.2f, 2.5f};
  for (const float dsf : kDeviceScaleFactors) {
    SetDeviceScaleFactor(dsf);
    EXPECT_EQ(dsf, window()->GetHost()->device_scale_factor());
    VerifyKeyComboWidgetPosition();
  }
}

// Tests that the touch highlight layer will be created on touch
// down and removed on touch up. It also tests that the bounds of the touch
// highlight layer will be updated correctly on the touch drag event.
TEST_P(CaptureModeDemoToolsTestWithAllSources, TouchHighlightTest) {
  StartDemoToolsEnabledVideoRecordingWithParam();
  auto* demo_tools_controller = GetCaptureModeDemoToolsController();
  EXPECT_TRUE(demo_tools_controller);
  CaptureModeDemoToolsTestApi demo_tools_test_api(demo_tools_controller);

  const gfx::Rect confined_bounds_in_screen =
      GetConfinedBoundsInScreenCoordinates();
  auto* event_generator = GetEventGenerator();

  const auto& touch_highlight_map =
      demo_tools_test_api.GetTouchIdToHighlightLayerMap();

  const auto center_point = confined_bounds_in_screen.CenterPoint();
  event_generator->PressTouchId(0, center_point);
  EXPECT_FALSE(touch_highlight_map.empty());
  event_generator->ReleaseTouchId(0);
  EXPECT_TRUE(touch_highlight_map.empty());

  const gfx::Vector2d drag_offset =
      gfx::Vector2d(confined_bounds_in_screen.width() / 4,
                    confined_bounds_in_screen.height() / 4);
  DragTouchAndVerifyHighlight(/*touch_id=*/0, /*touch_point=*/center_point,
                              drag_offset);
}

// Tests the behaviors when multiple touches are performed.
// 1. The corresponding touch highlight will be generated on touch down;
// 2. The number of touch highlights kept in the demo tools controller is the
// same as the number of touch down events;
// 3. The bounds of the touch highlights will be updated correctly when dragging
// multiple touch events simultaneously;
// 4. The corresponding touch highlight will be removed on touch up. The
// number of touch highlights kept in the demo tools controller will become zero
// when all touches are released or cancelled.
TEST_P(CaptureModeDemoToolsTestWithAllSources, MutiTouchHighlightTest) {
  StartDemoToolsEnabledVideoRecordingWithParam();
  auto* demo_tools_controller = GetCaptureModeDemoToolsController();
  EXPECT_TRUE(demo_tools_controller);
  CaptureModeDemoToolsTestApi demo_tools_test_api(demo_tools_controller);

  const auto& touch_highlight_map =
      demo_tools_test_api.GetTouchIdToHighlightLayerMap();
  EXPECT_TRUE(touch_highlight_map.empty());

  gfx::Rect inner_rect = GetConfinedBoundsInScreenCoordinates();
  inner_rect.Inset(20);

  struct {
    int touch_id;
    gfx::Point touch_point;
    gfx::Vector2d drag_offset;
  } kTestCases[] = {
      {/*touch_id=*/1, inner_rect.CenterPoint(), gfx::Vector2d(15, 25)},
      {/*touch_id=*/0, inner_rect.origin(), gfx::Vector2d(10, -20)},
      {/*touch_id=*/2, inner_rect.bottom_right(), gfx::Vector2d(-30, -20)}};

  // Iterate through the kTestCases and perform the touch down. The
  // corresponding touch highlight will be generated. Drag these touch events
  // and check if the bounds of the corresponding touch highlight are updated
  // correctly.
  for (const auto& test_case : kTestCases) {
    DragTouchAndVerifyHighlight(test_case.touch_id, test_case.touch_point,
                                test_case.drag_offset);
  }

  EXPECT_EQ(touch_highlight_map.size(), 3u);

  // Release the touch event one by one and the corresponding touch highlight
  // layer will be removed. The number of highlight layers kept in the demo
  // tools controller will become zero when all touches are released or
  // cancelled.
  for (const auto& test_case : kTestCases) {
    GetEventGenerator()->ReleaseTouchId(test_case.touch_id);
    EXPECT_FALSE(touch_highlight_map.contains(
        static_cast<ui::PointerId>(test_case.touch_id)));
  }

  EXPECT_TRUE(touch_highlight_map.empty());
}

INSTANTIATE_TEST_SUITE_P(All,
                         CaptureModeDemoToolsTestWithAllSources,
                         testing::Values(CaptureModeSource::kFullscreen,
                                         CaptureModeSource::kRegion,
                                         CaptureModeSource::kWindow));

class ProjectorCaptureModeDemoToolsTest : public CaptureModeDemoToolsTest {
 public:
  ProjectorCaptureModeDemoToolsTest() = default;
  ~ProjectorCaptureModeDemoToolsTest() override = default;

  // CaptureModeDemoToolsTest:
  void SetUp() override {
    CaptureModeDemoToolsTest::SetUp();
    projector_helper_.SetUp();
  }

  void StartProjectorModeSession() {
    projector_helper_.StartProjectorModeSession();
  }

 private:
  ProjectorCaptureModeIntegrationHelper projector_helper_;
};

// Tests that the metrics that record if a recording starts with demo tools
// feature enabled are recorded correctly in a projector-initiated capture
// session both in clamshell and tablet mode.
TEST_F(ProjectorCaptureModeDemoToolsTest,
       ProjectorDemoToolsEnabledOnRecordingStartHistogramTest) {
  base::HistogramTester histogram_tester;
  constexpr char kHistogramNameBase[] =
      "Ash.CaptureModeController.Projector.DemoToolsEnabledOnRecordingStart";

  struct {
    bool enable_tablet_mode;
    bool enable_demo_tools;
  } kTestCases[]{
      {/*enable_tablet_mode=*/false, /*enable_demo_tools=*/false},
      {/*enable_tablet_mode=*/false, /*enable_demo_tools=*/true},
      {/*enable_tablet_mode=*/true, /*enable_demo_tools=*/false},
      {/*enable_tablet_mode=*/true, /*enable_demo_tools=*/true},
  };

  for (const auto test_case : kTestCases) {
    if (test_case.enable_tablet_mode) {
      SwitchToTabletMode();
      EXPECT_TRUE(Shell::Get()->IsInTabletMode());
    } else {
      EXPECT_FALSE(Shell::Get()->IsInTabletMode());
    }

    const auto histogram_name = GetCaptureModeHistogramName(kHistogramNameBase);
    histogram_tester.ExpectBucketCount(histogram_name,
                                       test_case.enable_demo_tools, 0);
    auto* controller = CaptureModeController::Get();
    controller->SetType(CaptureModeType::kVideo);
    controller->SetSource(CaptureModeSource::kFullscreen);

    StartProjectorModeSession();
    controller->EnableDemoTools(test_case.enable_demo_tools);
    EXPECT_TRUE(controller->IsActive());
    EXPECT_TRUE(controller->capture_mode_session()->is_in_projector_mode());

    StartVideoRecordingImmediately();
    EXPECT_TRUE(controller->is_recording_in_progress());
    WaitForSeconds(1);

    controller->EndVideoRecording(EndRecordingReason::kStopRecordingButton);
    WaitForCaptureFileToBeSaved();
    histogram_tester.ExpectBucketCount(histogram_name,
                                       test_case.enable_demo_tools, 1);
  }
}

}  // namespace ash
