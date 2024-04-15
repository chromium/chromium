// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ACCESSIBILITY_FACEGAZE_TEST_UTILS_H_
#define CHROME_BROWSER_ASH_ACCESSIBILITY_FACEGAZE_TEST_UTILS_H_

#include <string>

#include "base/values.h"

namespace gfx {
class Point;
}  // namespace gfx

namespace ash {

// A class that can be used to exercise FaceGaze in browsertests.
class FaceGazeTestUtils {
 public:
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
  void SetGesturesToMacros(const base::Value::Dict& gestures_to_macros);
  // Sets the gesture confidences mapping pref.
  void SetGestureConfidences(const base::Value::Dict& gesture_confidences);
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
