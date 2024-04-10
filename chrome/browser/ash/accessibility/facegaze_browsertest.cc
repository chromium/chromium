// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <optional>

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
#include "ui/gfx/geometry/point_f.h"

namespace ash {

using CursorSpeeds = FaceGazeTestUtils::CursorSpeeds;
using MockFaceLandmarkerResult = FaceGazeTestUtils::MockFaceLandmarkerResult;

namespace {

const char* kDefaultDisplaySize = "1200x800";

// A class that helps initialize FaceGaze with a configuration.
class Config {
 public:
  Config() = default;
  ~Config() = default;
  Config(const Config&) = delete;
  Config& operator=(const Config&) = delete;

  // Returns a Config that sets required properties to default values.
  Config& AsDefault() {
    forehead_location_ = gfx::PointF(0.1, 0.2);
    cursor_location_ = gfx::Point(600, 400);
    buffer_size_ = 1;
    use_cursor_acceleration_ = false;
    return *this;
  }

  Config& WithForeheadLocation(const gfx::PointF& location) {
    forehead_location_ = location;
    return *this;
  }

  Config& WithCursorLocation(const gfx::Point& location) {
    cursor_location_ = location;
    return *this;
  }

  Config& WithBufferSize(int size) {
    buffer_size_ = size;
    return *this;
  }

  Config& WithCursorAcceleration(bool acceleration) {
    use_cursor_acceleration_ = acceleration;
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

  const gfx::PointF& forehead_location() const { return forehead_location_; }
  const gfx::Point& cursor_location() const { return cursor_location_; }
  int buffer_size() const { return buffer_size_; }
  bool use_cursor_acceleration() const { return use_cursor_acceleration_; }
  const std::optional<base::Value::Dict>& gestures_to_macros() const {
    return gestures_to_macros_;
  }
  const std::optional<base::Value::Dict>& gesture_confidences() const {
    return gesture_confidences_;
  }
  const std::optional<CursorSpeeds>& cursor_speeds() const {
    return cursor_speeds_;
  }

 private:
  // Required properties.
  gfx::PointF forehead_location_;
  gfx::Point cursor_location_;
  int buffer_size_;
  bool use_cursor_acceleration_;

  // Optional properties.
  std::optional<base::Value::Dict> gestures_to_macros_;
  std::optional<base::Value::Dict> gesture_confidences_;
  std::optional<CursorSpeeds> cursor_speeds_;
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
        .UpdateDisplay(kDefaultDisplaySize);

    // Initialize FaceGaze.
    utils_->EnableFaceGaze();
    utils_->CreateFaceLandmarker();
  }

  void ConfigureFaceGaze(const Config& config) {
    // Set optional configuration properties.
    if (config.cursor_speeds().has_value()) {
      utils_->SetCursorSpeeds(config.cursor_speeds().value());
    }
    if (config.gestures_to_macros().has_value()) {
      utils_->SetGesturesToMacros(config.gestures_to_macros().value());
    }
    if (config.gesture_confidences().has_value()) {
      utils_->SetGestureConfidences(config.gesture_confidences().value());
    }

    // Set required configuration properties.
    utils_->SetBufferSize(config.buffer_size());
    utils_->SetCursorAcceleration(config.use_cursor_acceleration());

    // By default the cursor is placed at the center of the screen. To
    // initialize FaceGaze, move the cursor somewhere, then move it to the
    // location specified by the config.
    event_generator_->set_mouse_source_device_id(1);
    MoveMouseTo(gfx::Point(0, 0));
    AssertCursorAt(gfx::Point(0, 0));
    MoveMouseTo(config.cursor_location());
    AssertCursorAt(config.cursor_location());

    // No matter the starting location, the cursor position won't change
    // initially, and upcoming forehead locations will be computed relative to
    // this.
    utils_->ProcessFaceLandmarkerResult(
        MockFaceLandmarkerResult().WithNormalizedForeheadLocation(
            config.forehead_location().x(), config.forehead_location().y()));
    utils_->TriggerMouseControllerInterval();
    AssertCursorAt(config.cursor_location());
  }

