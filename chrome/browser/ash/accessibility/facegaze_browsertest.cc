// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>

#include "ash/accessibility/accessibility_controller.h"
#include "ash/accessibility/ui/accessibility_confirmation_dialog.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/shell.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ash/accessibility/accessibility_feature_browsertest.h"
#include "chrome/browser/ash/accessibility/accessibility_manager.h"
#include "chrome/browser/ash/accessibility/accessibility_test_utils.h"
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
    if (!event->IsSynthesized() ||
        event->source_device_id() != ui::EventDeviceId::ED_UNKNOWN_DEVICE) {
      // FaceGaze will only send synthesized events. Since this class is meant
      // to verify events sent by FaceGaze, we can ignore all non-synthesized
      // events.
      return;
    }

    ui::EventType type = event->type();
    if (type == ui::EventType::kMousePressed ||
        type == ui::EventType::kMouseReleased ||
        type == ui::EventType::kMouseMoved ||
        type == ui::EventType::kMousewheel) {
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
      MockFaceLandmarkerResult().WithNormalizedForeheadLocation(
          kDefaultForeheadLocation));
  utils()->TriggerMouseControllerInterval();
  utils()->AssertCursorAt(kDefaultCursorLocation);

  // We expect two mouse move events to be received because the FaceGaze
  // extension calls two APIs to update the cursor position.
  const std::vector<ui::MouseEvent> mouse_events =
      event_handler().mouse_events();
  ASSERT_EQ(2u, mouse_events.size());
  ASSERT_EQ(ui::EventType::kMouseMoved, mouse_events[0].type());
  ASSERT_EQ(kDefaultCursorLocation, mouse_events[0].root_location());
  ASSERT_TRUE(mouse_events[0].IsSynthesized());
  ASSERT_EQ(ui::EventType::kMouseMoved, mouse_events[1].type());
  ASSERT_EQ(kDefaultCursorLocation, mouse_events[1].root_location());
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
  // extension only calls one API to reset the cursor position.
  const std::vector<ui::MouseEvent> mouse_events =
      event_handler().mouse_events();
  ASSERT_EQ(1u, mouse_events.size());
  ASSERT_EQ(ui::EventType::kMouseMoved, mouse_events[0].type());
  ASSERT_EQ(kCenter, mouse_events[0].root_location());
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

IN_PROC_BROWSER_TEST_F(FaceGazeIntegrationTest,
                       MouseLongPressAndReleaseEvents) {
  utils()->EnableFaceGaze(
      Config()
          .Default()
          .WithGesturesToMacros({{FaceGazeGesture::MOUTH_RIGHT,
                                  MacroName::MOUSE_LONG_CLICK_LEFT}})
          .WithGestureConfidences({{FaceGazeGesture::MOUTH_RIGHT, 30}})
          .WithGestureRepeatDelayMs(0));
  event_handler().ClearEvents();

  // Move mouth right to trigger mouse press event.
  utils()->ProcessFaceLandmarkerResult(MockFaceLandmarkerResult().WithGesture(
      MediapipeGesture::MOUTH_RIGHT, 40));
  std::vector<ui::MouseEvent> mouse_events =
      event_handler().mouse_events(ui::EventType::kMousePressed);
  ASSERT_EQ(1u, mouse_events.size());
  ASSERT_EQ(ui::EventType::kMousePressed, mouse_events.back().type());
  ASSERT_TRUE(mouse_events.back().IsOnlyLeftMouseButton());
  ASSERT_EQ(kCenter, mouse_events.back().root_location());
  ASSERT_TRUE(mouse_events.back().IsSynthesized());

  // TODO(b:371199038): Move mouse to trigger drag event.

  // Move mouth right again to trigger mouse release event.
  event_handler().ClearEvents();
  utils()->ProcessFaceLandmarkerResult(MockFaceLandmarkerResult().WithGesture(
      MediapipeGesture::MOUTH_RIGHT, 40));
  mouse_events = event_handler().mouse_events(ui::EventType::kMouseReleased);
  ASSERT_EQ(1u, mouse_events.size());
  ASSERT_EQ(ui::EventType::kMouseReleased, mouse_events.back().type());
  ASSERT_TRUE(mouse_events.back().IsOnlyLeftMouseButton());
  ASSERT_EQ(kCenter, mouse_events.back().root_location());
  ASSERT_TRUE(mouse_events.back().IsSynthesized());
}

IN_PROC_BROWSER_TEST_F(FaceGazeIntegrationTest, PerformanceHistogram) {
  utils()->EnableFaceGaze(
      Config()
          .Default()
          .WithGesturesToMacros(
              {{FaceGazeGesture::MOUTH_PUCKER, MacroName::MOUSE_CLICK_LEFT}})
          .WithGestureConfidences({{FaceGazeGesture::MOUTH_PUCKER, 50}}));

  HistogramWaiter waiter("Accessibility.FaceGaze.AverageFaceLandmarkerLatency");
  for (int i = 0; i < 100; ++i) {
    utils()->ProcessFaceLandmarkerResult(
        MockFaceLandmarkerResult().WithLatency(i));
  }

  waiter.Wait();
}

IN_PROC_BROWSER_TEST_F(FaceGazeIntegrationTest, OpenSettingsPage) {
  utils()->EnableFaceGaze(
      Config()
          .Default()
          .WithGesturesToMacros({{FaceGazeGesture::MOUTH_RIGHT,
                                  MacroName::OPEN_FACEGAZE_SETTINGS}})
          .WithGestureConfidences({{FaceGazeGesture::MOUTH_RIGHT, 30}}));

  base::RunLoop waiter;
  AccessibilityManager::Get()->SetOpenSettingsSubpageObserverForTest(
      base::BindLambdaForTesting([&waiter]() { waiter.Quit(); }));

  // Move mouth right to open the FaceGaze settings page.
  utils()->ProcessFaceLandmarkerResult(MockFaceLandmarkerResult().WithGesture(
      MediapipeGesture::MOUTH_RIGHT, 40));
  waiter.Run();
}

IN_PROC_BROWSER_TEST_F(FaceGazeIntegrationTest, ToggleVirtualKeyboard) {
  utils()->EnableFaceGaze(
      Config()
          .Default()
          .WithGesturesToMacros(
              {{FaceGazeGesture::JAW_OPEN, MacroName::TOGGLE_VIRTUAL_KEYBOARD}})
          .WithGestureConfidences({{FaceGazeGesture::JAW_OPEN, 30}}));

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
  utils()->EnableFaceGaze(
      Config()
          .Default()
          .WithGesturesToMacros({{FaceGazeGesture::MOUTH_FUNNEL,
                                  MacroName::MOUSE_CLICK_LEFT_DOUBLE}})
          .WithGestureConfidences({{FaceGazeGesture::MOUTH_FUNNEL, 50}}));
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
  utils()->EnableFaceGaze(
      Config()
          .Default()
          .WithGesturesToMacros(
              {{FaceGazeGesture::JAW_LEFT, MacroName::TOGGLE_SCROLL_MODE}})
          .WithGestureConfidences({{FaceGazeGesture::JAW_LEFT, 30}})
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
  ASSERT_EQ(/* MOUTH_SMILE */ 35,
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

}  // namespace ash
