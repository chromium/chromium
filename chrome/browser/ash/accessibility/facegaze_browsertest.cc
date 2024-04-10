// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shell.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ash/accessibility/accessibility_feature_browsertest.h"
#include "chrome/browser/ash/accessibility/facegaze_test_utils.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/test/browser_test.h"
#include "ui/accessibility/accessibility_features.h"
#include "ui/display/screen.h"
#include "ui/display/test/display_manager_test_api.h"
#include "ui/events/test/event_generator.h"
#include "ui/gfx/geometry/point.h"

namespace ash {

using CursorSpeeds = FaceGazeTestUtils::CursorSpeeds;
using MockFaceLandmarkerResult = FaceGazeTestUtils::MockFaceLandmarkerResult;

namespace {

// A class that helps initialize FaceGaze with a configuration.
class Config {
 public:
  Config() = default;
  ~Config() = default;
  Config(const Config&) = delete;
  Config& operator=(const Config&) = delete;

  Config& WithForeheadLocation(double x, double y) {
    forehead_x_ = x;
    forehead_y_ = y;
    return *this;
  }

  Config& WithMouseLocation(const gfx::Point& location) {
    mouse_location_ = location;
    return *this;
  }

  Config& WithBufferSize(int size) {
    buffer_size_ = size;
    return *this;
  }

  Config& WithMouseAcceleration(bool acceleration) {
    use_mouse_acceleration_ = acceleration;
    return *this;
  }

  Config& WithGesturesToMacros(const base::Value::Dict& gestures_to_macros) {
    gestures_to_macros_ = gestures_to_macros.Clone();
    return *this;
  }

  Config& WithGestureConfidences(const base::Value::Dict& gesture_confidences) {
    gesture_confidences_ = gesture_confidences.Clone();
    return *this;
  }

  Config& WithCursorSpeeds(const CursorSpeeds& speeds) {
    cursor_speeds_ = speeds;
    return *this;
  }

  double forehead_x() const { return forehead_x_; }
  double forehead_y() const { return forehead_y_; }
  const gfx::Point& mouse_location() const { return mouse_location_; }
  int buffer_size() const { return buffer_size_; }
  bool use_mouse_acceleration() const { return use_mouse_acceleration_; }
  const base::Value::Dict& gestures_to_macros() const {
    return gestures_to_macros_;
  }
  const base::Value::Dict& gesture_confidences() const {
    return gesture_confidences_;
  }
  const CursorSpeeds& cursor_speeds() const { return cursor_speeds_; }

 private:
  double forehead_x_;
  double forehead_y_;
  gfx::Point mouse_location_;
  int buffer_size_;
  bool use_mouse_acceleration_;
  base::Value::Dict gestures_to_macros_;
  base::Value::Dict gesture_confidences_;
  CursorSpeeds cursor_speeds_;
};

}  // namespace

class FaceGazeIntegrationTest : public AccessibilityFeatureBrowserTest {
 public:
  FaceGazeIntegrationTest() = default;
  ~FaceGazeIntegrationTest() override = default;
  FaceGazeIntegrationTest(const FaceGazeIntegrationTest&) = delete;
  FaceGazeIntegrationTest& operator=(const FaceGazeIntegrationTest&) = delete;

 protected:
  // InProcessBrowserTest:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    utils_ = std::make_unique<FaceGazeTestUtils>();
    scoped_feature_list_.InitAndEnableFeature(
        ::features::kAccessibilityFaceGaze);
    InProcessBrowserTest::SetUpCommandLine(command_line);
  }

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    event_generator_ = std::make_unique<ui::test::EventGenerator>(
        Shell::Get()->GetPrimaryRootWindow());
    display::test::DisplayManagerTestApi(Shell::Get()->display_manager())
        .UpdateDisplay("1200x800");

    // Initialize FaceGaze.
    utils_->EnableFaceGaze();
    utils_->CreateFaceLandmarker();
  }

  void ConfigureFaceGaze(const Config& config) {
    utils_->SetCursorSpeeds(config.cursor_speeds());
    utils_->SetBufferSize(config.buffer_size());
    utils_->SetMouseAcceleration(config.use_mouse_acceleration());
    utils_->SetGesturesToMacros(config.gestures_to_macros());
    utils_->SetGestureConfidences(config.gesture_confidences());
    SetMouseSourceDeviceId(1);
    // By default the mouse is placed at the center of the screen. To initialize
    // FaceGaze, move the mouse somewhere, then move it to the location
    // specified by the config.
    SendMouseMoveTo(gfx::Point(0, 0));
    utils_->WaitForMousePosition(gfx::Point(0, 0));
    SendMouseMoveTo(config.mouse_location());
    utils_->WaitForMousePosition(config.mouse_location());

    // No matter the starting location, the cursor position won't change
    // initially, and upcoming forehead locations will be computed relative to
    // this.
    utils_->ProcessFaceLandmarkerResult(
        MockFaceLandmarkerResult().WithNormalizedForeheadLocation(
            config.forehead_x(), config.forehead_y()));
    utils_->TriggerMouseControllerInterval();
    ASSERT_EQ(config.mouse_location(),
              display::Screen::GetScreen()->GetCursorScreenPoint());
  }

  void SendMouseMoveTo(const gfx::Point& location) {
    event_generator_->MoveMouseTo(location.x(), location.y());
  }

  void SetMouseSourceDeviceId(int id) {
    event_generator_->set_mouse_source_device_id(id);
  }

  FaceGazeTestUtils* utils() { return utils_.get(); }

 private:
  std::unique_ptr<FaceGazeTestUtils> utils_;
  std::unique_ptr<ui::test::EventGenerator> event_generator_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(FaceGazeIntegrationTest, UpdateMouseLocation) {
  ConfigureFaceGaze(
      Config()
          .WithForeheadLocation(0.1, 0.2)
          .WithMouseLocation(gfx::Point(600, 400))
          .WithBufferSize(1)
          .WithMouseAcceleration(false)
          .WithGesturesToMacros(
              base::Value::Dict().Set("jawOpen", /*RESET_CURSOR*/ 37))
          .WithGestureConfidences(base::Value::Dict().Set("jawOpen", 70)));

  // Move mouse using forehead.
  utils()->ProcessFaceLandmarkerResult(
      MockFaceLandmarkerResult().WithNormalizedForeheadLocation(0.11, 0.21));
  utils()->TriggerMouseControllerInterval();
  ASSERT_EQ(gfx::Point(360, 560),
            display::Screen::GetScreen()->GetCursorScreenPoint());
}

