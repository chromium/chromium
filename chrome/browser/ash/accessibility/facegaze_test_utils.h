// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ACCESSIBILITY_FACEGAZE_TEST_UTILS_H_
#define CHROME_BROWSER_ASH_ACCESSIBILITY_FACEGAZE_TEST_UTILS_H_

#include <string>

namespace ash {

// A class that can be used to exercise FaceGaze in browsertests.
class FaceGazeTestUtils {
 public:
  FaceGazeTestUtils();
  ~FaceGazeTestUtils();
  FaceGazeTestUtils(const FaceGazeTestUtils&) = delete;
  FaceGazeTestUtils& operator=(const FaceGazeTestUtils&) = delete;

  // Enables and sets up FaceGaze.
  void EnableFaceGaze();
  // Creates and initializes the FaceLandmarker API within the extension.
  void CreateFaceLandmarker();

 private:
  void ExecuteAccessibilityCommonScript(const std::string& script);

  // Setup-related methods.
  void SetUpMediapipeDir();
  void WaitForJSReady();
  void SetUpJSTestSupport();
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_ACCESSIBILITY_FACEGAZE_TEST_UTILS_H_
