// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/capture_mode/capture_mode_bar_view.h"
#include "ash/capture_mode/capture_mode_close_button.h"
#include "ash/capture_mode/capture_mode_controller.h"
#include "ash/capture_mode/capture_mode_session.h"
#include "ash/capture_mode/capture_mode_source_view.h"
#include "ash/capture_mode/capture_mode_toggle_button.h"
#include "ash/capture_mode/capture_mode_type_view.h"
#include "ash/capture_mode/capture_mode_types.h"
#include "ash/capture_mode/stop_recording_button_tray.h"
#include "ash/display/cursor_window_controller.h"
#include "ash/display/window_tree_host_manager.h"
#include "ash/public/cpp/ash_features.h"
#include "ash/root_window_controller.h"
#include "ash/shell.h"
#include "ash/system/status_area_widget.h"
#include "ash/test/ash_test_base.h"
#include "base/test/scoped_feature_list.h"
#include "ui/events/keycodes/keyboard_codes_posix.h"
#include "ui/views/view.h"

namespace ash {

namespace {

// Returns true if the software-composited cursor is enabled.
bool IsCursorCompositingEnabled() {
  return Shell::Get()
      ->window_tree_host_manager()
      ->cursor_window_controller()
      ->ShouldEnableCursorCompositing();
}

void ClickOnView(const views::View* view,
                 ui::test::EventGenerator* event_generator) {
  DCHECK(view);
  DCHECK(event_generator);

  const gfx::Point view_center = view->GetBoundsInScreen().CenterPoint();
  event_generator->MoveMouseTo(view_center);
  event_generator->ClickLeftButton();
}

void SendKey(ui::KeyboardCode key_code,
             ui::test::EventGenerator* event_generator) {
  event_generator->PressKey(key_code, /*flags=*/0);
  event_generator->ReleaseKey(key_code, /*flags=*/0);
}

}  // namespace

class CaptureModeTest : public AshTestBase {
 public:
  CaptureModeTest() = default;
  CaptureModeTest(const CaptureModeTest&) = delete;
  CaptureModeTest& operator=(const CaptureModeTest&) = delete;
  ~CaptureModeTest() override = default;

  // AshTestBase:
  void SetUp() override {
    scoped_feature_list_.InitAndEnableFeature(features::kCaptureMode);
    AshTestBase::SetUp();
  }

  CaptureModeToggleButton* GetImageToggleButton() const {
    auto* controller = CaptureModeController::Get();
    DCHECK(controller->IsActive());
    return controller->capture_mode_session()
        ->capture_mode_bar_view()
        ->capture_type_view()
        ->image_toggle_button();
  }

  CaptureModeToggleButton* GetVideoToggleButton() const {
    auto* controller = CaptureModeController::Get();
    DCHECK(controller->IsActive());
    return controller->capture_mode_session()
        ->capture_mode_bar_view()
        ->capture_type_view()
        ->video_toggle_button();
  }

  CaptureModeToggleButton* GetFullscreenToggleButton() const {
    auto* controller = CaptureModeController::Get();
    DCHECK(controller->IsActive());
    return controller->capture_mode_session()
        ->capture_mode_bar_view()
        ->capture_source_view()
        ->fullscreen_toggle_button();
  }

  CaptureModeToggleButton* GetRegionToggleButton() const {
    auto* controller = CaptureModeController::Get();
    DCHECK(controller->IsActive());
    return controller->capture_mode_session()
        ->capture_mode_bar_view()
        ->capture_source_view()
        ->region_toggle_button();
  }

  CaptureModeToggleButton* GetWindowToggleButton() const {
    auto* controller = CaptureModeController::Get();
    DCHECK(controller->IsActive());
    return controller->capture_mode_session()
        ->capture_mode_bar_view()
        ->capture_source_view()
        ->window_toggle_button();
  }