  void MoveMouseTo(const gfx::Point& location) {
    event_generator_->MoveMouseTo(location.x(), location.y());
  }

  void AssertCursorAt(const gfx::Point& location) {
    utils_->WaitForCursorPosition(location);
    ASSERT_EQ(location, display::Screen::GetScreen()->GetCursorScreenPoint());
  }

  FaceGazeTestUtils* utils() { return utils_.get(); }

 private:
  std::unique_ptr<FaceGazeTestUtils> utils_;
  std::unique_ptr<ui::test::EventGenerator> event_generator_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(FaceGazeIntegrationTest, UpdateCursorLocation) {
  ConfigureFaceGaze(Config().AsDefault());

  // Move cursor using forehead.
  utils()->ProcessFaceLandmarkerResult(
      MockFaceLandmarkerResult().WithNormalizedForeheadLocation(0.11, 0.21));
  utils()->TriggerMouseControllerInterval();
  AssertCursorAt(gfx::Point(360, 560));
}

IN_PROC_BROWSER_TEST_F(FaceGazeIntegrationTest, ResetCursor) {
  ConfigureFaceGaze(
      Config()
          .AsDefault()
          .WithGesturesToMacros(
              base::Value::Dict().Set("jawOpen", /*RESET_CURSOR*/ 37))
          .WithGestureConfidences(base::Value::Dict().Set("jawOpen", 70)));

  // Move cursor.
  utils()->ProcessFaceLandmarkerResult(
      MockFaceLandmarkerResult().WithNormalizedForeheadLocation(0.11, 0.21));
  utils()->TriggerMouseControllerInterval();
  AssertCursorAt(gfx::Point(360, 560));

  // Reset the cursor to the center of the screen using a gesture.
  utils()->ProcessFaceLandmarkerResult(
      MockFaceLandmarkerResult().WithGesture("jawOpen", 90));
  AssertCursorAt(gfx::Point(600, 400));
}

IN_PROC_BROWSER_TEST_F(FaceGazeIntegrationTest,
                       IgnoreGesturesWithLowConfidence) {
  ConfigureFaceGaze(
      Config()
          .AsDefault()
          .WithGesturesToMacros(
              base::Value::Dict().Set("jawOpen", /*RESET_CURSOR*/ 37))
          .WithGestureConfidences(base::Value::Dict().Set("jawOpen", 100)));

  // Move cursor.
  utils()->ProcessFaceLandmarkerResult(
      MockFaceLandmarkerResult().WithNormalizedForeheadLocation(0.11, 0.21));
  utils()->TriggerMouseControllerInterval();
  AssertCursorAt(gfx::Point(360, 560));

  // Attempt to reset the cursor to the center of the screen using a gesture.
  // This gesture will be ignored because the gesture doesn't have high enough
  // confidence.
  utils()->ProcessFaceLandmarkerResult(
      MockFaceLandmarkerResult().WithGesture("jawOpen", 90));
  AssertCursorAt(gfx::Point(360, 560));
}

IN_PROC_BROWSER_TEST_F(FaceGazeIntegrationTest,
                       UpdateCursorLocationWithSpeed1) {
  ConfigureFaceGaze(Config().AsDefault().WithCursorSpeeds(
      {/*up=*/1, /*down=*/1, /*left=*/1, /*right=*/1}));

  // With cursor acceleration off and buffer size 1, one-pixel head movements
  // correspond to one-pixel changes on screen.
  double px = 1.0 / 1200;
  double py = 1.0 / 800;
  for (int i = 1; i < 10; ++i) {
    utils()->ProcessFaceLandmarkerResult(
        MockFaceLandmarkerResult().WithNormalizedForeheadLocation(
            0.1 + px * i, 0.2 + py * i));
    utils()->TriggerMouseControllerInterval();
    AssertCursorAt(gfx::Point(600 - i, 400 + i));
  }
}

}  // namespace ash
