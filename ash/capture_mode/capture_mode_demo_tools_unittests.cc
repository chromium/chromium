// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/capture_mode/capture_mode_controller.h"
#include "ash/capture_mode/capture_mode_demo_tools_controller.h"
#include "ash/capture_mode/capture_mode_test_util.h"
#include "ash/capture_mode/capture_mode_types.h"
#include "ash/constants/ash_features.h"
#include "ash/test/ash_test_base.h"
#include "base/test/scoped_feature_list.h"
#include "ui/events/event.h"
#include "ui/events/event_constants.h"
#include "ui/events/keycodes/keyboard_codes_posix.h"
#include "ui/gfx/geometry/rect.h"

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

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<aura::Window> window_;
};

// Tests that the key event is considered to generate the `demo_tools_widget_`
// or ignored otherwise in a correct way.
TEST_F(CaptureModeDemoToolsTest, ConsiderKeyEvent) {
  const auto* controller =
      StartVideoRecordingWithGivenSource(CaptureModeSource::kFullscreen);
  EXPECT_TRUE(controller->is_recording_in_progress());

  auto* recording_watcher = controller->video_recording_watcher_for_testing();
  DCHECK(recording_watcher);
  auto* demo_tools_controller =
      recording_watcher->demo_tools_controller_for_testing();

  // Press the 'A' key and the event will not be considered to generate a
  // corresponding key widget.
  auto* event_generator = GetEventGenerator();
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

}  // namespace ash