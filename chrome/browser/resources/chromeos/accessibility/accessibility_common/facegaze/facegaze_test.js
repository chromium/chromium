// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

GEN_INCLUDE(['facegaze_test_base.js']);

FaceGazeTest = class extends FaceGazeTestBase {
  /** @override */
  testGenPreamble() {
    super.testGenPreamble();
    super.testGenPreambleCommon(
        /*extensionIdName=*/ 'kAccessibilityCommonExtensionId',
        /*failOnConsoleError=*/ true);
  }
};

AX_TEST_F('FaceGazeTest', 'UpdateMouseLocation', async function() {
  this.setMouseLocation({x: 600, y: 400});
  const result =
      new MockFaceLandmarkerResult().setNormalizedForeheadLocation(0.1, 0.2);
  this.processFaceLandmarkerResult(result);
  assertEquals(1080, this.mockAccessibilityPrivate.getLatestCursorPosition().x);
  assertEquals(160, this.mockAccessibilityPrivate.getLatestCursorPosition().y);
});

AX_TEST_F('FaceGazeTest', 'DetectGesturesAndPerformActions', async function() {
  // Assert default mapping of gestures to actions.
  assertEquals(
      Action.CLICK_LEFT, this.getActionForGesture(FacialGesture.JAW_OPEN));
  assertEquals(
      Action.CLICK_RIGHT,
      this.getActionForGesture(FacialGesture.BROW_INNER_UP));

  this.setMouseLocation({x: 600, y: 400});
  const result =
      new MockFaceLandmarkerResult()
          .addGestureWithConfidence(FacialGesture.JAW_OPEN, 0.9)
          .addGestureWithConfidence(FacialGesture.BROW_INNER_UP, 0.3);
  this.processFaceLandmarkerResult(result);

  assertEquals(2, this.mockAccessibilityPrivate.syntheticMouseEvents_.length);
  const pressEvent = this.mockAccessibilityPrivate.syntheticMouseEvents_[0];
  assertEquals(
      this.mockAccessibilityPrivate.SyntheticMouseEventType.PRESS,
      pressEvent.type);
  assertEquals(
      this.mockAccessibilityPrivate.SyntheticMouseEventButton.LEFT,
      pressEvent.mouseButton);
  assertEquals(600, pressEvent.x);
  assertEquals(400, pressEvent.y);
  const releaseEvent = this.mockAccessibilityPrivate.syntheticMouseEvents_[1];
  assertEquals(
      this.mockAccessibilityPrivate.SyntheticMouseEventType.RELEASE,
      releaseEvent.type);
  assertEquals(
      this.mockAccessibilityPrivate.SyntheticMouseEventButton.LEFT,
      releaseEvent.mouseButton);
  assertEquals(600, releaseEvent.x);
  assertEquals(400, releaseEvent.y);
});
