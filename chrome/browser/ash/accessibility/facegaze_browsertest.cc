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
#include "ui/events/event.h"
#include "ui/events/event_handler.h"
#include "ui/events/keycodes/keyboard_codes_posix.h"
#include "ui/events/types/event_type.h"
#include "ui/gfx/geometry/point.h"

namespace ash {

using Config = FaceGazeTestUtils::Config;
using FaceGazeGesture = FaceGazeTestUtils::FaceGazeGesture;
using MacroName = FaceGazeTestUtils::MacroName;
using MediapipeGesture = FaceGazeTestUtils::MediapipeGesture;
using MockFaceLandmarkerResult = FaceGazeTestUtils::MockFaceLandmarkerResult;

namespace {

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
  }

  void TearDownOnMainThread() override {
    GetRootWindow()->RemovePreTargetHandler(&event_handler_);
    InProcessBrowserTest::TearDownOnMainThread();
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
  MockEventHandler event_handler_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(FaceGazeIntegrationTest, UpdateCursorLocation) {
  utils()->EnableFaceGaze(Config().Default());
  event_handler().ClearEvents();

  utils()->ProcessFaceLandmarkerResult(
      MockFaceLandmarkerResult().WithNormalizedForeheadLocation(0.11, 0.21));
  utils()->TriggerMouseControllerInterval();
  utils()->AssertCursorAt(gfx::Point(360, 560));

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
  utils()->EnableFaceGaze(
      Config()
          .Default()
          .WithGesturesToMacros(
              {{FaceGazeGesture::JAW_OPEN, MacroName::RESET_CURSOR}})
          .WithGestureConfidences({{FaceGazeGesture::JAW_OPEN, 70}}));

  // Move cursor.
  utils()->ProcessFaceLandmarkerResult(
      MockFaceLandmarkerResult().WithNormalizedForeheadLocation(0.11, 0.21));
  utils()->TriggerMouseControllerInterval();
  utils()->AssertCursorAt(gfx::Point(360, 560));

  event_handler().ClearEvents();

  // Reset the cursor to the center of the screen using a gesture.
  utils()->ProcessFaceLandmarkerResult(
      MockFaceLandmarkerResult().WithGesture(MediapipeGesture::JAW_OPEN, 90));
  utils()->AssertCursorAt(gfx::Point(600, 400));

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
  utils()->EnableFaceGaze(
      Config()
          .Default()
          .WithGesturesToMacros(
              {{FaceGazeGesture::JAW_OPEN, MacroName::RESET_CURSOR}})
          .WithGestureConfidences({{FaceGazeGesture::JAW_OPEN, 100}}));

  // Move cursor.
  utils()->ProcessFaceLandmarkerResult(
      MockFaceLandmarkerResult().WithNormalizedForeheadLocation(0.11, 0.21));
  utils()->TriggerMouseControllerInterval();
  utils()->AssertCursorAt(gfx::Point(360, 560));

  // Attempt to reset the cursor to the center of the screen using a gesture.
  // This gesture will be ignored because the gesture doesn't have high enough
  // confidence.
  event_handler().ClearEvents();
  utils()->ProcessFaceLandmarkerResult(
      MockFaceLandmarkerResult().WithGesture(MediapipeGesture::JAW_OPEN, 90));
  utils()->AssertCursorAt(gfx::Point(360, 560));
  ASSERT_EQ(0u, event_handler().mouse_events().size());
}

IN_PROC_BROWSER_TEST_F(FaceGazeIntegrationTest,
                       UpdateCursorLocationWithSpeed1) {
  utils()->EnableFaceGaze(Config().Default().WithCursorSpeeds(
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
    utils()->AssertCursorAt(gfx::Point(600 - i, 400 + i));
  }
}

IN_PROC_BROWSER_TEST_F(FaceGazeIntegrationTest, SpaceKeyEvents) {
  utils()->EnableFaceGaze(
      Config()
          .Default()
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
  utils()->EnableFaceGaze(
      Config()
          .Default()
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
  utils()->AssertCursorAt(gfx::Point(600, 400));
  AssertLatestMouseEvent(1, ui::ET_MOUSE_MOVED, gfx::Point(600, 400));

  // Reset the mouse cursor away from the center.
  utils()->MoveMouseTo(gfx::Point(0, 0));
  utils()->AssertCursorAt(gfx::Point(0, 0));

  // If BROW_DOWN_RIGHT is recognized, then perform the action.
  event_handler().ClearEvents();
  utils()->ProcessFaceLandmarkerResult(
      MockFaceLandmarkerResult()
          .WithGesture(MediapipeGesture::BROW_DOWN_LEFT, 30)
          .WithGesture(MediapipeGesture::BROW_DOWN_RIGHT, 50));
  utils()->AssertCursorAt(gfx::Point(600, 400));
  AssertLatestMouseEvent(1, ui::ET_MOUSE_MOVED, gfx::Point(600, 400));

  // Reset the mouse cursor away from the center.
  utils()->MoveMouseTo(gfx::Point(0, 0));
  utils()->AssertCursorAt(gfx::Point(0, 0));

  // If both of the gestures are recognized, then perform the action.
  event_handler().ClearEvents();
  utils()->ProcessFaceLandmarkerResult(
      MockFaceLandmarkerResult()
          .WithGesture(MediapipeGesture::BROW_DOWN_LEFT, 50)
          .WithGesture(MediapipeGesture::BROW_DOWN_RIGHT, 50));
  utils()->AssertCursorAt(gfx::Point(600, 400));
  AssertLatestMouseEvent(1, ui::ET_MOUSE_MOVED, gfx::Point(600, 400));
}

IN_PROC_BROWSER_TEST_F(FaceGazeIntegrationTest, MousePressAndReleaseEvents) {
  utils()->EnableFaceGaze(
      Config()
          .Default()
          .WithGesturesToMacros(
              {{FaceGazeGesture::MOUTH_PUCKER, MacroName::MOUSE_CLICK_LEFT}})
          .WithGestureConfidences({{FaceGazeGesture::MOUTH_PUCKER, 50}}));
  event_handler().ClearEvents();

  // Move mouth right to trigger mouse press event.
  utils()->ProcessFaceLandmarkerResult(MockFaceLandmarkerResult().WithGesture(
      MediapipeGesture::MOUTH_PUCKER, 60));
  std::vector<ui::MouseEvent> mouse_events = event_handler().mouse_events();
  ASSERT_EQ(2u, mouse_events.size());
  ASSERT_EQ(ui::ET_MOUSE_PRESSED, mouse_events[0].type());
  ASSERT_TRUE(mouse_events[0].IsOnlyLeftMouseButton());
  ASSERT_EQ(gfx::Point(600, 400), mouse_events[0].root_location());
  ASSERT_TRUE(mouse_events[0].IsSynthesized());
  ASSERT_EQ(ui::ET_MOUSE_RELEASED, mouse_events[1].type());
  ASSERT_TRUE(mouse_events[1].IsOnlyLeftMouseButton());
  ASSERT_EQ(gfx::Point(600, 400), mouse_events[1].root_location());
  ASSERT_TRUE(mouse_events[1].IsSynthesized());

  // Release doesn't trigger anything else.
  event_handler().ClearEvents();
  utils()->ProcessFaceLandmarkerResult(MockFaceLandmarkerResult().WithGesture(
      MediapipeGesture::MOUTH_PUCKER, 30));
  mouse_events = event_handler().mouse_events();
  ASSERT_EQ(0u, mouse_events.size());
}

IN_PROC_BROWSER_TEST_F(FaceGazeIntegrationTest,
                       MouseLongPressAndReleaseEvents) {
  utils()->EnableFaceGaze(
      Config()
          .Default()
          .WithGesturesToMacros({{FaceGazeGesture::MOUTH_RIGHT,
                                  MacroName::MOUSE_LONG_CLICK_LEFT}})
          .WithGestureConfidences({{FaceGazeGesture::MOUTH_RIGHT, 30}}));
  event_handler().ClearEvents();

  // Move mouth right to trigger mouse press event.
  utils()->ProcessFaceLandmarkerResult(MockFaceLandmarkerResult().WithGesture(
      MediapipeGesture::MOUTH_RIGHT, 40));
  std::vector<ui::MouseEvent> mouse_events = event_handler().mouse_events();
  ASSERT_EQ(1u, mouse_events.size());
  ASSERT_EQ(ui::ET_MOUSE_PRESSED, mouse_events[0].type());
  ASSERT_TRUE(mouse_events[0].IsOnlyLeftMouseButton());
  ASSERT_EQ(gfx::Point(600, 400), mouse_events[0].root_location());
  ASSERT_TRUE(mouse_events[0].IsSynthesized());

  // Release mouth right to trigger mouse release event.
  event_handler().ClearEvents();
  utils()->ProcessFaceLandmarkerResult(MockFaceLandmarkerResult().WithGesture(
      MediapipeGesture::MOUTH_RIGHT, 20));
  mouse_events = event_handler().mouse_events();
  ASSERT_EQ(1u, mouse_events.size());
  ASSERT_EQ(ui::ET_MOUSE_RELEASED, mouse_events[0].type());
  ASSERT_TRUE(mouse_events[0].IsOnlyLeftMouseButton());
  ASSERT_EQ(gfx::Point(600, 400), mouse_events[0].root_location());
  ASSERT_TRUE(mouse_events[0].IsSynthesized());
}

}  // namespace ash
