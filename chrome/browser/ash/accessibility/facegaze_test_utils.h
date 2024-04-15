// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ACCESSIBILITY_FACEGAZE_TEST_UTILS_H_
#define CHROME_BROWSER_ASH_ACCESSIBILITY_FACEGAZE_TEST_UTILS_H_

#include <string>

#include "base/containers/flat_map.h"
#include "base/values.h"

namespace gfx {
class Point;
}  // namespace gfx

namespace ash {

// A class that can be used to exercise FaceGaze in browsertests.
class FaceGazeTestUtils {
 public:
  // The facial gestures that are supported by FaceGaze. Ensure this enum stays
  // in sync with the source of truth in
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

  // Facial gestures recognized by Mediapipe. Ensure this enum stays in sync
  // with the source of truth in chrome/browser/resources/chromeos/\
  // accessibility/accessibility_common/facegaze/gesture_detector.ts.
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

  // A struct that holds cursor speed values.
  struct CursorSpeeds {
    int up;
    int down;
    int left;
    int right;
  };

  // A class that represents a fake FaceLandmarkerResult.
  class MockFaceLandmarkerResult {
   public:
    MockFaceLandmarkerResult();
    ~MockFaceLandmarkerResult();
    MockFaceLandmarkerResult(const MockFaceLandmarkerResult&) = delete;
    MockFaceLandmarkerResult& operator=(const MockFaceLandmarkerResult&) =
        delete;

    MockFaceLandmarkerResult& WithNormalizedForeheadLocation(double x,
                                                             double y);
    const base::Value::Dict& forehead_location() const {
      return forehead_location_;
    }

    MockFaceLandmarkerResult& WithGesture(const MediapipeGesture& gesture,
                                          int confidence);
    const base::Value::List& recognized_gestures() const {
      return recognized_gestures_;
    }

   private:
    base::Value::Dict forehead_location_;
    base::Value::List recognized_gestures_;
  };

  FaceGazeTestUtils();
  ~FaceGazeTestUtils();
  FaceGazeTestUtils(const FaceGazeTestUtils&) = delete;
  FaceGazeTestUtils& operator=(const FaceGazeTestUtils&) = delete;

  // Enables and sets up FaceGaze.
  void EnableFaceGaze();
  // Creates and initializes the FaceLandmarker API within the extension.
  void CreateFaceLandmarker();
  // Waits for the cursor location to propagate to the FaceGaze MouseController.
  void WaitForCursorPosition(const gfx::Point& location);
  // Sets cursor speed prefs.
  void SetCursorSpeeds(const CursorSpeeds& speeds);
  // Sets the buffer size pref.
  void SetBufferSize(int size);
  // Sets the cursor acceleration pref.
  void SetCursorAcceleration(bool use_acceleration);
  // Sets the gesture to macro mapping pref.
  void SetGesturesToMacros(
      const base::flat_map<FaceGazeGesture, MacroName>& gestures_to_macros);
  // Sets the gesture confidences mapping pref.
  void SetGestureConfidences(
      const base::flat_map<FaceGazeGesture, int>& gesture_confidences);
  // Sets the gesture repeat delay threshold.
  void SetGestureRepeatDelayMs(int delay);
  // Forces FaceGaze to process `result`, since tests don't have access to real
  // camera data.
  void ProcessFaceLandmarkerResult(const MockFaceLandmarkerResult& result);
  // The MouseController updates the cursor location at a set interval. To
  // increase test stability, the interval is canceled in tests, and must be
  // triggered manually using this method.
  void TriggerMouseControllerInterval();

 private:
  void ExecuteAccessibilityCommonScript(const std::string& script);

  // Setup-related methods.
  void SetUpMediapipeDir();
  void WaitForJSReady();
  void SetUpJSTestSupport();
  void CancelMouseControllerInterval();
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_ACCESSIBILITY_FACEGAZE_TEST_UTILS_H_
