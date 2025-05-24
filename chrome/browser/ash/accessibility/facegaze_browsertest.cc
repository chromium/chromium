// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>

#include "ash/accessibility/accessibility_controller.h"
#include "ash/accessibility/drag_event_rewriter.h"
#include "ash/accessibility/ui/accessibility_confirmation_dialog.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/shell.h"
#include "ash/system/accessibility/accessibility_feature_disable_dialog.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ash/accessibility/accessibility_feature_browsertest.h"
#include "chrome/browser/ash/accessibility/accessibility_manager.h"
#include "chrome/browser/ash/accessibility/accessibility_test_utils.h"
#include "chrome/browser/ash/accessibility/facegaze_bubble_test_helper.h"
#include "chrome/browser/ash/accessibility/facegaze_test_utils.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_test.h"
#include "ui/accessibility/accessibility_features.h"
#include "ui/events/event.h"
#include "ui/events/event_constants.h"
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

// Default forehead location.
constexpr std::pair<double, double> kDefaultForeheadLocation =
    std::make_pair(0.11, 0.21);
// Expected cursor location when the above forehead location is used.
constexpr gfx::Point kDefaultCursorLocation = gfx::Point(360, 560);
// The center of the screen.
constexpr gfx::Point kCenter = gfx::Point(600, 400);

PrefService* GetPrefs() {
  return AccessibilityManager::Get()->profile()->GetPrefs();
}

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
    if (event->source_device_id() != ui::EventDeviceId::ED_UNKNOWN_DEVICE) {
      return;
    }

    ui::EventType type = event->type();
    if (type == ui::EventType::kMousePressed ||
        type == ui::EventType::kMouseReleased ||
        type == ui::EventType::kMouseMoved ||
        type == ui::EventType::kMousewheel ||
        type == ui::EventType::kMouseDragged) {
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

  std::vector<ui::MouseEvent> mouse_events(ui::EventType type) const {
    std::vector<ui::MouseEvent> events;
    for (const auto& event : mouse_events_) {
      if (event.type() == type) {
        events.push_back(event);
      }
    }

    return events;
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
    scoped_feature_list_.InitAndEnableFeature(
        ::features::kAccessibilityFaceGaze);
    InProcessBrowserTest::SetUpCommandLine(command_line);
  }

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    utils_ = std::make_unique<FaceGazeTestUtils>();
    bubble_helper_ = std::make_unique<FaceGazeBubbleTestHelper>();
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
  }

  MockEventHandler& event_handler() { return event_handler_; }
  FaceGazeTestUtils* utils() { return utils_.get(); }
  FaceGazeBubbleTestHelper* bubble_helper() { return bubble_helper_.get(); }

 private:
  std::unique_ptr<FaceGazeTestUtils> utils_;
  std::unique_ptr<FaceGazeBubbleTestHelper> bubble_helper_;
  MockEventHandler event_handler_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(FaceGazeIntegrationTest, UpdateCursorLocation) {
  utils()->EnableFaceGaze(Config().Default());
  event_handler().ClearEvents();

  utils()->ProcessFaceLandmarkerResult(
      MockFaceLandmarkerResult().WithNormalizedForeheadLocation(
          kDefaultForeheadLocation));
  utils()->TriggerMouseControllerInterval();
  utils()->AssertCursorAt(kDefaultCursorLocation);

  // We expect two mouse move events to be received because the FaceGaze
  // extension calls two APIs to update the cursor position.
  // EventGenerator generates a non-synthetic event. 'setCursorPosition'
  // generates a synthetic event.
  const std::vector<ui::MouseEvent> mouse_events =
      event_handler().mouse_events();
  ASSERT_EQ(2u, mouse_events.size());
  ASSERT_EQ(ui::EventType::kMouseMoved, mouse_events[0].type());
  ASSERT_EQ(kDefaultCursorLocation, mouse_events[0].root_location());
  ASSERT_FALSE(mouse_events[0].IsSynthesized());
  ASSERT_EQ(ui::EventType::kMouseMoved, mouse_events[1].type());
  ASSERT_EQ(kDefaultCursorLocation, mouse_events[1].root_location());
  ASSERT_TRUE(mouse_events[1].IsSynthesized());
}

IN_PROC_BROWSER_TEST_F(FaceGazeIntegrationTest, ResetCursor) {
  const base::flat_map<FaceGazeGesture, MacroName> gestures_to_macros = {
      {FaceGazeGesture::JAW_OPEN, MacroName::RESET_CURSOR}};
  const base::flat_map<FaceGazeGesture, int> gestures_to_confidences = {
      {FaceGazeGesture::JAW_OPEN, 70}};
  utils()->EnableFaceGaze(Config().Default().WithBindings(
      gestures_to_macros, gestures_to_confidences));

  // Move cursor.
  utils()->ProcessFaceLandmarkerResult(
      MockFaceLandmarkerResult().WithNormalizedForeheadLocation(
          kDefaultForeheadLocation));
  utils()->TriggerMouseControllerInterval();
  utils()->AssertCursorAt(kDefaultCursorLocation);

  event_handler().ClearEvents();

  // Reset the cursor to the center of the screen using a gesture.
  utils()->ProcessFaceLandmarkerResult(
      MockFaceLandmarkerResult().WithGesture(MediapipeGesture::JAW_OPEN, 90));
  utils()->AssertCursorAt(kCenter);

  // We expect one mouse move event to be received because the FaceGaze
  // extension only calls one API to reset the cursor position, which sends a
  // synthesized event.
  const std::vector<ui::MouseEvent> mouse_events =
      event_handler().mouse_events();
  ASSERT_EQ(1u, mouse_events.size());
  ASSERT_EQ(ui::EventType::kMouseMoved, mouse_events[0].type());
  ASSERT_EQ(kCenter, mouse_events[0].root_location());
  ASSERT_TRUE(mouse_events[0].IsSynthesized());
}

