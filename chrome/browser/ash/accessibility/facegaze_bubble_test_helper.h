// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ACCESSIBILITY_FACEGAZE_BUBBLE_TEST_HELPER_H_
#define CHROME_BROWSER_ASH_ACCESSIBILITY_FACEGAZE_BUBBLE_TEST_HELPER_H_

namespace gfx {
class Point;
}  // namespace gfx

namespace ash {

class FaceGazeBubbleTestHelper {
 public:
  FaceGazeBubbleTestHelper();
  ~FaceGazeBubbleTestHelper();
  FaceGazeBubbleTestHelper(const FaceGazeBubbleTestHelper&) = delete;
  FaceGazeBubbleTestHelper& operator=(const FaceGazeBubbleTestHelper&) = delete;

  // Returns true if FaceGazeBubbleView is visible, false otherwise.
  bool IsVisible();

  // Returns the center point of the FaceGazeBubbleCloseView.
  gfx::Point GetCloseButtonCenterPoint();
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_ACCESSIBILITY_FACEGAZE_BUBBLE_TEST_HELPER_H_
