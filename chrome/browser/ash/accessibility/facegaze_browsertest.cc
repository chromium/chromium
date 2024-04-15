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
#include "ui/events/event.h"
#include "ui/events/event_handler.h"
#include "ui/events/keycodes/keyboard_codes_posix.h"
#include "ui/events/test/event_generator.h"
#include "ui/events/types/event_type.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/point_f.h"

namespace ash {

using CursorSpeeds = FaceGazeTestUtils::CursorSpeeds;
using MockFaceLandmarkerResult = FaceGazeTestUtils::MockFaceLandmarkerResult;

namespace {

const int kMouseDeviceId = 1;
const char* kDefaultDisplaySize = "1200x800";

// The facial gestures that are supported by FaceGaze. Ensure this enum stays in
// sync with the source of truth in
// ash/webui/common/resources/accessibility/facial_gestures.ts.
enum class FaceGazeGesture {
  BROW_INNER_UP,
  BROWS_DOWN,
  EYE_SQUINT_LEFT,
  EYE_SQUINT_RIGHT,
  EYES_BLINK,
  EYES_LOOK_DOWN,
  EYES_LOOK_LEFT,
  EYES_LOOK_RIGHT,
  EYES_LOOK_UP,
  JAW_OPEN,
  MOUTH_LEFT,
  MOUTH_PUCKER,
  MOUTH_RIGHT,
  MOUTH_SMILE,
  MOUTH_UPPER_UP,
};

std::string ToString(const FaceGazeGesture& gesture) {
  switch (gesture) {
    case FaceGazeGesture::BROW_INNER_UP:
      return "browInnerUp";
    case FaceGazeGesture::BROWS_DOWN:
      return "browsDown";
    case FaceGazeGesture::EYE_SQUINT_LEFT:
      return "eyeSquintLeft";
    case FaceGazeGesture::EYE_SQUINT_RIGHT:
      return "eyeSquintRight";
    case FaceGazeGesture::EYES_BLINK:
      return "eyesBlink";
    case FaceGazeGesture::EYES_LOOK_DOWN:
      return "eyesLookDown";
    case FaceGazeGesture::EYES_LOOK_LEFT:
      return "eyesLookLeft";
    case FaceGazeGesture::EYES_LOOK_RIGHT:
      return "eyesLookRight";
    case FaceGazeGesture::EYES_LOOK_UP:
      return "eyesLookUp";
    case FaceGazeGesture::JAW_OPEN:
      return "jawOpen";
    case FaceGazeGesture::MOUTH_LEFT:
      return "mouthLeft";
    case FaceGazeGesture::MOUTH_PUCKER:
      return "mouthPucker";
    case FaceGazeGesture::MOUTH_RIGHT:
      return "mouthRight";
    case FaceGazeGesture::MOUTH_SMILE:
      return "mouthSmile";
    case FaceGazeGesture::MOUTH_UPPER_UP:
      return "mouthUpperUp";
  }
}

// Facial gestures recognized by Mediapipe. Ensure this enum stays in sync with
// the source of truth in chrome/browser/resources/chromeos/accessibility/\
// accessibility_common/facegaze/gesture_detector.ts.
enum class MediapipeGesture {
  BROW_DOWN_LEFT,
  BROW_DOWN_RIGHT,
  BROW_INNER_UP,
  EYE_BLINK_LEFT,
  EYE_BLINK_RIGHT,
  EYE_LOOK_DOWN_LEFT,
  EYE_LOOK_DOWN_RIGHT,
  EYE_LOOK_IN_LEFT,
  EYE_LOOK_IN_RIGHT,
  EYE_LOOK_OUT_LEFT,
  EYE_LOOK_OUT_RIGHT,
  EYE_LOOK_UP_LEFT,
  EYE_LOOK_UP_RIGHT,
  EYE_SQUINT_LEFT,
  EYE_SQUINT_RIGHT,
  JAW_OPEN,
  MOUTH_LEFT,
  MOUTH_PUCKER,
  MOUTH_RIGHT,
  MOUTH_SMILE_LEFT,
  MOUTH_SMILE_RIGHT,
  MOUTH_UPPER_UP_LEFT,
  MOUTH_UPPER_UP_RIGHT,
};

std::string ToString(const MediapipeGesture& gesture) {
  switch (gesture) {
    case MediapipeGesture::BROW_DOWN_LEFT:
      return "browDownLeft";
    case MediapipeGesture::BROW_DOWN_RIGHT:
      return "browDownRight";
    case MediapipeGesture::BROW_INNER_UP:
      return "browInnerUp";
    case MediapipeGesture::EYE_BLINK_LEFT:
      return "eyeBlinkLeft";
    case MediapipeGesture::EYE_BLINK_RIGHT:
      return "eyeBlinkRight";
    case MediapipeGesture::EYE_LOOK_DOWN_LEFT:
      return "eyeLookDownLeft";
    case MediapipeGesture::EYE_LOOK_DOWN_RIGHT:
      return "eyeLookDownRight";
    case MediapipeGesture::EYE_LOOK_IN_LEFT:
      return "eyeLookInLeft";
    case MediapipeGesture::EYE_LOOK_IN_RIGHT:
      return "eyeLookInRight";
    case MediapipeGesture::EYE_LOOK_OUT_LEFT:
      return "eyeLookOutLeft";
    case MediapipeGesture::EYE_LOOK_OUT_RIGHT:
      return "eyeLookOutRight";
    case MediapipeGesture::EYE_LOOK_UP_LEFT:
      return "eyeLookUpLeft";
    case MediapipeGesture::EYE_LOOK_UP_RIGHT:
      return "eyeLookUpRight";
    case MediapipeGesture::EYE_SQUINT_LEFT:
      return "eyeSquintLeft";
    case MediapipeGesture::EYE_SQUINT_RIGHT:
      return "eyeSquintRight";
    case MediapipeGesture::JAW_OPEN:
      return "jawOpen";
    case MediapipeGesture::MOUTH_LEFT:
      return "mouthLeft";
    case MediapipeGesture::MOUTH_PUCKER:
      return "mouthPucker";
    case MediapipeGesture::MOUTH_RIGHT:
      return "mouthRight";
    case MediapipeGesture::MOUTH_SMILE_LEFT:
      return "mouthSmileLeft";
    case MediapipeGesture::MOUTH_SMILE_RIGHT:
      return "mouthSmileRight";
    case MediapipeGesture::MOUTH_UPPER_UP_LEFT:
      return "mouthUpperUpLeft";
    case MediapipeGesture::MOUTH_UPPER_UP_RIGHT:
      return "mouthUpperUpRight";
  }
}

// Macros used by accessibility features on ChromeOS.
// Ensure this enum stays in sync with the source of truth in
// ash/webui/common/resources/accessibility/macro_names.ts.
enum MacroName {
  UNSPECIFIED = 0,
  INPUT_TEXT_VIEW = 1,
  DELETE_PREV_CHAR = 2,
  NAV_PREV_CHAR = 3,
  NAV_NEXT_CHAR = 4,
  NAV_PREV_LINE = 5,
  NAV_NEXT_LINE = 6,
  COPY_SELECTED_TEXT = 7,
  PASTE_TEXT = 8,
  CUT_SELECTED_TEXT = 9,
  UNDO_TEXT_EDIT = 10,
  REDO_ACTION = 11,
  SELECT_ALL_TEXT = 12,
  UNSELECT_TEXT = 13,
  LIST_COMMANDS = 14,
  NEW_LINE = 15,
  TOGGLE_DICTATION = 16,
  DELETE_PREV_WORD = 17,
  DELETE_PREV_SENT = 18,
  NAV_NEXT_WORD = 19,
  NAV_PREV_WORD = 20,
  SMART_DELETE_PHRASE = 21,
  SMART_REPLACE_PHRASE = 22,
  SMART_INSERT_BEFORE = 23,
  SMART_SELECT_BTWN_INCL = 24,
  NAV_NEXT_SENT = 25,
  NAV_PREV_SENT = 26,
  DELETE_ALL_TEXT = 27,
  NAV_START_TEXT = 28,
  NAV_END_TEXT = 29,
  SELECT_PREV_WORD = 30,
  SELECT_NEXT_WORD = 31,
  SELECT_NEXT_CHAR = 32,
  SELECT_PREV_CHAR = 33,
  REPEAT = 34,
  MOUSE_CLICK_LEFT = 35,
  MOUSE_CLICK_RIGHT = 36,
  RESET_CURSOR = 37,
  KEY_PRESS_SPACE = 38,
  KEY_PRESS_LEFT = 39,
  KEY_PRESS_RIGHT = 40,
  KEY_PRESS_UP = 41,
  KEY_PRESS_DOWN = 42,
};

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
          .WithGesturesToMacros(base::Value::Dict().Set(
              ToString(FaceGazeGesture::JAW_OPEN), MacroName::RESET_CURSOR))
          .WithGestureConfidences(base::Value::Dict().Set(
              ToString(FaceGazeGesture::JAW_OPEN), 70)));