IN_PROC_BROWSER_TEST_F(FaceGazeIntegrationTest, ResetCursor) {
  ConfigureFaceGaze(
      Config()
          .WithForeheadLocation(0.1, 0.2)
          .WithMouseLocation(gfx::Point(600, 400))
          .WithBufferSize(1)
          .WithMouseAcceleration(false)
          .WithGesturesToMacros(
              base::Value::Dict().Set("jawOpen", /*RESET_CURSOR*/ 37))
          .WithGestureConfidences(base::Value::Dict().Set("jawOpen", 70)));

  // Move mouse.
  utils()->ProcessFaceLandmarkerResult(
      MockFaceLandmarkerResult().WithNormalizedForeheadLocation(0.11, 0.21));
  utils()->TriggerMouseControllerInterval();
  ASSERT_EQ(gfx::Point(360, 560),
            display::Screen::GetScreen()->GetCursorScreenPoint());

  // Reset the mouse to the center of the screen using a gesture.
  utils()->ProcessFaceLandmarkerResult(
      MockFaceLandmarkerResult().WithGesture("jawOpen", 90));
  ASSERT_EQ(gfx::Point(600, 400),
            display::Screen::GetScreen()->GetCursorScreenPoint());
}

IN_PROC_BROWSER_TEST_F(FaceGazeIntegrationTest,
                       IgnoreGesturesWithLowConfidence) {
  ConfigureFaceGaze(
      Config()
          .WithForeheadLocation(0.1, 0.2)
          .WithMouseLocation(gfx::Point(600, 400))
          .WithBufferSize(1)
          .WithMouseAcceleration(false)
          .WithGesturesToMacros(
              base::Value::Dict().Set("jawOpen", /*RESET_CURSOR*/ 37))
          .WithGestureConfidences(base::Value::Dict().Set("jawOpen", 100)));

  // Move mouse.
  utils()->ProcessFaceLandmarkerResult(
      MockFaceLandmarkerResult().WithNormalizedForeheadLocation(0.11, 0.21));
  utils()->TriggerMouseControllerInterval();
  ASSERT_EQ(gfx::Point(360, 560),
            display::Screen::GetScreen()->GetCursorScreenPoint());

  // Attempt to reset the mouse to the center of the screen using a gesture.
  // This gesture will be ignored because the gesture doesn't have high enough
  // confidence.
  utils()->ProcessFaceLandmarkerResult(
      MockFaceLandmarkerResult().WithGesture("jawOpen", 90));
  ASSERT_EQ(gfx::Point(360, 560),
            display::Screen::GetScreen()->GetCursorScreenPoint());
}

// TODO(b/309121742): Convert all instances of "mouse" into "cursor".
IN_PROC_BROWSER_TEST_F(FaceGazeIntegrationTest,
                       UpdateCursorLocationWithSpeed1) {
  ConfigureFaceGaze(
      Config()
          .WithForeheadLocation(0.1, 0.2)
          .WithMouseLocation(gfx::Point(600, 400))
          .WithBufferSize(1)
          .WithMouseAcceleration(false)
          .WithGesturesToMacros(
              base::Value::Dict().Set("jawOpen", /*RESET_CURSOR*/ 37))
          .WithGestureConfidences(base::Value::Dict().Set("jawOpen", 70))
          .WithCursorSpeeds({/*up=*/1, /*down=*/1, /*left=*/1, /*right=*/1}));

  // With mouse acceleration off and buffer size 1, one-pixel head movements
  // correspond to one-pixel changes on screen.
  double px = 1.0 / 1200;
  double py = 1.0 / 800;
  for (int i = 1; i < 10; ++i) {
    utils()->ProcessFaceLandmarkerResult(
        MockFaceLandmarkerResult().WithNormalizedForeheadLocation(
            0.1 + px * i, 0.2 + py * i));
    utils()->TriggerMouseControllerInterval();
    ASSERT_EQ(gfx::Point(600 - i, 400 + i),
              display::Screen::GetScreen()->GetCursorScreenPoint());
  }
}

}  // namespace ash
