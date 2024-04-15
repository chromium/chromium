// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <optional>

#include "ash/shell.h"
#include "base/containers/flat_map.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ash/accessibility/accessibility_feature_browsertest.h"
#include "chrome/browser/ash/accessibility/facegaze_test_utils.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/test/browser_test.h"
#include "ui/accessibility/accessibility_features.h"
#include "ui/display/screen.h"
#include "ui/display/test/display_manager_test_api.h"
#include "ui/events/event.h"
#include "ui/events/event_handler.h"
#include "ui/events/keycodes/keyboard_codes_posix.h"
#include "ui/events/test/event_generator.h"
#include "ui/events/types/event_type.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/point_f.h"

namespace ash {

using CursorSpeeds = FaceGazeTestUtils::CursorSpeeds;
using FaceGazeGesture = FaceGazeTestUtils::FaceGazeGesture;
using MacroName = FaceGazeTestUtils::MacroName;
using MediapipeGesture = FaceGazeTestUtils::MediapipeGesture;
using MockFaceLandmarkerResult = FaceGazeTestUtils::MockFaceLandmarkerResult;

namespace {

const int kMouseDeviceId = 1;
const char* kDefaultDisplaySize = "1200x800";

aura::Window* GetRootWindow() {
  auto* root_window = Shell::GetRootWindowForNewWindows();
  if (!root_window) {
    root_window = Shell::GetPrimaryRootWindow();
  }

  return root_window;
}

// A class that records mouse and key events.
class MockEventHandler : public ui::EventHandler {
 public:
  MockEventHandler() = default;
  ~MockEventHandler() override = default;
  MockEventHandler(const MockEventHandler&) = delete;
  MockEventHandler& operator=(const MockEventHandler&) = delete;

  void OnKeyEvent(ui::KeyEvent* event) override {
    key_events_.push_back(*event);
  }

  void OnMouseEvent(ui::MouseEvent* event) override {
    ui::EventType type = event->type();
    if (type == ui::EventType::ET_MOUSE_PRESSED ||
        type == ui::EventType::ET_MOUSE_RELEASED ||
        type == ui::EventType::ET_MOUSE_MOVED) {
      mouse_events_.push_back(*event);
    }
  }

  void ClearEvents() {
    key_events_.clear();
    mouse_events_.clear();
  }

  const std::vector<ui::KeyEvent>& key_events() const { return key_events_; }
  const std::vector<ui::MouseEvent>& mouse_events() const {
    return mouse_events_;
  }

 private:
  std::vector<ui::KeyEvent> key_events_;
  std::vector<ui::MouseEvent> mouse_events_;
};

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

  Config& WithGesturesToMacros(
      const base::flat_map<FaceGazeGesture, MacroName>& gestures_to_macros) {
    gestures_to_macros_ = std::move(gestures_to_macros);
    return *this;
  }

  Config& WithGestureConfidences(
      const base::flat_map<FaceGazeGesture, int>& gesture_confidences) {
    gesture_confidences_ = std::move(gesture_confidences);
    return *this;
  }

  Config& WithCursorSpeeds(const CursorSpeeds& speeds) {
    cursor_speeds_ = speeds;
    return *this;
  }

  Config& WithGestureRepeatDelayMs(int delay) {
    gesture_repeat_delay_ms_ = delay;
    return *this;
  }

  const gfx::PointF& forehead_location() const { return forehead_location_; }
  const gfx::Point& cursor_location() const { return cursor_location_; }
  int buffer_size() const { return buffer_size_; }
  bool use_cursor_acceleration() const { return use_cursor_acceleration_; }
  const std::optional<base::flat_map<FaceGazeGesture, MacroName>>&
  gestures_to_macros() const {
    return gestures_to_macros_;
  }
  const std::optional<base::flat_map<FaceGazeGesture, int>>&
  gesture_confidences() const {
    return gesture_confidences_;
  }
  const std::optional<CursorSpeeds>& cursor_speeds() const {
    return cursor_speeds_;
  }
  std::optional<int> gesture_repeat_delay_ms() const {
    return gesture_repeat_delay_ms_;
  }

