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

  async startFacegazeWithConfigAndForeheadLocation_(
      config, forehead_x, forehead_y) {
    await this.configureFaceGaze(config);

    // No matter the starting location, the cursor position won't change
    // initially, and upcoming forehead locations will be computed relative to
    // this.
    const result = new MockFaceLandmarkerResult().setNormalizedForeheadLocation(
        forehead_x, forehead_y);
    this.processFaceLandmarkerResult(result);
    const cursorPosition =
        this.mockAccessibilityPrivate.getLatestCursorPosition();
    assertEquals(config.mouseLocation.x, cursorPosition.x);
    assertEquals(config.mouseLocation.y, cursorPosition.y);
  }
};

AX_TEST_F('FaceGazeTest', 'UpdateMouseLocation', async function() {
  const config =
      new Config().withMouseLocation({x: 600, y: 400}).withBufferSize(1);
  await this.startFacegazeWithConfigAndForeheadLocation_(config, 0.1, 0.2);

  // Move left and down. Note that increasing the x coordinate results in
  // moving left because the image is mirrored, while increasing y moves
  // downward as expected.
  result =
      new MockFaceLandmarkerResult().setNormalizedForeheadLocation(0.11, 0.21);
  this.processFaceLandmarkerResult(result);
  cursorPosition = this.mockAccessibilityPrivate.getLatestCursorPosition();
  assertEquals(594, cursorPosition.x);
  assertEquals(404, cursorPosition.y);

  // Move to where we were. Since the buffer size is 1, should end up at the
  // exact same location.
  result =
      new MockFaceLandmarkerResult().setNormalizedForeheadLocation(0.1, 0.2);
  this.processFaceLandmarkerResult(result);
  cursorPosition = this.mockAccessibilityPrivate.getLatestCursorPosition();
  assertEquals(600, cursorPosition.x);
  assertEquals(400, cursorPosition.y);
});

// Test that if the forehead location is moving around a different part of
// the screen, it still has the same offsets (i.e. we aren't tracking
// absolute forehead position, but instead relative).
// This test should use the same cursor positions as the previous version,
// but different forehead locations (with the same offsets).
AX_TEST_F(
    'FaceGazeTest', 'UpdateMouseLocationFromDifferentForeheadLocation',
    async function() {
      const config =
          new Config().withMouseLocation({x: 600, y: 400}).withBufferSize(1);
      await this.startFacegazeWithConfigAndForeheadLocation_(config, 0.6, 0.7);

      // Move left and down. Note that increasing the x coordinate results in
      // moving left because the image is mirrored.
      result = new MockFaceLandmarkerResult().setNormalizedForeheadLocation(
          0.61, 0.71);
      this.processFaceLandmarkerResult(result);
      cursorPosition = this.mockAccessibilityPrivate.getLatestCursorPosition();
      assertEquals(594, cursorPosition.x);
      assertEquals(404, cursorPosition.y);

      // Move to where we were. Since the buffer size is 1, should end up at the
      // exact same location.
      result = new MockFaceLandmarkerResult().setNormalizedForeheadLocation(
          0.6, 0.7);
      this.processFaceLandmarkerResult(result);
      cursorPosition = this.mockAccessibilityPrivate.getLatestCursorPosition();
      assertEquals(600, cursorPosition.x);
      assertEquals(400, cursorPosition.y);
    });

AX_TEST_F('FaceGazeTest', 'UpdateMouseLocationWithBuffer', async function() {
  const config =
      new Config().withMouseLocation({x: 600, y: 400}).withBufferSize(6);
  await this.startFacegazeWithConfigAndForeheadLocation_(config, 0.1, 0.2);

  // Move left and down. Note that increasing the x coordinate results in
  // moving left because the image is mirrored.
  result =
      new MockFaceLandmarkerResult().setNormalizedForeheadLocation(0.11, 0.21);
  this.processFaceLandmarkerResult(result);
  cursorPosition = this.mockAccessibilityPrivate.getLatestCursorPosition();
  assertTrue(cursorPosition.x < 600);
  assertTrue(cursorPosition.y > 400);

  // Move right and up. Due to smoothing, we don't exactly reach (600,400)
  // again, but do get closer to it.
  result =
      new MockFaceLandmarkerResult().setNormalizedForeheadLocation(0.1, 0.2);
  this.processFaceLandmarkerResult(result);
  let newCursorPosition =
      this.mockAccessibilityPrivate.getLatestCursorPosition();
  assertTrue(newCursorPosition.x > cursorPosition.x);
  assertTrue(newCursorPosition.y < cursorPosition.y);
  assertTrue(newCursorPosition.x < 600);
  assertTrue(newCursorPosition.y > 400);

  cursorPosition = newCursorPosition;
  // Process the same result again. We move even closer to (600, 400).
  this.processFaceLandmarkerResult(result);
  newCursorPosition = this.mockAccessibilityPrivate.getLatestCursorPosition();
  assertTrue(newCursorPosition.x > cursorPosition.x);
  assertTrue(newCursorPosition.y < cursorPosition.y);
  assertTrue(newCursorPosition.x < 600);
  assertTrue(newCursorPosition.y > 400);
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