IN_PROC_BROWSER_TEST_F(FaceGazeIntegrationTest,
                       IgnoreGesturesWithLowConfidence) {
  const base::flat_map<FaceGazeGesture, MacroName> gestures_to_macros = {
      {FaceGazeGesture::JAW_OPEN, MacroName::RESET_CURSOR}};
  const base::flat_map<FaceGazeGesture, int> gestures_to_confidences = {
      {FaceGazeGesture::JAW_OPEN, 100}};
  utils()->EnableFaceGaze(Config().Default().WithBindings(
      gestures_to_macros, gestures_to_confidences));

  // Move cursor.
  utils()->ProcessFaceLandmarkerResult(
      MockFaceLandmarkerResult().WithNormalizedForeheadLocation(
          kDefaultForeheadLocation));
  utils()->TriggerMouseControllerInterval();
  utils()->AssertCursorAt(kDefaultCursorLocation);

  // Attempt to reset the cursor to the center of the screen using a gesture.
  // This gesture will be ignored because the gesture doesn't have high enough
  // confidence.
  event_handler().ClearEvents();
  utils()->ProcessFaceLandmarkerResult(
      MockFaceLandmarkerResult().WithGesture(MediapipeGesture::JAW_OPEN, 90));
  utils()->AssertCursorAt(kDefaultCursorLocation);
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
            std::make_pair(0.1 + px * i, 0.2 + py * i)));
    utils()->TriggerMouseControllerInterval();
    utils()->AssertCursorAt(gfx::Point(600 - i, 400 + i));
  }
}

IN_PROC_BROWSER_TEST_F(FaceGazeIntegrationTest, SpaceKeyEvents) {
  const base::flat_map<FaceGazeGesture, MacroName> gestures_to_macros = {
      {FaceGazeGesture::MOUTH_LEFT, MacroName::KEY_PRESS_SPACE}};
  const base::flat_map<FaceGazeGesture, int> gestures_to_confidences = {
      {FaceGazeGesture::MOUTH_LEFT, 70}};
  utils()->EnableFaceGaze(Config().Default().WithBindings(
      gestures_to_macros, gestures_to_confidences));

  // Open jaw for space key press.
  event_handler().ClearEvents();
  utils()->ProcessFaceLandmarkerResult(
      MockFaceLandmarkerResult().WithGesture(MediapipeGesture::MOUTH_LEFT, 90));
  ASSERT_EQ(0u, event_handler().mouse_events().size());
  std::vector<ui::KeyEvent> key_events = event_handler().key_events();
  ASSERT_EQ(1u, key_events.size());
  ASSERT_EQ(ui::KeyboardCode::VKEY_SPACE, key_events[0].key_code());
  ASSERT_EQ(ui::EventType::kKeyPressed, key_events[0].type());

  // Release gesture for space key release.
  utils()->ProcessFaceLandmarkerResult(
      MockFaceLandmarkerResult().WithGesture(MediapipeGesture::MOUTH_LEFT, 10));
  ASSERT_EQ(0u, event_handler().mouse_events().size());
  key_events = event_handler().key_events();
  ASSERT_EQ(2u, event_handler().key_events().size());
  ASSERT_EQ(ui::KeyboardCode::VKEY_SPACE, key_events[1].key_code());
  ASSERT_EQ(ui::EventType::kKeyReleased, key_events[1].type());
}