 private:
  // Required properties.
  gfx::PointF forehead_location_;
  gfx::Point cursor_location_;
  int buffer_size_;
  bool use_cursor_acceleration_;

  // Optional properties.
  std::optional<base::flat_map<FaceGazeGesture, MacroName>> gestures_to_macros_;
  std::optional<base::flat_map<FaceGazeGesture, int>> gesture_confidences_;
  std::optional<CursorSpeeds> cursor_speeds_;
  std::optional<int> gesture_repeat_delay_ms_;
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
    GetRootWindow()->AddPreTargetHandler(&event_handler_);
    event_generator_ = std::make_unique<ui::test::EventGenerator>(
        Shell::Get()->GetPrimaryRootWindow());
    display::test::DisplayManagerTestApi(Shell::Get()->display_manager())
        .UpdateDisplay(kDefaultDisplaySize);

    // Initialize FaceGaze.
    utils_->EnableFaceGaze();
    utils_->CreateFaceLandmarker();
  }

  void TearDownOnMainThread() override {
    GetRootWindow()->RemovePreTargetHandler(&event_handler_);
    InProcessBrowserTest::TearDownOnMainThread();
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
    if (config.gesture_repeat_delay_ms().has_value()) {
      utils_->SetGestureRepeatDelayMs(config.gesture_repeat_delay_ms().value());
    }

    // Set required configuration properties.
    utils_->SetBufferSize(config.buffer_size());
    utils_->SetCursorAcceleration(config.use_cursor_acceleration());

    // By default the cursor is placed at the center of the screen. To
    // initialize FaceGaze, move the cursor somewhere, then move it to the
    // location specified by the config.
    event_generator_->set_mouse_source_device_id(kMouseDeviceId);
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

  void AssertLatestMouseEvent(size_t num_events,
                              ui::EventType type,
                              const gfx::Point& root_location) {
    std::vector<ui::MouseEvent> mouse_events = event_handler().mouse_events();
    ASSERT_GT(mouse_events.size(), 0u);
    ASSERT_EQ(num_events, mouse_events.size());
    ASSERT_EQ(type, mouse_events[0].type());
    ASSERT_EQ(root_location, mouse_events[0].root_location());
    // All FaceGaze mouse events should be synthesized.
    ASSERT_TRUE(mouse_events[0].IsSynthesized());
  }

  MockEventHandler& event_handler() { return event_handler_; }
  FaceGazeTestUtils* utils() { return utils_.get(); }

 private:
  std::unique_ptr<FaceGazeTestUtils> utils_;
  std::unique_ptr<ui::test::EventGenerator> event_generator_;
  MockEventHandler event_handler_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(FaceGazeIntegrationTest, UpdateCursorLocation) {
  ConfigureFaceGaze(Config().AsDefault());
  event_handler().ClearEvents();

  utils()->ProcessFaceLandmarkerResult(
      MockFaceLandmarkerResult().WithNormalizedForeheadLocation(0.11, 0.21));
  utils()->TriggerMouseControllerInterval();
  AssertCursorAt(gfx::Point(360, 560));

  // We expect two mouse move events to be received because the FaceGaze
  // extension calls two APIs to update the cursor position.
  const std::vector<ui::MouseEvent> mouse_events =
      event_handler().mouse_events();
  ASSERT_EQ(2u, mouse_events.size());
  ASSERT_EQ(ui::ET_MOUSE_MOVED, mouse_events[0].type());
  ASSERT_EQ(gfx::Point(360, 560), mouse_events[0].root_location());
  ASSERT_TRUE(mouse_events[0].IsSynthesized());
  ASSERT_EQ(ui::ET_MOUSE_MOVED, mouse_events[1].type());
  ASSERT_EQ(gfx::Point(360, 560), mouse_events[1].root_location());
  ASSERT_TRUE(mouse_events[1].IsSynthesized());
}

IN_PROC_BROWSER_TEST_F(FaceGazeIntegrationTest, ResetCursor) {
  ConfigureFaceGaze(
      Config()
          .AsDefault()
          .WithGesturesToMacros(
              {{FaceGazeGesture::JAW_OPEN, MacroName::RESET_CURSOR}})
          .WithGestureConfidences({{FaceGazeGesture::JAW_OPEN, 70}}));

  // Move cursor.
  utils()->ProcessFaceLandmarkerResult(
      MockFaceLandmarkerResult().WithNormalizedForeheadLocation(0.11, 0.21));
  utils()->TriggerMouseControllerInterval();
  AssertCursorAt(gfx::Point(360, 560));

  event_handler().ClearEvents();

  // Reset the cursor to the center of the screen using a gesture.
  utils()->ProcessFaceLandmarkerResult(
      MockFaceLandmarkerResult().WithGesture(MediapipeGesture::JAW_OPEN, 90));
  AssertCursorAt(gfx::Point(600, 400));

  // We expect one mouse move event to be received because the FaceGaze
  // extension only calls one API to reset the cursor position.
  const std::vector<ui::MouseEvent> mouse_events =
      event_handler().mouse_events();
  ASSERT_EQ(1u, mouse_events.size());
  ASSERT_EQ(ui::ET_MOUSE_MOVED, mouse_events[0].type());
  ASSERT_EQ(gfx::Point(600, 400), mouse_events[0].root_location());
  ASSERT_TRUE(mouse_events[0].IsSynthesized());
}

IN_PROC_BROWSER_TEST_F(FaceGazeIntegrationTest,
                       IgnoreGesturesWithLowConfidence) {
  ConfigureFaceGaze(
      Config()
          .AsDefault()
          .WithGesturesToMacros(
              {{FaceGazeGesture::JAW_OPEN, MacroName::RESET_CURSOR}})
          .WithGestureConfidences({{FaceGazeGesture::JAW_OPEN, 100}}));

  // Move cursor.
  utils()->ProcessFaceLandmarkerResult(
      MockFaceLandmarkerResult().WithNormalizedForeheadLocation(0.11, 0.21));
  utils()->TriggerMouseControllerInterval();
  AssertCursorAt(gfx::Point(360, 560));

  // Attempt to reset the cursor to the center of the screen using a gesture.
  // This gesture will be ignored because the gesture doesn't have high enough
  // confidence.
  event_handler().ClearEvents();
  utils()->ProcessFaceLandmarkerResult(
      MockFaceLandmarkerResult().WithGesture(MediapipeGesture::JAW_OPEN, 90));
  AssertCursorAt(gfx::Point(360, 560));
  ASSERT_EQ(0u, event_handler().mouse_events().size());
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

IN_PROC_BROWSER_TEST_F(FaceGazeIntegrationTest, SpaceKeyEvents) {
  ConfigureFaceGaze(
      Config()
          .AsDefault()
          .WithGesturesToMacros(
              {{FaceGazeGesture::MOUTH_LEFT, MacroName::KEY_PRESS_SPACE}})
          .WithGestureConfidences({{FaceGazeGesture::MOUTH_LEFT, 70}}));

  // Open jaw for space key press.
  event_handler().ClearEvents();
  utils()->ProcessFaceLandmarkerResult(
      MockFaceLandmarkerResult().WithGesture(MediapipeGesture::MOUTH_LEFT, 90));
  ASSERT_EQ(0u, event_handler().mouse_events().size());
  std::vector<ui::KeyEvent> key_events = event_handler().key_events();
  ASSERT_EQ(1u, key_events.size());
  ASSERT_EQ(ui::KeyboardCode::VKEY_SPACE, key_events[0].key_code());
  ASSERT_EQ(ui::EventType::ET_KEY_PRESSED, key_events[0].type());

  // Release gesture for space key release.
  utils()->ProcessFaceLandmarkerResult(
      MockFaceLandmarkerResult().WithGesture(MediapipeGesture::MOUTH_LEFT, 10));
  ASSERT_EQ(0u, event_handler().mouse_events().size());
  key_events = event_handler().key_events();
  ASSERT_EQ(2u, event_handler().key_events().size());
  ASSERT_EQ(ui::KeyboardCode::VKEY_SPACE, key_events[1].key_code());
  ASSERT_EQ(ui::EventType::ET_KEY_RELEASED, key_events[1].type());
}

// The BrowsDown gesture is special because it is the combination of two
// separate facial gestures (BROW_DOWN_LEFT and BROW_DOWN_RIGHT). This test
// ensures that the associated action is performed if either of the gestures is
// detected.
IN_PROC_BROWSER_TEST_F(FaceGazeIntegrationTest, BrowsDownGesture) {
  ConfigureFaceGaze(
      Config()
          .AsDefault()
          .WithCursorLocation(gfx::Point(0, 0))
          .WithGesturesToMacros(
              {{FaceGazeGesture::BROWS_DOWN, MacroName::RESET_CURSOR}})
          .WithGestureConfidences({{FaceGazeGesture::BROWS_DOWN, 40}})
          .WithGestureRepeatDelayMs(0));

  // If neither gesture is detected, then don't perform the associated action.
  event_handler().ClearEvents();
  utils()->ProcessFaceLandmarkerResult(
      MockFaceLandmarkerResult()
          .WithGesture(MediapipeGesture::BROW_DOWN_LEFT, 30)
          .WithGesture(MediapipeGesture::BROW_DOWN_RIGHT, 30));
  ASSERT_EQ(0u, event_handler().mouse_events().size());

  // If BROW_DOWN_LEFT is recognized, then perform the action.
  event_handler().ClearEvents();
  utils()->ProcessFaceLandmarkerResult(
      MockFaceLandmarkerResult()
          .WithGesture(MediapipeGesture::BROW_DOWN_LEFT, 50)
          .WithGesture(MediapipeGesture::BROW_DOWN_RIGHT, 30));
  AssertCursorAt(gfx::Point(600, 400));
  AssertLatestMouseEvent(1, ui::ET_MOUSE_MOVED, gfx::Point(600, 400));

  // Reset the mouse cursor away from the center.
  MoveMouseTo(gfx::Point(0, 0));
  AssertCursorAt(gfx::Point(0, 0));

  // If BROW_DOWN_RIGHT is recognized, then perform the action.
  event_handler().ClearEvents();
  utils()->ProcessFaceLandmarkerResult(
      MockFaceLandmarkerResult()
          .WithGesture(MediapipeGesture::BROW_DOWN_LEFT, 30)
          .WithGesture(MediapipeGesture::BROW_DOWN_RIGHT, 50));
  AssertCursorAt(gfx::Point(600, 400));
  AssertLatestMouseEvent(1, ui::ET_MOUSE_MOVED, gfx::Point(600, 400));

  // Reset the mouse cursor away from the center.
  MoveMouseTo(gfx::Point(0, 0));
  AssertCursorAt(gfx::Point(0, 0));

  // If both of the gestures are recognized, then perform the action.
  event_handler().ClearEvents();
  utils()->ProcessFaceLandmarkerResult(
      MockFaceLandmarkerResult()
          .WithGesture(MediapipeGesture::BROW_DOWN_LEFT, 50)
          .WithGesture(MediapipeGesture::BROW_DOWN_RIGHT, 50));
  AssertCursorAt(gfx::Point(600, 400));
  AssertLatestMouseEvent(1, ui::ET_MOUSE_MOVED, gfx::Point(600, 400));
}

}  // namespace ash
