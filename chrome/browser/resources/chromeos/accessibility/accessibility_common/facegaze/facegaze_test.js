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
  const config = new Config().withMouseLocation({x: 600, y: 400});
  await this.configureFaceGaze(config);

  const result =
      new MockFaceLandmarkerResult().setNormalizedForeheadLocation(0.1, 0.2);
  this.processFaceLandmarkerResult(result);
  assertEquals(1080, this.mockAccessibilityPrivate.getLatestCursorPosition().x);
  assertEquals(160, this.mockAccessibilityPrivate.getLatestCursorPosition().y);
});

AX_TEST_F('FaceGazeTest', 'DetectGesturesAndPerformActions', async function() {
  const gestureToAction =
      new Map()
          .set(FacialGesture.JAW_OPEN, Action.CLICK_LEFT)
          .set(FacialGesture.BROW_INNER_UP, Action.CLICK_RIGHT);
  const gestureToConfidence = new Map()
                                  .set(FacialGesture.JAW_OPEN, 0.6)
                                  .set(FacialGesture.BROW_INNER_UP, 0.6);
  const config = new Config()
                     .withMouseLocation({x: 600, y: 400})
                     .withGestureToAction(gestureToAction)
                     .withGestureToConfidence(gestureToConfidence);
  await this.configureFaceGaze(config);

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

// The BrowDown gesture is special because it is the combination of two
// separate facial gestures. This test ensures that the associated action is
// only performed when both gestures are detected.
AX_TEST_F('FaceGazeTest', 'BrowDownGesture', async function() {
  const gestureToAction =
      new Map()
          .set(FacialGesture.BROW_DOWN_LEFT, Action.RESET_MOUSE)
          .set(FacialGesture.BROW_DOWN_RIGHT, Action.RESET_MOUSE);
  const gestureToConfidence = new Map()
                                  .set(FacialGesture.BROW_DOWN_LEFT, 0.6)
                                  .set(FacialGesture.BROW_DOWN_RIGHT, 0.6);
  const config = new Config()
                     .withMouseLocation({x: 0, y: 0})
                     .withGestureToAction(gestureToAction)
                     .withGestureToConfidence(gestureToConfidence);
  await this.configureFaceGaze(config);
  this.mockAccessibilityPrivate.clearCursorPosition();

  let result =
      new MockFaceLandmarkerResult()
          .addGestureWithConfidence(FacialGesture.BROW_DOWN_LEFT, 0.9)
          .addGestureWithConfidence(FacialGesture.BROW_DOWN_RIGHT, 0.3);
  this.processFaceLandmarkerResult(result);
  assertEquals(null, this.mockAccessibilityPrivate.getLatestCursorPosition());

  result = new MockFaceLandmarkerResult()
               .addGestureWithConfidence(FacialGesture.BROW_DOWN_LEFT, 0.3)
               .addGestureWithConfidence(FacialGesture.BROW_DOWN_RIGHT, 0.9);
  this.processFaceLandmarkerResult(result);
  assertEquals(null, this.mockAccessibilityPrivate.getLatestCursorPosition());

  result = new MockFaceLandmarkerResult()
               .addGestureWithConfidence(FacialGesture.BROW_DOWN_LEFT, 0.9)
               .addGestureWithConfidence(FacialGesture.BROW_DOWN_RIGHT, 0.9);
  this.processFaceLandmarkerResult(result);
  assertEquals(600, this.mockAccessibilityPrivate.getLatestCursorPosition().x);
  assertEquals(400, this.mockAccessibilityPrivate.getLatestCursorPosition().y);
});
