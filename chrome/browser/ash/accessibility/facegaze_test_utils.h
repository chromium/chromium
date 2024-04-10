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

    MockFaceLandmarkerResult& WithGesture(const std::string& gesture,
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