// The BrowsDown gesture is special because it is the combination of two
// separate facial gestures (BROW_DOWN_LEFT and BROW_DOWN_RIGHT). This test
// ensures that the associated action is performed if either of the gestures is
// detected.
IN_PROC_BROWSER_TEST_F(FaceGazeIntegrationTest, BrowsDownGesture) {
  const base::flat_map<FaceGazeGesture, MacroName> gestures_to_macros = {
      {FaceGazeGesture::BROWS_DOWN, MacroName::RESET_CURSOR}};
  const base::flat_map<FaceGazeGesture, int> gestures_to_confidences = {
      {FaceGazeGesture::BROWS_DOWN, 40}};
  utils()->EnableFaceGaze(
      Config()
          .Default()
          .WithCursorLocation(gfx::Point(0, 0))
          .WithBindings(gestures_to_macros, gestures_to_confidences)
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
  utils()->AssertCursorAt(kCenter);
  AssertLatestMouseEvent(1, ui::EventType::kMouseMoved, kCenter);

  // Reset the mouse cursor away from the center.
  utils()->MoveMouseTo(gfx::Point(0, 0));
  utils()->AssertCursorAt(gfx::Point(0, 0));

  // If BROW_DOWN_RIGHT is recognized, then perform the action.
  event_handler().ClearEvents();
  utils()->ProcessFaceLandmarkerResult(
      MockFaceLandmarkerResult()
          .WithGesture(MediapipeGesture::BROW_DOWN_LEFT, 30)
          .WithGesture(MediapipeGesture::BROW_DOWN_RIGHT, 50));
  utils()->AssertCursorAt(kCenter);
  AssertLatestMouseEvent(1, ui::EventType::kMouseMoved, kCenter);

  // Reset the mouse cursor away from the center.
  utils()->MoveMouseTo(gfx::Point(0, 0));
  utils()->AssertCursorAt(gfx::Point(0, 0));

  // If both of the gestures are recognized, then perform the action.
  event_handler().ClearEvents();
  utils()->ProcessFaceLandmarkerResult(
      MockFaceLandmarkerResult()
          .WithGesture(MediapipeGesture::BROW_DOWN_LEFT, 50)
          .WithGesture(MediapipeGesture::BROW_DOWN_RIGHT, 50));
  utils()->AssertCursorAt(kCenter);
  AssertLatestMouseEvent(1, ui::EventType::kMouseMoved, kCenter);
}

IN_PROC_BROWSER_TEST_F(FaceGazeIntegrationTest, MousePressAndReleaseEvents) {
  const base::flat_map<FaceGazeGesture, MacroName> gestures_to_macros = {
      {FaceGazeGesture::MOUTH_PUCKER, MacroName::MOUSE_CLICK_LEFT}};
  const base::flat_map<FaceGazeGesture, int> gestures_to_confidences = {
      {FaceGazeGesture::MOUTH_PUCKER, 50}};
  utils()->EnableFaceGaze(Config().Default().WithBindings(
      gestures_to_macros, gestures_to_confidences));
  event_handler().ClearEvents();

  // Move mouth right to trigger mouse press event.
  utils()->ProcessFaceLandmarkerResult(MockFaceLandmarkerResult().WithGesture(
      MediapipeGesture::MOUTH_PUCKER, 60));
  auto press_events =
      event_handler().mouse_events(ui::EventType::kMousePressed);
  auto release_events =
      event_handler().mouse_events(ui::EventType::kMouseReleased);
  ASSERT_EQ(1u, press_events.size());
  ASSERT_EQ(1u, release_events.size());
  ASSERT_TRUE(press_events.back().IsOnlyLeftMouseButton());
  ASSERT_EQ(kCenter, press_events[0].root_location());
  ASSERT_TRUE(release_events.back().IsOnlyLeftMouseButton());
  ASSERT_EQ(kCenter, release_events[0].root_location());

  // Release doesn't trigger anything else.
  event_handler().ClearEvents();
  utils()->ProcessFaceLandmarkerResult(MockFaceLandmarkerResult().WithGesture(
      MediapipeGesture::MOUTH_PUCKER, 30));
  ASSERT_EQ(0u, event_handler().mouse_events().size());
}

IN_PROC_BROWSER_TEST_F(FaceGazeIntegrationTest, MouseLongClick) {
  const base::flat_map<FaceGazeGesture, MacroName> gestures_to_macros = {
      {FaceGazeGesture::MOUTH_RIGHT, MacroName::MOUSE_LONG_CLICK_LEFT}};
  const base::flat_map<FaceGazeGesture, int> gestures_to_confidences = {
      {FaceGazeGesture::MOUTH_RIGHT, 30}};
  utils()->EnableFaceGaze(
      Config()
          .Default()
          .WithBindings(gestures_to_macros, gestures_to_confidences)
          .WithGestureRepeatDelayMs(0));
  event_handler().ClearEvents();

  auto* drag_event_rewriter = ash::Shell::Get()
                                  ->accessibility_controller()
                                  ->GetDragEventRewriterForTest();
  ASSERT_NE(drag_event_rewriter, nullptr);
  ASSERT_FALSE(drag_event_rewriter->IsEnabled());

  // Move mouth right to trigger mouse press event.
  utils()->ProcessFaceLandmarkerResult(MockFaceLandmarkerResult().WithGesture(
      MediapipeGesture::MOUTH_RIGHT, 40));
  std::vector<ui::MouseEvent> mouse_events =
      event_handler().mouse_events(ui::EventType::kMousePressed);
  ASSERT_EQ(1u, mouse_events.size());
  ASSERT_EQ(ui::EventType::kMousePressed, mouse_events.back().type());
  ASSERT_TRUE(mouse_events.back().IsOnlyLeftMouseButton());
  ASSERT_EQ(kCenter, mouse_events.back().root_location());
  ASSERT_FALSE(mouse_events.back().IsSynthesized());
  ASSERT_TRUE(drag_event_rewriter->IsEnabled());

  // Move forehead to trigger a kMouseDragged event.
  event_handler().ClearEvents();
  utils()->ProcessFaceLandmarkerResult(
      MockFaceLandmarkerResult().WithNormalizedForeheadLocation(
          std::make_pair(0.9, 0.9)));
  utils()->TriggerMouseControllerInterval();
  mouse_events = event_handler().mouse_events(ui::EventType::kMouseDragged);
  ASSERT_EQ(1u, mouse_events.size());
  ASSERT_EQ(ui::EventType::kMouseDragged, mouse_events.back().type());
  ASSERT_TRUE(mouse_events.back().IsOnlyLeftMouseButton());
  ASSERT_NE(kCenter, mouse_events.back().root_location());
  ASSERT_FALSE(mouse_events.back().IsSynthesized());
  ASSERT_TRUE(drag_event_rewriter->IsEnabled());

  // Move mouth right again to trigger mouse release event.
  event_handler().ClearEvents();
  utils()->ProcessFaceLandmarkerResult(MockFaceLandmarkerResult().WithGesture(
      MediapipeGesture::MOUTH_RIGHT, 40));
  mouse_events = event_handler().mouse_events(ui::EventType::kMouseReleased);
  ASSERT_EQ(1u, mouse_events.size());
  ASSERT_EQ(ui::EventType::kMouseReleased, mouse_events.back().type());
  ASSERT_TRUE(mouse_events.back().IsOnlyLeftMouseButton());
  ASSERT_NE(kCenter, mouse_events.back().root_location());
  ASSERT_FALSE(mouse_events.back().IsSynthesized());
  ASSERT_FALSE(drag_event_rewriter->IsEnabled());
}

// TODO(crbug.com/367758998): Re-enable this test.
IN_PROC_BROWSER_TEST_F(FaceGazeIntegrationTest, DISABLED_PerformanceHistogram) {
  const base::flat_map<FaceGazeGesture, MacroName> gestures_to_macros = {
      {FaceGazeGesture::MOUTH_PUCKER, MacroName::MOUSE_CLICK_LEFT}};
  const base::flat_map<FaceGazeGesture, int> gestures_to_confidences = {
      {FaceGazeGesture::MOUTH_PUCKER, 50}};
  utils()->EnableFaceGaze(Config().Default().WithBindings(
      gestures_to_macros, gestures_to_confidences));

  HistogramWaiter waiter("Accessibility.FaceGaze.AverageFaceLandmarkerLatency");
  for (int i = 0; i < 100; ++i) {
    utils()->ProcessFaceLandmarkerResult(
        MockFaceLandmarkerResult().WithLatency(i));
  }

  waiter.Wait();
}

IN_PROC_BROWSER_TEST_F(FaceGazeIntegrationTest, OpenSettingsPage) {
  const base::flat_map<FaceGazeGesture, MacroName> gestures_to_macros = {
      {FaceGazeGesture::MOUTH_RIGHT, MacroName::OPEN_FACEGAZE_SETTINGS}};
  const base::flat_map<FaceGazeGesture, int> gestures_to_confidences = {
      {FaceGazeGesture::MOUTH_RIGHT, 30}};
  utils()->EnableFaceGaze(Config().Default().WithBindings(
      gestures_to_macros, gestures_to_confidences));

  base::RunLoop waiter;
  AccessibilityManager::Get()->SetOpenSettingsSubpageObserverForTest(
      base::BindLambdaForTesting([&waiter]() { waiter.Quit(); }));

  // Move mouth right to open the FaceGaze settings page.
  utils()->ProcessFaceLandmarkerResult(MockFaceLandmarkerResult().WithGesture(
      MediapipeGesture::MOUTH_RIGHT, 40));
  waiter.Run();
}

IN_PROC_BROWSER_TEST_F(FaceGazeIntegrationTest, ToggleVirtualKeyboard) {
  const base::flat_map<FaceGazeGesture, MacroName> gestures_to_macros = {
      {FaceGazeGesture::JAW_OPEN, MacroName::TOGGLE_VIRTUAL_KEYBOARD}};
  const base::flat_map<FaceGazeGesture, int> gestures_to_confidences = {
      {FaceGazeGesture::JAW_OPEN, 30}};
  utils()->EnableFaceGaze(Config().Default().WithBindings(
      gestures_to_macros, gestures_to_confidences));

  base::RunLoop waiter;
  ash::Shell::Get()
      ->accessibility_controller()
      ->SetVirtualKeyboardVisibleCallbackForTesting(
          base::BindLambdaForTesting([&waiter]() { waiter.Quit(); }));

  // Open jaw to toggle the virtual keyboard.
  utils()->ProcessFaceLandmarkerResult(
      MockFaceLandmarkerResult().WithGesture(MediapipeGesture::JAW_OPEN, 40));
  waiter.Run();
}

IN_PROC_BROWSER_TEST_F(FaceGazeIntegrationTest, DoubleClick) {
  const base::flat_map<FaceGazeGesture, MacroName> gestures_to_macros = {
      {FaceGazeGesture::MOUTH_FUNNEL, MacroName::MOUSE_CLICK_LEFT_DOUBLE}};
  const base::flat_map<FaceGazeGesture, int> gestures_to_confidences = {
      {FaceGazeGesture::MOUTH_FUNNEL, 50}};
  utils()->EnableFaceGaze(Config().Default().WithBindings(
      gestures_to_macros, gestures_to_confidences));
  event_handler().ClearEvents();

  // Mouth funnel to trigger double click event.
  utils()->ProcessFaceLandmarkerResult(MockFaceLandmarkerResult().WithGesture(
      MediapipeGesture::MOUTH_FUNNEL, 60));
  auto press_events =
      event_handler().mouse_events(ui::EventType::kMousePressed);
  auto release_events =
      event_handler().mouse_events(ui::EventType::kMouseReleased);

  ASSERT_EQ(1u, press_events.size());
  ASSERT_EQ(1u, release_events.size());
  const auto& press_event = press_events.back();
  const auto& release_event = release_events.back();

  ASSERT_TRUE(press_event.IsOnlyLeftMouseButton());
  ASSERT_EQ(kCenter, press_event.root_location());
  // Assert that the press event is for a double click.
  ASSERT_TRUE(ui::EF_IS_DOUBLE_CLICK & press_event.flags());

  ASSERT_TRUE(release_event.IsOnlyLeftMouseButton());
  ASSERT_EQ(kCenter, release_event.root_location());
  // Assert that the release event is for a double click.
  ASSERT_TRUE(ui::EF_IS_DOUBLE_CLICK & release_event.flags());

  // Release doesn't trigger anything else.
  event_handler().ClearEvents();
  utils()->ProcessFaceLandmarkerResult(MockFaceLandmarkerResult().WithGesture(
      MediapipeGesture::MOUTH_FUNNEL, 30));
  ASSERT_EQ(0u, event_handler().mouse_events().size());
}

IN_PROC_BROWSER_TEST_F(FaceGazeIntegrationTest, TripleClick) {
  const base::flat_map<FaceGazeGesture, MacroName> gestures_to_macros = {
      {FaceGazeGesture::MOUTH_FUNNEL, MacroName::MOUSE_CLICK_LEFT_TRIPLE}};
  const base::flat_map<FaceGazeGesture, int> gestures_to_confidences = {
      {FaceGazeGesture::MOUTH_FUNNEL, 50}};
  utils()->EnableFaceGaze(Config().Default().WithBindings(
      gestures_to_macros, gestures_to_confidences));
  event_handler().ClearEvents();

  // Mouth funnel to trigger triple click event.
  utils()->ProcessFaceLandmarkerResult(MockFaceLandmarkerResult().WithGesture(
      MediapipeGesture::MOUTH_FUNNEL, 60));
  auto press_events =
      event_handler().mouse_events(ui::EventType::kMousePressed);
  auto release_events =
      event_handler().mouse_events(ui::EventType::kMouseReleased);

  ASSERT_EQ(1u, press_events.size());
  ASSERT_EQ(1u, release_events.size());
  const auto& press_event = press_events.back();
  const auto& release_event = release_events.back();

  ASSERT_TRUE(press_event.IsOnlyLeftMouseButton());
  ASSERT_EQ(kCenter, press_event.root_location());
  // Assert that the press event is for a triple click.
  ASSERT_TRUE(ui::EF_IS_TRIPLE_CLICK & press_event.flags());

  ASSERT_TRUE(release_event.IsOnlyLeftMouseButton());
  ASSERT_EQ(kCenter, release_event.root_location());
  // Assert that the release event is for a triple click.
  ASSERT_TRUE(ui::EF_IS_TRIPLE_CLICK & release_event.flags());

  // Release doesn't trigger anything else.
  event_handler().ClearEvents();
  utils()->ProcessFaceLandmarkerResult(MockFaceLandmarkerResult().WithGesture(
      MediapipeGesture::MOUTH_FUNNEL, 30));
  ASSERT_EQ(0u, event_handler().mouse_events().size());
}

IN_PROC_BROWSER_TEST_F(FaceGazeIntegrationTest, AcceptDialog) {
  auto* controller = ash::Shell::Get()->accessibility_controller();
  auto* prefs = GetPrefs();

  base::RunLoop dialog_waiter;
  controller->AddShowConfirmationDialogCallbackForTesting(
      base::BindLambdaForTesting([&dialog_waiter]() { dialog_waiter.Quit(); }));
  // Enabling FaceGaze should show the confirmation dialog.
  utils()->EnableFaceGaze(Config().Default().WithDialogAccepted(false));
  dialog_waiter.Run();
  ASSERT_TRUE(prefs->GetBoolean(prefs::kAccessibilityFaceGazeEnabled));
  ASSERT_FALSE(prefs->GetBoolean(
      prefs::kAccessibilityFaceGazeAcceleratorDialogHasBeenAccepted));
  ASSERT_NE(nullptr, controller->GetConfirmationDialogForTest());

  base::RunLoop settings_waiter;
  AccessibilityManager::Get()->SetOpenSettingsSubpageObserverForTest(
      base::BindLambdaForTesting(
          [&settings_waiter]() { settings_waiter.Quit(); }));
  // Accepting the dialog should initialize the FaceLandmarker and open the
  // settings page.
  controller->GetConfirmationDialogForTest()->Accept();
  utils()->WaitForFaceLandmarker();
  settings_waiter.Run();
  ASSERT_TRUE(prefs->GetBoolean(prefs::kAccessibilityFaceGazeEnabled));
  // Verify that the dialog accepted pref is now true.
  ASSERT_TRUE(prefs->GetBoolean(
      prefs::kAccessibilityFaceGazeAcceleratorDialogHasBeenAccepted));
}

IN_PROC_BROWSER_TEST_F(FaceGazeIntegrationTest, CancelDialog) {
  auto* controller = ash::Shell::Get()->accessibility_controller();
  auto* prefs = GetPrefs();

  base::RunLoop dialog_waiter;
  controller->AddShowConfirmationDialogCallbackForTesting(
      base::BindLambdaForTesting([&dialog_waiter]() { dialog_waiter.Quit(); }));
  // Enabling FaceGaze should show the confirmation dialog.
  utils()->EnableFaceGaze(Config().Default().WithDialogAccepted(false));
  dialog_waiter.Run();
  ASSERT_TRUE(prefs->GetBoolean(prefs::kAccessibilityFaceGazeEnabled));
  ASSERT_FALSE(prefs->GetBoolean(
      prefs::kAccessibilityFaceGazeAcceleratorDialogHasBeenAccepted));
  ASSERT_NE(nullptr, controller->GetConfirmationDialogForTest());

  base::RunLoop pref_waiter;
  PrefChangeRegistrar change_observer;
  change_observer.Init(prefs);
  change_observer.Add(prefs::kAccessibilityFaceGazeEnabled,
                      pref_waiter.QuitClosure());

  // Canceling the dialog should turn off FaceGaze.
  controller->GetConfirmationDialogForTest()->Cancel();
  pref_waiter.Run();

  ASSERT_FALSE(prefs->GetBoolean(prefs::kAccessibilityFaceGazeEnabled));
  // Verify that the dialog accepted pref is still false.
  ASSERT_FALSE(prefs->GetBoolean(
      prefs::kAccessibilityFaceGazeAcceleratorDialogHasBeenAccepted));
}

IN_PROC_BROWSER_TEST_F(FaceGazeIntegrationTest, ScrollMode) {
  const base::flat_map<FaceGazeGesture, MacroName> gestures_to_macros = {
      {FaceGazeGesture::JAW_LEFT, MacroName::TOGGLE_SCROLL_MODE}};
  const base::flat_map<FaceGazeGesture, int> gestures_to_confidences = {
      {FaceGazeGesture::JAW_LEFT, 30}};
  utils()->EnableFaceGaze(
      Config()
          .Default()
          .WithBindings(gestures_to_macros, gestures_to_confidences)
          // Ensure speeds are high so that head movements exceed the scroll
          // mode movement threshold.
          .WithCursorSpeeds({/*up=*/5, /*down=*/5, /*left=*/5, /*right=*/5})
          .WithGestureRepeatDelayMs(0));

  // Move jaw left to enter scroll mode.
  utils()->ProcessFaceLandmarkerResult(
      MockFaceLandmarkerResult().WithGesture(MediapipeGesture::JAW_LEFT, 40));
  utils()->AssertScrollMode(true);

  event_handler().ClearEvents();
  utils()->ProcessFaceLandmarkerResult(
      MockFaceLandmarkerResult()
          .WithNormalizedForeheadLocation(std::make_pair(0.9, 0.9))
          .WithGesture(MediapipeGesture::JAW_LEFT, 0));
  utils()->TriggerMouseControllerInterval();

  // Head movement should cause one scroll event to be sent.
  ASSERT_EQ(1u,
            event_handler().mouse_events(ui::EventType::kMousewheel).size());
  // No mouse movement events should be sent.
  ASSERT_EQ(0u,
            event_handler().mouse_events(ui::EventType::kMouseMoved).size());

  // Move jaw left again to exit scroll mode.
  utils()->ProcessFaceLandmarkerResult(
      MockFaceLandmarkerResult().WithGesture(MediapipeGesture::JAW_LEFT, 40));
  utils()->AssertScrollMode(false);
}

IN_PROC_BROWSER_TEST_F(FaceGazeIntegrationTest, DefaultBehavior) {
  utils()->EnableFaceGaze(Config().Default());
  // Default gesture-to-macro and gesture-to-confidence mappings should be
  // installed if we didn't specify them.
  const auto& gestures_to_macros =
      GetPrefs()->GetDict(prefs::kAccessibilityFaceGazeGesturesToMacros);
  const auto& gestures_to_confidences =
      GetPrefs()->GetDict(prefs::kAccessibilityFaceGazeGesturesToConfidence);
  ASSERT_EQ(gestures_to_macros.size(), 2u);
  ASSERT_EQ(gestures_to_confidences.size(), 2u);
  ASSERT_EQ(/* MOUSE_CLICK_LEFT */ 35,
            gestures_to_macros.FindInt(
                FaceGazeTestUtils::ToString(FaceGazeGesture::MOUTH_SMILE)));
  ASSERT_EQ(/* SCROLL */ 50,
            gestures_to_macros.FindInt(
                FaceGazeTestUtils::ToString(FaceGazeGesture::JAW_OPEN)));
  ASSERT_EQ(60, gestures_to_confidences.FindInt(
                    FaceGazeTestUtils::ToString(FaceGazeGesture::MOUTH_SMILE)));
  ASSERT_EQ(60, gestures_to_confidences.FindInt(
                    FaceGazeTestUtils::ToString(FaceGazeGesture::JAW_OPEN)));
}

IN_PROC_BROWSER_TEST_F(FaceGazeIntegrationTest, DisableDialogAccept) {
  auto* controller = ash::Shell::Get()->accessibility_controller();
  auto* prefs = GetPrefs();

  base::RunLoop dialog_waiter;
  controller->AddFeatureDisableDialogCallbackForTesting(
      base::BindLambdaForTesting([&dialog_waiter]() { dialog_waiter.Quit(); }));

  // Enabling FaceGaze should not show the feature disable dialog.
  utils()->EnableFaceGaze(Config().Default());
  ASSERT_TRUE(prefs->GetBoolean(prefs::kAccessibilityFaceGazeEnabled));
  ASSERT_EQ(nullptr, controller->GetFeatureDisableDialogForTest());

  // Showing the feature disable dialog should leave the enabled pref unchanged.
  controller->RequestDisableFaceGaze();
  dialog_waiter.Run();
  ASSERT_TRUE(prefs->GetBoolean(prefs::kAccessibilityFaceGazeEnabled));
  ASSERT_NE(nullptr, controller->GetFeatureDisableDialogForTest());

  base::RunLoop pref_waiter;
  PrefChangeRegistrar change_observer;
  change_observer.Init(prefs);
  change_observer.Add(prefs::kAccessibilityFaceGazeEnabled,
                      pref_waiter.QuitClosure());

  // Accepting the dialog should turn off FaceGaze.
  controller->GetFeatureDisableDialogForTest()->Accept();
  pref_waiter.Run();
  ASSERT_FALSE(prefs->GetBoolean(prefs::kAccessibilityFaceGazeEnabled));
}

IN_PROC_BROWSER_TEST_F(FaceGazeIntegrationTest, DisableDialogCancel) {
  auto* controller = ash::Shell::Get()->accessibility_controller();
  auto* prefs = GetPrefs();

  base::RunLoop dialog_waiter;
  controller->AddFeatureDisableDialogCallbackForTesting(
      base::BindLambdaForTesting([&dialog_waiter]() { dialog_waiter.Quit(); }));

  // Enabling FaceGaze should not show the feature disable dialog.
  utils()->EnableFaceGaze(Config().Default());
  ASSERT_TRUE(prefs->GetBoolean(prefs::kAccessibilityFaceGazeEnabled));
  ASSERT_EQ(nullptr, controller->GetFeatureDisableDialogForTest());

  // Showing the feature disable dialog should leave the enabled pref unchanged.
  controller->RequestDisableFaceGaze();
  dialog_waiter.Run();
  ASSERT_TRUE(prefs->GetBoolean(prefs::kAccessibilityFaceGazeEnabled));
  ASSERT_NE(nullptr, controller->GetFeatureDisableDialogForTest());

  base::RunLoop pref_waiter;
  PrefChangeRegistrar change_observer;
  change_observer.Init(prefs);
  change_observer.Add(prefs::kAccessibilityFaceGazeEnabledSentinel,
                      pref_waiter.QuitClosure());

  // Cancelling the dialog should leave FaceGaze on.
  controller->GetFeatureDisableDialogForTest()->Cancel();
  ASSERT_TRUE(prefs->GetBoolean(prefs::kAccessibilityFaceGazeEnabled));
}

// TODO(crbug.com/383757982): Add test API for .WithCursorControlEnabled() and
// .WithActionsEnabled() and update tests accordingly.
IN_PROC_BROWSER_TEST_F(FaceGazeIntegrationTest, EnableCursorControlNoDialog) {
  auto* controller = ash::Shell::Get()->accessibility_controller();
  auto* prefs = GetPrefs();

  base::RunLoop dialog_waiter;
  controller->AddFeatureDisableDialogCallbackForTesting(
      base::BindLambdaForTesting([&dialog_waiter]() { dialog_waiter.Quit(); }));

  // Enabling FaceGaze should not show the feature disable dialog.
  utils()->EnableFaceGaze(Config().Default());
  ASSERT_TRUE(prefs->GetBoolean(prefs::kAccessibilityFaceGazeEnabled));
  ASSERT_EQ(nullptr, controller->GetFeatureDisableDialogForTest());

  // Setting sentinel value to true should not show the feature disable dialog.
  prefs->SetBoolean(prefs::kAccessibilityFaceGazeCursorControlEnabledSentinel,
                    true);
  ASSERT_TRUE(
      prefs->GetBoolean(prefs::kAccessibilityFaceGazeCursorControlEnabled));
  ASSERT_EQ(nullptr, controller->GetFeatureDisableDialogForTest());
}

IN_PROC_BROWSER_TEST_F(FaceGazeIntegrationTest,
                       DisableCursorControlDialogAccept) {
  auto* controller = ash::Shell::Get()->accessibility_controller();
  auto* prefs = GetPrefs();

  base::RunLoop dialog_waiter;
  controller->AddFeatureDisableDialogCallbackForTesting(
      base::BindLambdaForTesting([&dialog_waiter]() { dialog_waiter.Quit(); }));

  // Enabling FaceGaze should not show the feature disable dialog.
  utils()->EnableFaceGaze(Config().Default());
  ASSERT_TRUE(prefs->GetBoolean(prefs::kAccessibilityFaceGazeEnabled));
  ASSERT_EQ(nullptr, controller->GetFeatureDisableDialogForTest());
  prefs->SetBoolean(prefs::kAccessibilityFaceGazeCursorControlEnabled, true);
  ASSERT_TRUE(prefs->GetBoolean(
      prefs::kAccessibilityFaceGazeCursorControlEnabledSentinel));

  // Setting sentinel value to false should show the feature disable dialog and
  // leave the behavior pref unchanged.
  prefs->SetBoolean(prefs::kAccessibilityFaceGazeCursorControlEnabledSentinel,
                    false);
  dialog_waiter.Run();
  ASSERT_TRUE(
      prefs->GetBoolean(prefs::kAccessibilityFaceGazeCursorControlEnabled));
  ASSERT_FALSE(prefs->GetBoolean(
      prefs::kAccessibilityFaceGazeCursorControlEnabledSentinel));
  ASSERT_NE(nullptr, controller->GetFeatureDisableDialogForTest());

  base::RunLoop pref_waiter;
  PrefChangeRegistrar change_observer;
  change_observer.Init(prefs);
  change_observer.Add(prefs::kAccessibilityFaceGazeCursorControlEnabled,
                      pref_waiter.QuitClosure());

  // Accepting the dialog should turn off FaceGaze cursor control.
  controller->GetFeatureDisableDialogForTest()->Accept();
  pref_waiter.Run();

  // Assert behavior and sentinel prefs are in sync.
  ASSERT_FALSE(
      prefs->GetBoolean(prefs::kAccessibilityFaceGazeCursorControlEnabled));
  ASSERT_FALSE(prefs->GetBoolean(
      prefs::kAccessibilityFaceGazeCursorControlEnabledSentinel));
}

IN_PROC_BROWSER_TEST_F(FaceGazeIntegrationTest,
                       DisableCursorControlDialogCancel) {
  auto* controller = ash::Shell::Get()->accessibility_controller();
  auto* prefs = GetPrefs();

  base::RunLoop dialog_waiter;
  controller->AddFeatureDisableDialogCallbackForTesting(
      base::BindLambdaForTesting([&dialog_waiter]() { dialog_waiter.Quit(); }));

  // Enabling FaceGaze should not show the feature disable dialog.
  utils()->EnableFaceGaze(Config().Default());
  ASSERT_TRUE(prefs->GetBoolean(prefs::kAccessibilityFaceGazeEnabled));
  ASSERT_EQ(nullptr, controller->GetFeatureDisableDialogForTest());
  prefs->SetBoolean(prefs::kAccessibilityFaceGazeCursorControlEnabled, true);
  ASSERT_TRUE(prefs->GetBoolean(
      prefs::kAccessibilityFaceGazeCursorControlEnabledSentinel));

  // Setting sentinel value to false should show the feature disable dialog and
  // leave the behavior pref unchanged.
  prefs->SetBoolean(prefs::kAccessibilityFaceGazeCursorControlEnabledSentinel,
                    false);
  dialog_waiter.Run();
  ASSERT_TRUE(
      prefs->GetBoolean(prefs::kAccessibilityFaceGazeCursorControlEnabled));
  ASSERT_NE(nullptr, controller->GetFeatureDisableDialogForTest());

  base::RunLoop pref_waiter;
  PrefChangeRegistrar change_observer;
  change_observer.Init(prefs);
  change_observer.Add(prefs::kAccessibilityFaceGazeCursorControlEnabledSentinel,
                      pref_waiter.QuitClosure());

  // Cancelling the dialog should leave FaceGaze cursor control on and set the
  // sentinel to true.
  controller->GetFeatureDisableDialogForTest()->Cancel();
  pref_waiter.Run();

  // Assert behavior and sentinel prefs are in sync.
  ASSERT_TRUE(
      prefs->GetBoolean(prefs::kAccessibilityFaceGazeCursorControlEnabled));
  ASSERT_TRUE(prefs->GetBoolean(
      prefs::kAccessibilityFaceGazeCursorControlEnabledSentinel));
}

IN_PROC_BROWSER_TEST_F(FaceGazeIntegrationTest, EnableActionsNoDialog) {
  auto* controller = ash::Shell::Get()->accessibility_controller();
  auto* prefs = GetPrefs();

  base::RunLoop dialog_waiter;
  controller->AddFeatureDisableDialogCallbackForTesting(
      base::BindLambdaForTesting([&dialog_waiter]() { dialog_waiter.Quit(); }));

  // Enabling FaceGaze should not show the feature disable dialog.
  utils()->EnableFaceGaze(Config().Default());
  ASSERT_TRUE(prefs->GetBoolean(prefs::kAccessibilityFaceGazeEnabled));
  ASSERT_EQ(nullptr, controller->GetFeatureDisableDialogForTest());

  // Setting sentinel value to true should not show the feature disable dialog.
  prefs->SetBoolean(prefs::kAccessibilityFaceGazeActionsEnabledSentinel, true);
  ASSERT_TRUE(prefs->GetBoolean(prefs::kAccessibilityFaceGazeActionsEnabled));
  ASSERT_EQ(nullptr, controller->GetFeatureDisableDialogForTest());
}

IN_PROC_BROWSER_TEST_F(FaceGazeIntegrationTest, DisableActionsDialogAccept) {
  auto* controller = ash::Shell::Get()->accessibility_controller();
  auto* prefs = GetPrefs();

  base::RunLoop dialog_waiter;
  controller->AddFeatureDisableDialogCallbackForTesting(
      base::BindLambdaForTesting([&dialog_waiter]() { dialog_waiter.Quit(); }));

  // Setting sentinel value to true should not show the feature disable dialog.
  utils()->EnableFaceGaze(Config().Default());
  ASSERT_TRUE(prefs->GetBoolean(prefs::kAccessibilityFaceGazeEnabled));
  ASSERT_EQ(nullptr, controller->GetFeatureDisableDialogForTest());
  prefs->SetBoolean(prefs::kAccessibilityFaceGazeActionsEnabled, true);
  ASSERT_TRUE(
      prefs->GetBoolean(prefs::kAccessibilityFaceGazeActionsEnabledSentinel));

  // Setting sentinel value to false should show the feature disable dialog and
  // leave the behavior pref unchanged.
  prefs->SetBoolean(prefs::kAccessibilityFaceGazeActionsEnabledSentinel, false);
  dialog_waiter.Run();
  ASSERT_TRUE(prefs->GetBoolean(prefs::kAccessibilityFaceGazeActionsEnabled));
  ASSERT_FALSE(
      prefs->GetBoolean(prefs::kAccessibilityFaceGazeActionsEnabledSentinel));
  ASSERT_NE(nullptr, controller->GetFeatureDisableDialogForTest());

  base::RunLoop pref_waiter;
  PrefChangeRegistrar change_observer;
  change_observer.Init(prefs);
  change_observer.Add(prefs::kAccessibilityFaceGazeActionsEnabled,
                      pref_waiter.QuitClosure());

  // Accepting the dialog should turn off FaceGaze actions.
  controller->GetFeatureDisableDialogForTest()->Accept();
  pref_waiter.Run();

  // Assert behavior and sentinel prefs are in sync.
  ASSERT_FALSE(prefs->GetBoolean(prefs::kAccessibilityFaceGazeActionsEnabled));
  ASSERT_FALSE(
      prefs->GetBoolean(prefs::kAccessibilityFaceGazeActionsEnabledSentinel));
}

IN_PROC_BROWSER_TEST_F(FaceGazeIntegrationTest, DisableActionsDialogCancel) {
  auto* controller = ash::Shell::Get()->accessibility_controller();
  auto* prefs = GetPrefs();

  base::RunLoop dialog_waiter;
  controller->AddFeatureDisableDialogCallbackForTesting(
      base::BindLambdaForTesting([&dialog_waiter]() { dialog_waiter.Quit(); }));

  // Setting sentinel value to true should not show the feature disable dialog.
  utils()->EnableFaceGaze(Config().Default());
  ASSERT_TRUE(prefs->GetBoolean(prefs::kAccessibilityFaceGazeEnabled));
  ASSERT_EQ(nullptr, controller->GetFeatureDisableDialogForTest());
  prefs->SetBoolean(prefs::kAccessibilityFaceGazeActionsEnabled, true);
  ASSERT_TRUE(
      prefs->GetBoolean(prefs::kAccessibilityFaceGazeActionsEnabledSentinel));

  // Setting sentinel value to false should show the feature disable dialog and
  // leave the behavior pref unchanged.
  prefs->SetBoolean(prefs::kAccessibilityFaceGazeActionsEnabledSentinel, false);
  dialog_waiter.Run();
  ASSERT_TRUE(prefs->GetBoolean(prefs::kAccessibilityFaceGazeActionsEnabled));
  ASSERT_NE(nullptr, controller->GetFeatureDisableDialogForTest());

  base::RunLoop pref_waiter;
  PrefChangeRegistrar change_observer;
  change_observer.Init(prefs);
  change_observer.Add(prefs::kAccessibilityFaceGazeActionsEnabledSentinel,
                      pref_waiter.QuitClosure());

  // Cancelling the dialog should leave FaceGaze actions on and set the sentinel
  // to true.
  controller->GetFeatureDisableDialogForTest()->Cancel();
  pref_waiter.Run();

  // Assert behavior and sentinel prefs are in sync.
  ASSERT_TRUE(prefs->GetBoolean(prefs::kAccessibilityFaceGazeActionsEnabled));
  ASSERT_TRUE(
      prefs->GetBoolean(prefs::kAccessibilityFaceGazeActionsEnabledSentinel));
}

IN_PROC_BROWSER_TEST_F(FaceGazeIntegrationTest, CloseButton) {
  auto* controller = ash::Shell::Get()->accessibility_controller();
  auto* prefs = GetPrefs();

  base::RunLoop dialog_waiter;
  controller->AddFeatureDisableDialogCallbackForTesting(
      base::BindLambdaForTesting([&dialog_waiter]() { dialog_waiter.Quit(); }));

  // Setup FaceGaze.
  const base::flat_map<FaceGazeGesture, MacroName> gestures_to_macros = {
      {FaceGazeGesture::MOUTH_PUCKER, MacroName::MOUSE_CLICK_LEFT}};
  const base::flat_map<FaceGazeGesture, int> gestures_to_confidences = {
      {FaceGazeGesture::MOUTH_PUCKER, 50}};
  utils()->EnableFaceGaze(Config().Default().WithBindings(
      gestures_to_macros, gestures_to_confidences));

  // Assert initial state.
  ASSERT_TRUE(prefs->GetBoolean(prefs::kAccessibilityFaceGazeEnabled));
  ASSERT_EQ(nullptr, controller->GetFeatureDisableDialogForTest());

  // Move mouse to close button.
  gfx::Point close_button = bubble_helper()->GetCloseButtonCenterPoint();
  utils()->MoveMouseTo(close_button);
  utils()->AssertCursorAt(close_button);
  ASSERT_TRUE(bubble_helper()->IsVisible());

  // Clicking the close button will show the dialog to turn off FaceGaze. Note
  // that the feature remains on until the dialog is accepted.
  utils()->ProcessFaceLandmarkerResult(MockFaceLandmarkerResult().WithGesture(
      MediapipeGesture::MOUTH_PUCKER, 95));
  dialog_waiter.Run();
  ASSERT_TRUE(prefs->GetBoolean(prefs::kAccessibilityFaceGazeEnabled));
  ASSERT_NE(nullptr, controller->GetFeatureDisableDialogForTest());

  base::RunLoop pref_waiter;
  PrefChangeRegistrar change_observer;
  change_observer.Init(prefs);
  change_observer.Add(prefs::kAccessibilityFaceGazeEnabled,
                      pref_waiter.QuitClosure());

  // Accepting the dialog should turn off FaceGaze.
  controller->GetFeatureDisableDialogForTest()->Accept();
  pref_waiter.Run();
}

}  // namespace ash