  // Move cursor.
  utils()->ProcessFaceLandmarkerResult(
      MockFaceLandmarkerResult().WithNormalizedForeheadLocation(0.11, 0.21));
  utils()->TriggerMouseControllerInterval();
  AssertCursorAt(gfx::Point(360, 560));

  event_handler().ClearEvents();

  // Reset the cursor to the center of the screen using a gesture.
  utils()->ProcessFaceLandmarkerResult(MockFaceLandmarkerResult().WithGesture(
      ToString(MediapipeGesture::JAW_OPEN), 90));
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
          .WithGesturesToMacros(base::Value::Dict().Set(
              ToString(FaceGazeGesture::JAW_OPEN), MacroName::RESET_CURSOR))
          .WithGestureConfidences(base::Value::Dict().Set(
              ToString(FaceGazeGesture::JAW_OPEN), 100)));

  // Move cursor.
  utils()->ProcessFaceLandmarkerResult(
      MockFaceLandmarkerResult().WithNormalizedForeheadLocation(0.11, 0.21));
  utils()->TriggerMouseControllerInterval();
  AssertCursorAt(gfx::Point(360, 560));

  // Attempt to reset the cursor to the center of the screen using a gesture.
  // This gesture will be ignored because the gesture doesn't have high enough
  // confidence.
  event_handler().ClearEvents();
  utils()->ProcessFaceLandmarkerResult(MockFaceLandmarkerResult().WithGesture(
      ToString(MediapipeGesture::JAW_OPEN), 90));
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
  ConfigureFaceGaze(Config()
                        .AsDefault()
                        .WithGesturesToMacros(base::Value::Dict().Set(
                            ToString(FaceGazeGesture::MOUTH_LEFT),
                            MacroName::KEY_PRESS_SPACE))
                        .WithGestureConfidences(base::Value::Dict().Set(
                            ToString(FaceGazeGesture::MOUTH_LEFT), 70)));

  // Open jaw for space key press.
  event_handler().ClearEvents();
  utils()->ProcessFaceLandmarkerResult(MockFaceLandmarkerResult().WithGesture(
      ToString(MediapipeGesture::MOUTH_LEFT), 90));
  ASSERT_EQ(0u, event_handler().mouse_events().size());
  std::vector<ui::KeyEvent> key_events = event_handler().key_events();
  ASSERT_EQ(1u, key_events.size());
  ASSERT_EQ(ui::KeyboardCode::VKEY_SPACE, key_events[0].key_code());
  ASSERT_EQ(ui::EventType::ET_KEY_PRESSED, key_events[0].type());

  // Release gesture for space key release.
  utils()->ProcessFaceLandmarkerResult(MockFaceLandmarkerResult().WithGesture(
      ToString(MediapipeGesture::MOUTH_LEFT), 10));
  ASSERT_EQ(0u, event_handler().mouse_events().size());
  key_events = event_handler().key_events();
  ASSERT_EQ(2u, event_handler().key_events().size());
  ASSERT_EQ(ui::KeyboardCode::VKEY_SPACE, key_events[1].key_code());
  ASSERT_EQ(ui::EventType::ET_KEY_RELEASED, key_events[1].type());
}

}  // namespace ash