  CaptureModeCloseButton* GetCloseButton() const {
    auto* controller = CaptureModeController::Get();
    DCHECK(controller->IsActive());
    return controller->capture_mode_session()
        ->capture_mode_bar_view()
        ->close_button();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(CaptureModeTest, StartStop) {
  auto* controller = CaptureModeController::Get();
  controller->Start();
  EXPECT_TRUE(controller->IsActive());
  // Calling start again is a no-op.
  controller->Start();
  EXPECT_TRUE(controller->IsActive());
  controller->Stop();
  EXPECT_FALSE(controller->IsActive());
}

TEST_F(CaptureModeTest, StartWithMostRecentTypeAndSource) {
  auto* controller = CaptureModeController::Get();
  controller->SetSource(CaptureModeSource::kFullscreen);
  controller->SetType(CaptureModeType::kVideo);
  controller->Start();
  EXPECT_TRUE(controller->IsActive());

  EXPECT_FALSE(GetImageToggleButton()->GetToggled());
  EXPECT_TRUE(GetVideoToggleButton()->GetToggled());
  EXPECT_TRUE(GetFullscreenToggleButton()->GetToggled());
  EXPECT_FALSE(GetRegionToggleButton()->GetToggled());
  EXPECT_FALSE(GetWindowToggleButton()->GetToggled());

  ClickOnView(GetCloseButton(), GetEventGenerator());
  EXPECT_FALSE(controller->IsActive());
}

TEST_F(CaptureModeTest, ChangeTypeAndSourceFromUI) {
  auto* controller = CaptureModeController::Get();
  controller->Start();
  EXPECT_TRUE(controller->IsActive());

  EXPECT_TRUE(GetImageToggleButton()->GetToggled());
  EXPECT_FALSE(GetVideoToggleButton()->GetToggled());
  auto* event_generator = GetEventGenerator();
  ClickOnView(GetVideoToggleButton(), event_generator);
  EXPECT_FALSE(GetImageToggleButton()->GetToggled());
  EXPECT_TRUE(GetVideoToggleButton()->GetToggled());
  EXPECT_EQ(controller->type(), CaptureModeType::kVideo);

  ClickOnView(GetWindowToggleButton(), event_generator);
  EXPECT_FALSE(GetFullscreenToggleButton()->GetToggled());
  EXPECT_FALSE(GetRegionToggleButton()->GetToggled());
  EXPECT_TRUE(GetWindowToggleButton()->GetToggled());
  EXPECT_EQ(controller->source(), CaptureModeSource::kWindow);

  ClickOnView(GetFullscreenToggleButton(), event_generator);
  EXPECT_TRUE(GetFullscreenToggleButton()->GetToggled());
  EXPECT_FALSE(GetRegionToggleButton()->GetToggled());
  EXPECT_FALSE(GetWindowToggleButton()->GetToggled());
  EXPECT_EQ(controller->source(), CaptureModeSource::kFullscreen);
}

TEST_F(CaptureModeTest, VideoRecordingUiBehavior) {
  auto* controller = CaptureModeController::Get();
  // Start Capture Mode in a fullscreen video recording mode.
  controller->SetSource(CaptureModeSource::kFullscreen);
  controller->SetType(CaptureModeType::kVideo);
  controller->Start();
  EXPECT_TRUE(controller->IsActive());
  EXPECT_FALSE(controller->is_recording_in_progress());
  EXPECT_FALSE(IsCursorCompositingEnabled());

  // Hit Enter to begin recording.
  auto* event_generator = GetEventGenerator();
  SendKey(ui::VKEY_RETURN, event_generator);
  EXPECT_FALSE(controller->IsActive());
  EXPECT_TRUE(controller->is_recording_in_progress());

  // The composited cursor should be enabled, and the stop-recording button
  // should show up in the status area widget.
  EXPECT_TRUE(IsCursorCompositingEnabled());
  auto* stop_recording_button = Shell::GetPrimaryRootWindowController()
                                    ->GetStatusAreaWidget()
                                    ->stop_recording_button_tray();
  EXPECT_TRUE(stop_recording_button->visible_preferred());

  // End recording via the stop-recording button. Expect that it's now hidden,
  // and the cursor compositing is now disabled.
  ClickOnView(stop_recording_button, event_generator);
  EXPECT_FALSE(stop_recording_button->visible_preferred());
  EXPECT_FALSE(controller->is_recording_in_progress());
  EXPECT_FALSE(IsCursorCompositingEnabled());
}

}  // namespace ash
