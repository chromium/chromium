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

  assertMouseClickAt(args) {
    const {pressEvent, releaseEvent, isLeft, x, y} = args;
    const button = isLeft ?
        this.mockAccessibilityPrivate.SyntheticMouseEventButton.LEFT :
        this.mockAccessibilityPrivate.SyntheticMouseEventButton.RIGHT;
    assertEquals(
        this.mockAccessibilityPrivate.SyntheticMouseEventType.PRESS,
        pressEvent.type);
    assertEquals(button, pressEvent.mouseButton);
    assertEquals(x, pressEvent.x);
    assertEquals(y, pressEvent.y);
    assertEquals(
        this.mockAccessibilityPrivate.SyntheticMouseEventType.RELEASE,
        releaseEvent.type);
    assertEquals(button, releaseEvent.mouseButton);
    assertEquals(x, releaseEvent.x);
    assertEquals(y, releaseEvent.y);
  }
};

AX_TEST_F(
    'FaceGazeTest', 'FacialGesturesInFacialGesturesToMediapipeGestures', () => {
      // Tests that all new FacialGestures are mapped to
      // MediapipeFacialGestures. FacialGestures are those set by the user,
      // while MediapipeFacialGestures are the raw gestures recognized by
      // Mediapipe. This is to help developers remember to add gestures to both.
      const facialGestures = Object.values(FacialGesture);
      const facialGesturesToMediapipeGestures =
          new Set(FacialGesturesToMediapipeGestures.keys().toArray());
      assertEquals(
          facialGestures.length, facialGesturesToMediapipeGestures.size);

      for (const gesture of facialGestures) {
        assertTrue(facialGesturesToMediapipeGestures.has(gesture));
      }
    });

AX_TEST_F(
    'FaceGazeTest',
    'GestureDetectorUpdatesStateAfterToggleGestureInfoForSettingsEvent',
    async function() {
      await this.configureFaceGaze(new Config());

      // Tests that GestureDetector updates its state after a
      // toggleGestureInfoForSettings event is received from
      // chrome.accessibilityPrivate.
      this.mockAccessibilityPrivate.toggleGestureInfoForSettings(false);
      assertFalse(GestureDetector.shouldSendGestureDetectionInfo_);

      this.mockAccessibilityPrivate.toggleGestureInfoForSettings(true);
      assertTrue(GestureDetector.shouldSendGestureDetectionInfo_);
    });

AX_TEST_F(
    'FaceGazeTest',
    'GestureDetectorSendsGestureInfoAfterToggleGestureInfoForSettingsEvent',
    async function() {
      const gestureToMacroName =
          new Map()
              .set(FacialGesture.BROW_INNER_UP, MacroName.MOUSE_CLICK_RIGHT)
              .set(FacialGesture.JAW_OPEN, MacroName.MOUSE_CLICK_LEFT);
      const gestureToConfidence = new Map()
                                      .set(FacialGesture.BROW_INNER_UP, 0.6)
                                      .set(FacialGesture.JAW_OPEN, 0.6);
      const config = new Config()
                         .withMouseLocation({x: 600, y: 400})
                         .withGestureToMacroName(gestureToMacroName)
                         .withGestureToConfidence(gestureToConfidence);
      await this.configureFaceGaze(config);

      // Toggle sending on.
      this.mockAccessibilityPrivate.toggleGestureInfoForSettings(true);
      assertTrue(GestureDetector.shouldSendGestureDetectionInfo_);

      const result =
          new MockFaceLandmarkerResult()
              .addGestureWithConfidence(
                  MediapipeFacialGesture.BROW_INNER_UP, 0.3)
              .addGestureWithConfidence(MediapipeFacialGesture.JAW_OPEN, 0.9);
      this.processFaceLandmarkerResult(result);

      // Assert both values are sent.
      assertEquals(
          this.mockAccessibilityPrivate.getSendGestureInfoToSettingsCount(), 1);
      const gestureInfo =
          this.mockAccessibilityPrivate.getFaceGazeGestureInfo();
      assertEquals(gestureInfo.length, 2);
      assertEquals(gestureInfo[0].gesture, FacialGesture.BROW_INNER_UP);
      assertEquals(gestureInfo[0].confidence, 30);
      assertEquals(gestureInfo[1].gesture, FacialGesture.JAW_OPEN);
      assertEquals(gestureInfo[1].confidence, 90);
    });

AX_TEST_F(
    'FaceGazeTest',
    'GestureDetectorDoesNotSendGestureInfoIfNoToggleGestureInfoForSettingsEvent',
    async function() {
      const gestureToMacroName =
          new Map()
              .set(FacialGesture.BROW_INNER_UP, MacroName.MOUSE_CLICK_RIGHT)
              .set(FacialGesture.JAW_OPEN, MacroName.MOUSE_CLICK_LEFT);
      const gestureToConfidence = new Map()
                                      .set(FacialGesture.BROW_INNER_UP, 0.6)
                                      .set(FacialGesture.JAW_OPEN, 0.6);
      const config = new Config()
                         .withMouseLocation({x: 600, y: 400})
                         .withGestureToMacroName(gestureToMacroName)
                         .withGestureToConfidence(gestureToConfidence);
      await this.configureFaceGaze(config);

      const result =
          new MockFaceLandmarkerResult()
              .addGestureWithConfidence(
                  MediapipeFacialGesture.BROW_INNER_UP, 0.3)
              .addGestureWithConfidence(MediapipeFacialGesture.JAW_OPEN, 0.9);
      this.processFaceLandmarkerResult(result);

      // Assert no call is made.
      assertEquals(
          0, this.mockAccessibilityPrivate.getSendGestureInfoToSettingsCount());
    });

AX_TEST_F('FaceGazeTest', 'IntervalReusesForeheadLocation', async function() {
  const config =
      new Config().withMouseLocation({x: 600, y: 400}).withBufferSize(1);
  await this.startFacegazeWithConfigAndForeheadLocation_(config, 0.1, 0.2);

  // Manually executing the mouse interval sets the cursor position with the
  // most recent forehead location.
  for (let i = 0; i < 3; i++) {
    this.mockAccessibilityPrivate.clearCursorPosition();
    this.triggerMouseControllerInterval();
    this.assertLatestCursorPosition({x: 600, y: 400});
  }
});

AX_TEST_F('FaceGazeTest', 'CursorPositionUpdatedOnInterval', async function() {
  const config =
      new Config().withMouseLocation({x: 600, y: 400}).withBufferSize(1);
  await this.startFacegazeWithConfigAndForeheadLocation_(config, 0.1, 0.2);

  const result =
      new MockFaceLandmarkerResult().setNormalizedForeheadLocation(0.2, 0.4);
  this.processFaceLandmarkerResult(
      result, /*triggerMouseControllerInterval=*/ false);

  // Cursor position doesn't change on result.
  this.assertLatestCursorPosition({x: 600, y: 400});

  // Cursor position does change after interval fired.
  this.triggerMouseControllerInterval();
  const cursorPosition =
      this.mockAccessibilityPrivate.getLatestCursorPosition();
  assertNotEquals(600, cursorPosition.x);
  assertNotEquals(400, cursorPosition.y);
});

AX_TEST_F('FaceGazeTest', 'UpdateMouseLocation', async function() {
  const config = new Config()
                     .withMouseLocation({x: 600, y: 400})
                     .withBufferSize(1)
                     .withCursorControlEnabled(true);
  await this.startFacegazeWithConfigAndForeheadLocation_(config, 0.1, 0.2);

  // Move left and down. Note that increasing the x coordinate results in
  // moving left because the image is mirrored, while increasing y moves
  // downward as expected.
  let result =
      new MockFaceLandmarkerResult().setNormalizedForeheadLocation(0.11, 0.21);
  this.processFaceLandmarkerResult(result);

  this.assertLatestCursorPosition({x: 360, y: 560});

  // Move to where we were. Since the buffer size is 1, should end up at the
  // exact same location.
  result =
      new MockFaceLandmarkerResult().setNormalizedForeheadLocation(0.1, 0.2);
  this.processFaceLandmarkerResult(result);
  this.assertLatestCursorPosition({x: 600, y: 400});

  // Try a larger movement, 10% of the screen.
  result =
      new MockFaceLandmarkerResult().setNormalizedForeheadLocation(0.12, 0.22);
  this.processFaceLandmarkerResult(result);
  this.assertLatestCursorPosition({x: 120, y: 720});

  result =
      new MockFaceLandmarkerResult().setNormalizedForeheadLocation(0.1, 0.2);
  this.processFaceLandmarkerResult(result);
  this.assertLatestCursorPosition({x: 600, y: 400});

  // Try a very small movement, 0.5% of the screen, which ends up being about
  // one pixel.
  result = new MockFaceLandmarkerResult().setNormalizedForeheadLocation(
      0.101, 0.201);
  this.processFaceLandmarkerResult(result);
  this.assertLatestCursorPosition({x: 580, y: 420});
});

AX_TEST_F(
    'FaceGazeTest', 'UpdatesMousePositionOnlyWhenCursorControlEnabled',
    async function() {
      const config = new Config()
                         .withMouseLocation({x: 600, y: 400})
                         .withBufferSize(1)
                         .withCursorControlEnabled(false);
      await this.startFacegazeWithConfigAndForeheadLocation_(config, 0.1, 0.2);

      // Move left and down. No events are generated.
      let result = new MockFaceLandmarkerResult().setNormalizedForeheadLocation(
          0.11, 0.21);
      this.processFaceLandmarkerResult(result);

      assertEquals(
          null, this.mockAccessibilityPrivate.getLatestCursorPosition());

      // Try moving back. Still nothing.
      result = new MockFaceLandmarkerResult().setNormalizedForeheadLocation(
          0.1, 0.2);
      this.processFaceLandmarkerResult(result);
      assertEquals(
          null, this.mockAccessibilityPrivate.getLatestCursorPosition());

      // Turn on cursor control.
      await this.setPref(FaceGaze.PREF_CURSOR_CONTROL_ENABLED, true);

      // Now head movement should do something.
      // This is the first detected head movement and should end up at the
      // original mouse location.
      result = new MockFaceLandmarkerResult().setNormalizedForeheadLocation(
          0.11, 0.21);
      this.processFaceLandmarkerResult(result);
      this.assertLatestCursorPosition({x: 600, y: 400});

      // Moving the head further should move the mouse away from the original
      // cursor position.
      result = new MockFaceLandmarkerResult().setNormalizedForeheadLocation(
          0.12, 0.22);
      this.processFaceLandmarkerResult(result);

      this.assertLatestCursorPosition({x: 360, y: 560});

      // Turn it off again and move the mouse further. Nothing should happen.
      await this.setPref(FaceGaze.PREF_CURSOR_CONTROL_ENABLED, false);
      result = new MockFaceLandmarkerResult().setNormalizedForeheadLocation(
          0.13, 0.23);
      this.processFaceLandmarkerResult(result);
      this.assertLatestCursorPosition({x: 360, y: 560});
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
      let result = new MockFaceLandmarkerResult().setNormalizedForeheadLocation(
          0.61, 0.71);
      this.processFaceLandmarkerResult(result);
      this.assertLatestCursorPosition({x: 360, y: 560});

      // Move to where we were. Since the buffer size is 1, should end up at the
      // exact same location.
      result = new MockFaceLandmarkerResult().setNormalizedForeheadLocation(
          0.6, 0.7);
      this.processFaceLandmarkerResult(result);
      this.assertLatestCursorPosition({x: 600, y: 400});
    });

// Tests that left/top offsets in ScreenBounds are respected. This should have
// the same results as the first test offset by exactly left/top.
AX_TEST_F(
    'FaceGazeTest', 'UpdateMouseLocationWithScreenNotAtZero', async function() {
      this.mockAccessibilityPrivate.setDisplayBounds(
          [{left: 100, top: 50, width: 1200, height: 800}]);

      const config =
          new Config().withMouseLocation({x: 700, y: 450}).withBufferSize(1);
      await this.startFacegazeWithConfigAndForeheadLocation_(config, 0.1, 0.2);

      // Move left and down. Note that increasing the x coordinate results in
      // moving left because the image is mirrored, while increasing y moves
      // downward as expected.
      let result = new MockFaceLandmarkerResult().setNormalizedForeheadLocation(
          0.11, 0.21);
      this.processFaceLandmarkerResult(result);

      this.assertLatestCursorPosition({x: 460, y: 610});

      // Move to where we were. Since the buffer size is 1, should end up at the
      // exact same location.
      result = new MockFaceLandmarkerResult().setNormalizedForeheadLocation(
          0.1, 0.2);
      this.processFaceLandmarkerResult(result);
      this.assertLatestCursorPosition({x: 700, y: 450});
    });

AX_TEST_F('FaceGazeTest', 'UpdateMouseLocationWithBuffer', async function() {
  const config =
      new Config().withMouseLocation({x: 600, y: 400}).withBufferSize(6);
  await this.startFacegazeWithConfigAndForeheadLocation_(config, 0.1, 0.2);

  // Move left and down. Note that increasing the x coordinate results in
  // moving left because the image is mirrored.
  let result =
      new MockFaceLandmarkerResult().setNormalizedForeheadLocation(0.11, 0.21);
  this.processFaceLandmarkerResult(result);
  let cursorPosition = this.mockAccessibilityPrivate.getLatestCursorPosition();
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

AX_TEST_F(
    'FaceGazeTest', 'UpdateMouseLocationWithSpeed1Move1', async function() {
      const config = new Config()
                         .withMouseLocation({x: 600, y: 400})
                         .withBufferSize(1)
                         .withSpeeds(1, 1, 1, 1);
      await this.startFacegazeWithConfigAndForeheadLocation_(config, 0.1, 0.2);

      // With mouse acceleration off and buffer size 1, one-pixel head movements
      // correspond to one-pixel changes on screen.
      const px = 1.0 / 1200;
      const py = 1.0 / 800;

      for (let i = 1; i < 10; i++) {
        const result =
            new MockFaceLandmarkerResult().setNormalizedForeheadLocation(
                0.1 + px * i, 0.2 + py * i);
        this.processFaceLandmarkerResult(result);
        this.assertLatestCursorPosition({x: 600 - i, y: 400 + i});
      }
    });

AX_TEST_F(
    'FaceGazeTest', 'UpdateMouseLocationWithSpeed1Move5', async function() {
      const config = new Config()
                         .withMouseLocation({x: 600, y: 400})
                         .withBufferSize(1)
                         .withSpeeds(1, 1, 1, 1);
      await this.startFacegazeWithConfigAndForeheadLocation_(config, 0.1, 0.2);
      const px = 1.0 / 1200;
      const py = 1.0 / 800;

      // Move further. Should get a linear increase in position.
      for (let i = 0; i < 5; i++) {
        const result =
            new MockFaceLandmarkerResult().setNormalizedForeheadLocation(
                0.1 + px * i * 5, 0.2 + py * i * 5);
        this.processFaceLandmarkerResult(result);
        this.assertLatestCursorPosition({x: 600 - i * 5, y: 400 + i * 5});
      }
    });

AX_TEST_F(
    'FaceGazeTest', 'UpdateMouseLocationWithSpeed1Move20', async function() {
      const config = new Config()
                         .withMouseLocation({x: 600, y: 400})
                         .withBufferSize(1)
                         .withSpeeds(1, 1, 1, 1);
      await this.startFacegazeWithConfigAndForeheadLocation_(config, 0.1, 0.2);
      const px = 1.0 / 1200;
      const py = 1.0 / 800;

      // Move even further. Should get a linear increase in position.
      for (let i = 0; i < 5; i++) {
        const result =
            new MockFaceLandmarkerResult().setNormalizedForeheadLocation(
                0.1 + px * i * 20, 0.2 + py * i * 20);
        this.processFaceLandmarkerResult(result);
        this.assertLatestCursorPosition({x: 600 - i * 20, y: 400 + i * 20});
      }
    });

AX_TEST_F(
    'FaceGazeTest', 'UpdateMouseLocationWithAccelerationMove1',
    async function() {
      const config = new Config()
                         .withMouseLocation({x: 600, y: 400})
                         .withBufferSize(1)
                         .withSpeeds(1, 1, 1, 1)
                         .withMouseAcceleration();
      await this.startFacegazeWithConfigAndForeheadLocation_(config, 0.1, 0.2);

      // With mouse acceleration off and buffer size 1, one-pixel head movements
      // correspond to one-pixel changes on screen.
      const px = 1.0 / 1200;
      const py = 1.0 / 800;
      let xLocation = 0.1;
      let yLocation = 0.2;

      // With these settings, moving the face by one pixel at a time is not ever
      // enough to move the cursor.
      for (let i = 1; i < 10; i++) {
        xLocation += px;
        yLocation += py;
        const result =
            new MockFaceLandmarkerResult().setNormalizedForeheadLocation(
                xLocation, yLocation);
        this.processFaceLandmarkerResult(result);
        this.assertLatestCursorPosition({x: 600, y: 400});
      }
    });

AX_TEST_F(
    'FaceGazeTest', 'UpdateMouseLocationWithAccelerationMove5',
    async function() {
      const config = new Config()
                         .withMouseLocation({x: 600, y: 400})
                         .withBufferSize(1)
                         .withSpeeds(1, 1, 1, 1)
                         .withMouseAcceleration();
      await this.startFacegazeWithConfigAndForeheadLocation_(config, 0.1, 0.2);
      const px = 1.0 / 1200;
      const py = 1.0 / 800;
      let xLocation = 0.1;
      let yLocation = 0.2;

      // Move by 5 pixels at a time to get a non-linear increase of 3 (movement
      // still dampened by sigmoid).
      for (let i = 1; i < 5; i++) {
        xLocation += px * 5;
        yLocation += py * 5;
        const result =
            new MockFaceLandmarkerResult().setNormalizedForeheadLocation(
                xLocation, yLocation);
        this.processFaceLandmarkerResult(result);
        this.assertLatestCursorPosition({x: 600 - i * 3, y: 400 + i * 3});
      }
    });

AX_TEST_F(
    'FaceGazeTest', 'UpdateMouseLocationWithAccelerationMove10',
    async function() {
      const config = new Config()
                         .withMouseLocation({x: 600, y: 400})
                         .withBufferSize(1)
                         .withSpeeds(1, 1, 1, 1)
                         .withMouseAcceleration();
      await this.startFacegazeWithConfigAndForeheadLocation_(config, 0.1, 0.2);

      // With mouse acceleration off and buffer size 1, one-pixel head movements
      // correspond to one-pixel changes on screen.
      const px = 1.0 / 1200;
      const py = 1.0 / 800;
      let xLocation = 0.1;
      let yLocation = 0.2;

      // Move by 10 pixels at a time to get a linear increase of 10 (sigma at
      // 1).
      for (let i = 1; i < 5; i++) {
        xLocation += px * 10;
        yLocation += py * 10;
        const initialCursorPosition =
            this.mockAccessibilityPrivate.getLatestCursorPosition();
        const result =
            new MockFaceLandmarkerResult().setNormalizedForeheadLocation(
                xLocation, yLocation);
        this.processFaceLandmarkerResult(result);
        const cursorPosition =
            this.mockAccessibilityPrivate.getLatestCursorPosition();
        assertEquals(-10, cursorPosition.x - initialCursorPosition.x);
        assertEquals(10, cursorPosition.y - initialCursorPosition.y);
      }
    });

AX_TEST_F(
    'FaceGazeTest', 'UpdateMouseLocationWithAccelerationMove20',
    async function() {
      const config = new Config()
                         .withMouseLocation({x: 600, y: 400})
                         .withBufferSize(1)
                         .withSpeeds(1, 1, 1, 1)
                         .withMouseAcceleration();
      await this.startFacegazeWithConfigAndForeheadLocation_(config, 0.1, 0.2);
      const px = 1.0 / 1200;
      const py = 1.0 / 800;
      let xLocation = 0.1;
      let yLocation = 0.2;

      // Finally, do a large movement of 20 px. It should be magnified further.
      for (let i = 1; i < 5; i++) {
        xLocation += px * 20;
        yLocation += py * 20;
        const initialCursorPosition =
            this.mockAccessibilityPrivate.getLatestCursorPosition();
        const result =
            new MockFaceLandmarkerResult().setNormalizedForeheadLocation(
                xLocation, yLocation);
        this.processFaceLandmarkerResult(result);
        const cursorPosition =
            this.mockAccessibilityPrivate.getLatestCursorPosition();
        assertEquals(-24, cursorPosition.x - initialCursorPosition.x);
        assertEquals(24, cursorPosition.y - initialCursorPosition.y);
      }
    });

AX_TEST_F(
    'FaceGazeTest', 'DetectGesturesAndPerformShortActions', async function() {
      const gestureToMacroName =
          new Map()
              .set(FacialGesture.JAW_OPEN, MacroName.MOUSE_CLICK_LEFT)
              .set(FacialGesture.BROW_INNER_UP, MacroName.MOUSE_CLICK_RIGHT);
      const gestureToConfidence = new Map()
                                      .set(FacialGesture.JAW_OPEN, 0.6)
                                      .set(FacialGesture.BROW_INNER_UP, 0.6);
      const config = new Config()
                         .withMouseLocation({x: 600, y: 400})
                         .withGestureToMacroName(gestureToMacroName)
                         .withGestureToConfidence(gestureToConfidence);
      await this.configureFaceGaze(config);

      let result =
          new MockFaceLandmarkerResult()
              .addGestureWithConfidence(MediapipeFacialGesture.JAW_OPEN, 0.9)
              .addGestureWithConfidence(
                  MediapipeFacialGesture.BROW_INNER_UP, 0.3);
      this.processFaceLandmarkerResult(result);

      this.assertNumMouseEvents(2);
      const pressEvent = this.getMouseEvents()[0];
      const releaseEvent = this.getMouseEvents()[1];
      this.assertMouseClickAt(
          {pressEvent, releaseEvent, isLeft: true, x: 600, y: 400});

      // Release all gestures, nothing should happen.
      result =
          new MockFaceLandmarkerResult()
              .addGestureWithConfidence(MediapipeFacialGesture.JAW_OPEN, 0.4)
              .addGestureWithConfidence(
                  MediapipeFacialGesture.BROW_INNER_UP, 0.3);
      this.processFaceLandmarkerResult(result);

      // No more events are generated.
      this.assertNumMouseEvents(2);
    });


AX_TEST_F(
    'FaceGazeTest', 'DetectGesturesAndPerformLongActions', async function() {
      const gestureToMacroName =
          new Map()
              .set(FacialGesture.JAW_OPEN, MacroName.MOUSE_LONG_CLICK_LEFT)
              .set(FacialGesture.BROW_INNER_UP, MacroName.MOUSE_CLICK_RIGHT);
      const gestureToConfidence = new Map()
                                      .set(FacialGesture.JAW_OPEN, 0.6)
                                      .set(FacialGesture.BROW_INNER_UP, 0.6);
      const config = new Config()
                         .withMouseLocation({x: 600, y: 400})
                         .withGestureToMacroName(gestureToMacroName)
                         .withGestureToConfidence(gestureToConfidence)
                         .withRepeatDelayMs(0);
      await this.configureFaceGaze(config);

      let result =
          new MockFaceLandmarkerResult()
              .addGestureWithConfidence(MediapipeFacialGesture.JAW_OPEN, 0.9)
              .addGestureWithConfidence(
                  MediapipeFacialGesture.BROW_INNER_UP, 0.3);
      this.processFaceLandmarkerResult(result);

      this.assertNumMouseEvents(1);
      const pressEvent = this.getMouseEvents()[0];
      assertEquals(
          this.mockAccessibilityPrivate.SyntheticMouseEventType.PRESS,
          pressEvent.type);
      assertEquals(
          this.mockAccessibilityPrivate.SyntheticMouseEventButton.LEFT,
          pressEvent.mouseButton);
      assertEquals(600, pressEvent.x);
      assertEquals(400, pressEvent.y);

      // Trigger jaw open again to get the release event.
      result =
          new MockFaceLandmarkerResult()
              .addGestureWithConfidence(MediapipeFacialGesture.JAW_OPEN, 0.8)
              .addGestureWithConfidence(
                  MediapipeFacialGesture.BROW_INNER_UP, 0.3);
      this.processFaceLandmarkerResult(result);

      this.assertNumMouseEvents(2);
      const releaseEvent = this.getMouseEvents()[1];
      assertEquals(
          this.mockAccessibilityPrivate.SyntheticMouseEventType.RELEASE,
          releaseEvent.type);
      assertEquals(
          this.mockAccessibilityPrivate.SyntheticMouseEventButton.LEFT,
          releaseEvent.mouseButton);
      assertEquals(600, releaseEvent.x);
      assertEquals(400, releaseEvent.y);
    });

AX_TEST_F(
    'FaceGazeTest', 'SendMouseDragFromCursorControlDuringLongClick',
    async function() {
      const gestureToMacroName = new Map().set(
          FacialGesture.JAW_OPEN, MacroName.MOUSE_LONG_CLICK_LEFT);
      const gestureToConfidence = new Map().set(FacialGesture.JAW_OPEN, 0.6);
      const config = new Config()
                         .withMouseLocation({x: 600, y: 400})
                         .withGestureToMacroName(gestureToMacroName)
                         .withGestureToConfidence(gestureToConfidence)
                         .withCursorControlEnabled(true)
                         .withBufferSize(1)
                         .withRepeatDelayMs(0);
      await this.startFacegazeWithConfigAndForeheadLocation_(config, 0.1, 0.2);
      this.triggerMouseControllerInterval();
      this.assertLatestCursorPosition({x: 600, y: 400});

      // Cursor control sends two synthetic mouse events.
      this.assertNumMouseEvents(2);

      let result = new MockFaceLandmarkerResult().addGestureWithConfidence(
          MediapipeFacialGesture.JAW_OPEN, 0.9);
      this.processFaceLandmarkerResult(
          result, /*triggerMouseControllerInterval=*/ false);

      this.assertNumMouseEvents(3);
      const pressEvent = this.getMouseEvents()[2];
      assertEquals(
          this.mockAccessibilityPrivate.SyntheticMouseEventType.PRESS,
          pressEvent.type);
      assertEquals(
          this.mockAccessibilityPrivate.SyntheticMouseEventButton.LEFT,
          pressEvent.mouseButton);
      assertEquals(600, pressEvent.x);
      assertEquals(400, pressEvent.y);

      // Move the cursor to trigger drag event. Cursor control will send another
      // two synthetic mouse events.
      result = new MockFaceLandmarkerResult().setNormalizedForeheadLocation(
          0.11, 0.21);
      this.processFaceLandmarkerResult(result);
      this.triggerMouseControllerInterval();
      this.assertLatestCursorPosition({x: 360, y: 560});

      this.assertNumMouseEvents(5);
      let dragEvent = this.getMouseEvents()[3];
      assertEquals(
          this.mockAccessibilityPrivate.SyntheticMouseEventType.DRAG,
          dragEvent.type);
      assertEquals(
          this.mockAccessibilityPrivate.SyntheticMouseEventButton.LEFT,
          dragEvent.mouseButton);
      assertEquals(360, dragEvent.x);
      assertEquals(560, dragEvent.y);
      dragEvent = this.getMouseEvents()[4];
      assertEquals(
          this.mockAccessibilityPrivate.SyntheticMouseEventType.DRAG,
          dragEvent.type);
      assertEquals(
          this.mockAccessibilityPrivate.SyntheticMouseEventButton.LEFT,
          dragEvent.mouseButton);
      assertEquals(360, dragEvent.x);
      assertEquals(560, dragEvent.y);

      // Trigger jaw open again to get the release event.
      result = new MockFaceLandmarkerResult().addGestureWithConfidence(
          MediapipeFacialGesture.JAW_OPEN, 0.8);
      this.processFaceLandmarkerResult(
          result, /*triggerMouseControllerInterval=*/ false);

      this.assertNumMouseEvents(6);
      const releaseEvent = this.getMouseEvents()[5];
      assertEquals(
          this.mockAccessibilityPrivate.SyntheticMouseEventType.RELEASE,
          releaseEvent.type);
      assertEquals(
          this.mockAccessibilityPrivate.SyntheticMouseEventButton.LEFT,
          releaseEvent.mouseButton);
      assertEquals(360, releaseEvent.x);
      assertEquals(560, releaseEvent.y);
    });

AX_TEST_F(
    'FaceGazeTest', 'SendMouseDragFromUserDuringLongClick', async function() {
      const gestureToMacroName = new Map().set(
          FacialGesture.JAW_OPEN, MacroName.MOUSE_LONG_CLICK_LEFT);
      const gestureToConfidence = new Map().set(FacialGesture.JAW_OPEN, 0.6);
      const config = new Config()
                         .withMouseLocation({x: 600, y: 400})
                         .withGestureToMacroName(gestureToMacroName)
                         .withGestureToConfidence(gestureToConfidence)
                         .withRepeatDelayMs(0);
      await this.configureFaceGaze(config);

      let result = new MockFaceLandmarkerResult().addGestureWithConfidence(
          MediapipeFacialGesture.JAW_OPEN, 0.9);
      this.processFaceLandmarkerResult(
          result, /*triggerMouseControllerInterval=*/ false);

      this.assertNumMouseEvents(1);
      const pressEvent = this.getMouseEvents()[0];
      assertEquals(
          this.mockAccessibilityPrivate.SyntheticMouseEventType.PRESS,
          pressEvent.type);
      assertEquals(
          this.mockAccessibilityPrivate.SyntheticMouseEventButton.LEFT,
          pressEvent.mouseButton);
      assertEquals(600, pressEvent.x);
      assertEquals(400, pressEvent.y);

      // Move the cursor to trigger drag event.
      this.sendAutomationMouseEvent(
          {mouseX: 360, mouseY: 560, eventFrom: 'user'});

      this.assertNumMouseEvents(2);
      const dragEvent = this.getMouseEvents()[1];
      assertEquals(
          this.mockAccessibilityPrivate.SyntheticMouseEventType.DRAG,
          dragEvent.type);
      assertEquals(
          this.mockAccessibilityPrivate.SyntheticMouseEventButton.LEFT,
          dragEvent.mouseButton);
      assertEquals(360, dragEvent.x);
      assertEquals(560, dragEvent.y);

      // Trigger jaw open again to get the release event.
      result = new MockFaceLandmarkerResult().addGestureWithConfidence(
          MediapipeFacialGesture.JAW_OPEN, 0.8);
      this.processFaceLandmarkerResult(
          result, /*triggerMouseControllerInterval=*/ false);

      this.assertNumMouseEvents(3);
      const releaseEvent = this.getMouseEvents()[2];
      assertEquals(
          this.mockAccessibilityPrivate.SyntheticMouseEventType.RELEASE,
          releaseEvent.type);
      assertEquals(
          this.mockAccessibilityPrivate.SyntheticMouseEventButton.LEFT,
          releaseEvent.mouseButton);
      assertEquals(360, releaseEvent.x);
      assertEquals(560, releaseEvent.y);
    });

// The BrowDown gesture is special because it is the combination of two
// separate facial gestures. This test ensures that the associated action is
// performed if either of the gestures is detected.
AX_TEST_F('FaceGazeTest', 'BrowDownGesture', async function() {
  const gestureToMacroName =
      new Map().set(FacialGesture.BROWS_DOWN, MacroName.RESET_CURSOR);
  const gestureToConfidence = new Map().set(FacialGesture.BROWS_DOWN, 0.6);
  const config = new Config()
                     .withMouseLocation({x: 0, y: 0})
                     .withGestureToMacroName(gestureToMacroName)
                     .withGestureToConfidence(gestureToConfidence);
  await this.configureFaceGaze(config);
  this.mockAccessibilityPrivate.clearCursorPosition();

  let result =
      new MockFaceLandmarkerResult()
          .addGestureWithConfidence(MediapipeFacialGesture.BROW_DOWN_LEFT, 0.3)
          .addGestureWithConfidence(
              MediapipeFacialGesture.BROW_DOWN_RIGHT, 0.3);
  this.processFaceLandmarkerResult(result);
  assertEquals(null, this.mockAccessibilityPrivate.getLatestCursorPosition());

  result =
      new MockFaceLandmarkerResult()
          .addGestureWithConfidence(MediapipeFacialGesture.BROW_DOWN_LEFT, 0.9)
          .addGestureWithConfidence(
              MediapipeFacialGesture.BROW_DOWN_RIGHT, 0.3);
  this.processFaceLandmarkerResult(result);
  this.assertLatestCursorPosition({x: 600, y: 400});
  this.mockAccessibilityPrivate.clearCursorPosition();
  this.clearGestureLastRecognizedTime();

  result =
      new MockFaceLandmarkerResult()
          .addGestureWithConfidence(MediapipeFacialGesture.BROW_DOWN_LEFT, 0.3)
          .addGestureWithConfidence(
              MediapipeFacialGesture.BROW_DOWN_RIGHT, 0.9);
  this.processFaceLandmarkerResult(result);
  this.assertLatestCursorPosition({x: 600, y: 400});
  this.mockAccessibilityPrivate.clearCursorPosition();
  this.clearGestureLastRecognizedTime();

  result =
      new MockFaceLandmarkerResult()
          .addGestureWithConfidence(MediapipeFacialGesture.BROW_DOWN_LEFT, 0.9)
          .addGestureWithConfidence(
              MediapipeFacialGesture.BROW_DOWN_RIGHT, 0.9);
  this.processFaceLandmarkerResult(result);
  this.assertLatestCursorPosition({x: 600, y: 400});
});

AX_TEST_F(
    'FaceGazeTest', 'DoesNotPerformActionsWhenActionsDisabled',
    async function() {
      const gestureToMacroName =
          new Map()
              .set(FacialGesture.JAW_OPEN, MacroName.MOUSE_CLICK_LEFT)
              .set(FacialGesture.BROW_INNER_UP, MacroName.MOUSE_CLICK_RIGHT);
      const gestureToConfidence = new Map()
                                      .set(FacialGesture.JAW_OPEN, 0.6)
                                      .set(FacialGesture.BROW_INNER_UP, 0.6);
      const config = new Config()
                         .withMouseLocation({x: 600, y: 400})
                         .withGestureToMacroName(gestureToMacroName)
                         .withGestureToConfidence(gestureToConfidence)
                         .withCursorControlEnabled(true)
                         .withActionsEnabled(false);
      await this.configureFaceGaze(config);

      let result =
          new MockFaceLandmarkerResult()
              .addGestureWithConfidence(MediapipeFacialGesture.JAW_OPEN, 0.9)
              .addGestureWithConfidence(
                  MediapipeFacialGesture.BROW_INNER_UP, 0.3);
      this.processFaceLandmarkerResult(result);

      this.assertNumMouseEvents(0);

      result =
          new MockFaceLandmarkerResult()
              .addGestureWithConfidence(MediapipeFacialGesture.JAW_OPEN, 0.9)
              .addGestureWithConfidence(
                  MediapipeFacialGesture.BROW_INNER_UP, 0.9);
      this.processFaceLandmarkerResult(result);

      this.assertNumMouseEvents(0);

      // Enable actions. Now we we should get actions.
      await this.setPref(FaceGaze.PREF_ACTIONS_ENABLED, true);

      result =
          new MockFaceLandmarkerResult()
              .addGestureWithConfidence(MediapipeFacialGesture.JAW_OPEN, 0.9)
              .addGestureWithConfidence(
                  MediapipeFacialGesture.BROW_INNER_UP, 0.3);
      this.processFaceLandmarkerResult(result);

      this.assertNumMouseEvents(2);
      const pressEvent = this.getMouseEvents()[0];
      const releaseEvent = this.getMouseEvents()[1];
      this.assertMouseClickAt(
          {pressEvent, releaseEvent, isLeft: true, x: 600, y: 400});
    });

AX_TEST_F(
    'FaceGazeTest', 'ActionsUseMouseLocationWhenCursorControlDisabled',
    async function() {
      const gestureToMacroName = new Map().set(
          FacialGesture.MOUTH_PUCKER, MacroName.MOUSE_CLICK_RIGHT);
      const gestureToConfidence =
          new Map().set(FacialGesture.MOUTH_PUCKER, 0.5);
      const config = new Config()
                         .withMouseLocation({x: 600, y: 400})
                         .withGestureToMacroName(gestureToMacroName)
                         .withGestureToConfidence(gestureToConfidence)
                         .withActionsEnabled(true)
                         .withCursorControlEnabled(false);
      await this.configureFaceGaze(config);

      // Even though the mouse controller is off, automation mouse events still
      // change the mouse position for click actions.
      this.sendAutomationMouseEvent(
          {mouseX: 350, mouseY: 250, eventFrom: 'user'});

      result = new MockFaceLandmarkerResult().addGestureWithConfidence(
          MediapipeFacialGesture.MOUTH_PUCKER, 0.7);
      this.processFaceLandmarkerResult(result);

      this.assertNumMouseEvents(2);
      const pressEvent = this.getMouseEvents()[0];
      const releaseEvent = this.getMouseEvents()[1];
      this.assertMouseClickAt(
          {pressEvent, releaseEvent, isLeft: false, x: 350, y: 250});
    });

AX_TEST_F('FaceGazeTest', 'DoesNotRepeatGesturesTooSoon', async function() {
  const gestureToMacroName =
      new Map()
          .set(FacialGesture.JAW_OPEN, MacroName.MOUSE_LONG_CLICK_LEFT)
          .set(FacialGesture.BROW_INNER_UP, MacroName.RESET_CURSOR)
          .set(FacialGesture.BROWS_DOWN, MacroName.MOUSE_CLICK_RIGHT);
  const gestureToConfidence = new Map()
                                  .set(FacialGesture.JAW_OPEN, 0.6)
                                  .set(FacialGesture.BROW_INNER_UP, 0.6)
                                  .set(FacialGesture.BROWS_DOWN, 0.6);
  const config = new Config()
                     .withMouseLocation({x: 600, y: 400})
                     .withGestureToMacroName(gestureToMacroName)
                     .withGestureToConfidence(gestureToConfidence)
                     .withRepeatDelayMs(1000);
  await this.configureFaceGaze(config);

  for (let i = 0; i < 5; i++) {
    const result =
        new MockFaceLandmarkerResult()
            .addGestureWithConfidence(MediapipeFacialGesture.JAW_OPEN, 0.9)
            .addGestureWithConfidence(
                MediapipeFacialGesture.BROW_INNER_UP, 0.3);
    this.processFaceLandmarkerResult(result);

    // 5 times in quick succession still only generates one press.
    this.assertNumMouseEvents(1);
    const pressEvent = this.getMouseEvents()[0];
    assertEquals(
        this.mockAccessibilityPrivate.SyntheticMouseEventType.PRESS,
        pressEvent.type);
    assertEquals(
        this.mockAccessibilityPrivate.SyntheticMouseEventButton.LEFT,
        pressEvent.mouseButton);
  }

  this.getFaceGaze().gestureHandler_.repeatDelayMs_ = 0;

  // Release is generated when the JAW_OPEN is triggered again.
  let result =
      new MockFaceLandmarkerResult()
          .addGestureWithConfidence(MediapipeFacialGesture.JAW_OPEN, 0.9)
          .addGestureWithConfidence(MediapipeFacialGesture.BROW_INNER_UP, 0.3);
  this.processFaceLandmarkerResult(result);
  this.assertNumMouseEvents(2);
  let releaseEvent = this.getMouseEvents()[1];
  assertEquals(
      this.mockAccessibilityPrivate.SyntheticMouseEventType.RELEASE,
      releaseEvent.type);
  assertEquals(
      this.mockAccessibilityPrivate.SyntheticMouseEventButton.LEFT,
      releaseEvent.mouseButton);

  this.getFaceGaze().gestureHandler_.repeatDelayMs_ = 1000;

  // Another gesture is let through once and then also throttled.
  for (let i = 0; i < 5; i++) {
    const result = new MockFaceLandmarkerResult()
                       .addGestureWithConfidence(
                           MediapipeFacialGesture.BROW_DOWN_LEFT, 0.9)
                       .addGestureWithConfidence(
                           MediapipeFacialGesture.BROW_DOWN_RIGHT, 0.9);
    this.processFaceLandmarkerResult(result);

    this.assertNumMouseEvents(4);
    const pressEvent = this.getMouseEvents()[2];
    assertEquals(
        this.mockAccessibilityPrivate.SyntheticMouseEventType.PRESS,
        pressEvent.type);
    assertEquals(
        this.mockAccessibilityPrivate.SyntheticMouseEventButton.RIGHT,
        pressEvent.mouseButton);
    releaseEvent = this.getMouseEvents()[3];
    assertEquals(
        this.mockAccessibilityPrivate.SyntheticMouseEventType.RELEASE,
        releaseEvent.type);
    assertEquals(
        this.mockAccessibilityPrivate.SyntheticMouseEventButton.RIGHT,
        releaseEvent.mouseButton);
  }

  // No further events are sent when BROWS_DOWN ends.
  result =
      new MockFaceLandmarkerResult()
          .addGestureWithConfidence(MediapipeFacialGesture.BROW_DOWN_LEFT, 0.4)
          .addGestureWithConfidence(
              MediapipeFacialGesture.BROW_DOWN_RIGHT, 0.3);
  this.processFaceLandmarkerResult(result);
  this.assertNumMouseEvents(4);
});

AX_TEST_F('FaceGazeTest', 'DoesNotClickDuringLongClick', async function() {
  const gestureToMacroName =
      new Map()
          .set(FacialGesture.MOUTH_PUCKER, MacroName.MOUSE_LONG_CLICK_LEFT)
          .set(FacialGesture.EYE_SQUINT_LEFT, MacroName.MOUSE_CLICK_LEFT)
          .set(FacialGesture.EYE_SQUINT_RIGHT, MacroName.MOUSE_CLICK_RIGHT);

  const config = new Config()
                     .withMouseLocation({x: 600, y: 400})
                     .withGestureToMacroName(gestureToMacroName)
                     .withRepeatDelayMs(0);
  await this.configureFaceGaze(config);

  // Start the long click.
  let result =
      new MockFaceLandmarkerResult()
          .addGestureWithConfidence(MediapipeFacialGesture.MOUTH_PUCKER, 0.9)
          .addGestureWithConfidence(MediapipeFacialGesture.EYE_SQUINT_LEFT, 0.3)
          .addGestureWithConfidence(
              MediapipeFacialGesture.EYE_SQUINT_RIGHT, 0.3);
  this.processFaceLandmarkerResult(result);

  this.assertNumMouseEvents(1);
  const pressEvent = this.getMouseEvents()[0];
  assertEquals(
      this.mockAccessibilityPrivate.SyntheticMouseEventType.PRESS,
      pressEvent.type);
  assertEquals(
      this.mockAccessibilityPrivate.SyntheticMouseEventButton.LEFT,
      pressEvent.mouseButton);
  assertEquals(600, pressEvent.x);
  assertEquals(400, pressEvent.y);

  // Send a short left click gesture while long click is active.
  result =
      new MockFaceLandmarkerResult()
          .addGestureWithConfidence(MediapipeFacialGesture.EYE_SQUINT_LEFT, 0.9)
          .addGestureWithConfidence(
              MediapipeFacialGesture.EYE_SQUINT_RIGHT, 0.3);
  this.processFaceLandmarkerResult(result);
  // No more events are generated.
  this.assertNumMouseEvents(1);
  result =
      new MockFaceLandmarkerResult()
          .addGestureWithConfidence(MediapipeFacialGesture.EYE_SQUINT_LEFT, 0.3)
          .addGestureWithConfidence(
              MediapipeFacialGesture.EYE_SQUINT_RIGHT, 0.3);
  this.processFaceLandmarkerResult(result);
  // No more events are generated.
  this.assertNumMouseEvents(1);

  // Try with short right click gesture while long click is active.
  result =
      new MockFaceLandmarkerResult()
          .addGestureWithConfidence(MediapipeFacialGesture.EYE_SQUINT_LEFT, 0.3)
          .addGestureWithConfidence(
              MediapipeFacialGesture.EYE_SQUINT_RIGHT, 0.9);
  this.processFaceLandmarkerResult(result);
  // No more events are generated.
  this.assertNumMouseEvents(1);

  result =
      new MockFaceLandmarkerResult()
          .addGestureWithConfidence(MediapipeFacialGesture.EYE_SQUINT_LEFT, 0.3)
          .addGestureWithConfidence(
              MediapipeFacialGesture.EYE_SQUINT_RIGHT, 0.3);
  this.processFaceLandmarkerResult(result);
  // No more events are generated.
  this.assertNumMouseEvents(1);

  // Send the end of the long click by triggering mouth pucker again.
  result =
      new MockFaceLandmarkerResult()
          .addGestureWithConfidence(MediapipeFacialGesture.MOUTH_PUCKER, 0.9)
          .addGestureWithConfidence(MediapipeFacialGesture.EYE_SQUINT_LEFT, 0.3)
          .addGestureWithConfidence(
              MediapipeFacialGesture.EYE_SQUINT_RIGHT, 0.3);
  this.processFaceLandmarkerResult(result);
  this.assertNumMouseEvents(2);
  const releaseEvent = this.getMouseEvents()[1];
  assertEquals(
      this.mockAccessibilityPrivate.SyntheticMouseEventType.RELEASE,
      releaseEvent.type);
  assertEquals(
      this.mockAccessibilityPrivate.SyntheticMouseEventButton.LEFT,
      releaseEvent.mouseButton);
  assertEquals(600, releaseEvent.x);
  assertEquals(400, releaseEvent.y);
});

AX_TEST_F('FaceGazeTest', 'KeyEvents', async function() {
  const gestureToMacroName =
      new Map()
          .set(FacialGesture.EYE_SQUINT_LEFT, MacroName.KEY_PRESS_SPACE)
          .set(FacialGesture.EYE_SQUINT_RIGHT, MacroName.KEY_PRESS_UP)
          .set(FacialGesture.MOUTH_SMILE, MacroName.KEY_PRESS_DOWN)
          .set(FacialGesture.MOUTH_UPPER_UP, MacroName.KEY_PRESS_LEFT)
          .set(FacialGesture.EYES_BLINK, MacroName.KEY_PRESS_RIGHT)
          .set(FacialGesture.JAW_OPEN, MacroName.KEY_PRESS_TOGGLE_OVERVIEW)
          .set(
              FacialGesture.MOUTH_PUCKER, MacroName.KEY_PRESS_MEDIA_PLAY_PAUSE);
  const gestureToConfidence = new Map()
                                  .set(FacialGesture.EYE_SQUINT_LEFT, 0.7)
                                  .set(FacialGesture.EYE_SQUINT_RIGHT, 0.7)
                                  .set(FacialGesture.MOUTH_SMILE, 0.7)
                                  .set(FacialGesture.MOUTH_UPPER_UP, 0.7)
                                  .set(FacialGesture.EYES_BLINK, 0.7)
                                  .set(FacialGesture.JAW_OPEN, 0.7)
                                  .set(FacialGesture.MOUTH_PUCKER, 0.7);
  const config = new Config()
                     .withMouseLocation({x: 600, y: 400})
                     .withGestureToMacroName(gestureToMacroName)
                     .withGestureToConfidence(gestureToConfidence)
                     .withRepeatDelayMs(1000);
  await this.configureFaceGaze(config);

  const makeResultAndProcess = (gestures) => {
    const result = new MockFaceLandmarkerResult()
                       .addGestureWithConfidence(
                           MediapipeFacialGesture.EYE_SQUINT_LEFT,
                           gestures.squintLeft ? gestures.squintLeft : 0.3)
                       .addGestureWithConfidence(
                           MediapipeFacialGesture.EYE_SQUINT_RIGHT,
                           gestures.squintRight ? gestures.squintRight : 0.3)
                       .addGestureWithConfidence(
                           MediapipeFacialGesture.MOUTH_SMILE_LEFT,
                           gestures.smileLeft ? gestures.smileLeft : 0.3)
                       .addGestureWithConfidence(
                           MediapipeFacialGesture.MOUTH_SMILE_RIGHT,
                           gestures.smileRight ? gestures.smileRight : 0.3)
                       .addGestureWithConfidence(
                           MediapipeFacialGesture.MOUTH_UPPER_UP_LEFT,
                           gestures.upperUpLeft ? gestures.upperUpLeft : 0.3)
                       .addGestureWithConfidence(
                           MediapipeFacialGesture.MOUTH_UPPER_UP_RIGHT,
                           gestures.upperUpRight ? gestures.upperUpRight : 0.3)
                       .addGestureWithConfidence(
                           MediapipeFacialGesture.EYE_BLINK_LEFT,
                           gestures.blinkLeft ? gestures.blinkLeft : 0.3)
                       .addGestureWithConfidence(
                           MediapipeFacialGesture.EYE_BLINK_RIGHT,
                           gestures.blinkRight ? gestures.blinkRight : 0.3)
                       .addGestureWithConfidence(
                           MediapipeFacialGesture.JAW_OPEN,
                           gestures.jawOpen ? gestures.jawOpen : 0.3)
                       .addGestureWithConfidence(
                           MediapipeFacialGesture.MOUTH_PUCKER,
                           gestures.mouthPucker ? gestures.mouthPucker : 0.3);
    this.processFaceLandmarkerResult(result);
    return this.getKeyEvents();
  };

  // Squint left for space key press.
  let keyEvents = makeResultAndProcess({squintLeft: .75});
  assertEquals(1, keyEvents.length);
  assertEquals(
      chrome.accessibilityPrivate.SyntheticKeyboardEventType.KEYDOWN,
      keyEvents[0].type);
  assertEquals(KeyCode.SPACE, keyEvents[0].keyCode);

  // Stop squinting left for space key release.
  keyEvents = makeResultAndProcess({});
  assertEquals(2, keyEvents.length);
  assertEquals(
      chrome.accessibilityPrivate.SyntheticKeyboardEventType.KEYUP,
      keyEvents[1].type);
  assertEquals(KeyCode.SPACE, keyEvents[1].keyCode);

  // Squint right eye for up key press.
  keyEvents = makeResultAndProcess({squintRight: 0.8});
  assertEquals(3, keyEvents.length);
  assertEquals(
      chrome.accessibilityPrivate.SyntheticKeyboardEventType.KEYDOWN,
      keyEvents[2].type);
  assertEquals(KeyCode.UP, keyEvents[2].keyCode);

  // Start smiling on both sides to create down arrow key press.
  keyEvents = makeResultAndProcess(
      {squintRight: 0.8, smileLeft: 0.9, smileRight: 0.85});
  assertEquals(4, keyEvents.length);
  assertEquals(
      chrome.accessibilityPrivate.SyntheticKeyboardEventType.KEYDOWN,
      keyEvents[3].type);
  assertEquals(KeyCode.DOWN, keyEvents[3].keyCode);

  // Stop squinting right eye for up arrow key release.
  keyEvents = makeResultAndProcess({smileLeft: 0.9, smileRight: 0.85});
  assertEquals(5, keyEvents.length);
  assertEquals(
      chrome.accessibilityPrivate.SyntheticKeyboardEventType.KEYUP,
      keyEvents[4].type);
  assertEquals(KeyCode.UP, keyEvents[4].keyCode);

  // Stop smiling for down arrow key release.
  keyEvents = makeResultAndProcess({});
  assertEquals(6, keyEvents.length);
  assertEquals(
      chrome.accessibilityPrivate.SyntheticKeyboardEventType.KEYUP,
      keyEvents[5].type);
  assertEquals(KeyCode.DOWN, keyEvents[5].keyCode);

  // Mouth upper up on both sides for left key press.
  keyEvents = makeResultAndProcess({upperUpLeft: 0.9, upperUpRight: 0.8});
  assertEquals(7, keyEvents.length);
  assertEquals(
      chrome.accessibilityPrivate.SyntheticKeyboardEventType.KEYDOWN,
      keyEvents[6].type);
  assertEquals(KeyCode.LEFT, keyEvents[6].keyCode);

  // Blink both eyes for right key press.
  keyEvents = makeResultAndProcess({
    upperUpLeft: 0.85,
    upperUpRight: 0.9,
    blinkLeft: 0.85,
    blinkRight: 0.75,
  });
  assertEquals(8, keyEvents.length);
  assertEquals(
      chrome.accessibilityPrivate.SyntheticKeyboardEventType.KEYDOWN,
      keyEvents[7].type);
  assertEquals(KeyCode.RIGHT, keyEvents[7].keyCode);

  // Stop blinking, get right key up.
  keyEvents = makeResultAndProcess({upperUpLeft: 0.85, upperUpRight: 0.95});
  assertEquals(9, keyEvents.length);
  assertEquals(
      chrome.accessibilityPrivate.SyntheticKeyboardEventType.KEYUP,
      keyEvents[8].type);
  assertEquals(KeyCode.RIGHT, keyEvents[8].keyCode);

  // Stop all gestures, get final left key up.
  keyEvents = makeResultAndProcess({});
  assertEquals(10, keyEvents.length);
  assertEquals(
      chrome.accessibilityPrivate.SyntheticKeyboardEventType.KEYUP,
      keyEvents[9].type);
  assertEquals(KeyCode.LEFT, keyEvents[9].keyCode);

  // Jaw open for toggle overview key press.
  keyEvents = makeResultAndProcess({jawOpen: .75});
  assertEquals(11, keyEvents.length);
  assertEquals(
      chrome.accessibilityPrivate.SyntheticKeyboardEventType.KEYDOWN,
      keyEvents[10].type);
  assertEquals(KeyCode.MEDIA_LAUNCH_APP1, keyEvents[10].keyCode);

  // Jaw close for toggle overview key release.
  keyEvents = makeResultAndProcess({});
  assertEquals(12, keyEvents.length);
  assertEquals(
      chrome.accessibilityPrivate.SyntheticKeyboardEventType.KEYUP,
      keyEvents[11].type);
  assertEquals(KeyCode.MEDIA_LAUNCH_APP1, keyEvents[11].keyCode);

  // Mouth pucker for media play/pause key press.
  keyEvents = makeResultAndProcess({mouthPucker: .75});
  assertEquals(13, keyEvents.length);
  assertEquals(
      chrome.accessibilityPrivate.SyntheticKeyboardEventType.KEYDOWN,
      keyEvents[12].type);
  assertEquals(KeyCode.MEDIA_PLAY_PAUSE, keyEvents[12].keyCode);

  // Stop mouth pucker for media play/pause key release.
  keyEvents = makeResultAndProcess({});
  assertEquals(14, keyEvents.length);
  assertEquals(
      chrome.accessibilityPrivate.SyntheticKeyboardEventType.KEYUP,
      keyEvents[13].type);
  assertEquals(KeyCode.MEDIA_PLAY_PAUSE, keyEvents[13].keyCode);
});

// TODO(b/345059065): Test is flaky.
AX_TEST_F('FaceGazeTest', 'DISABLED_ClosesCameraStream', async function() {
  await this.getFaceGaze().cameraStreamReadyPromise_;
  let win = chrome.extension.getViews().find(
      view => view.location.href.includes('camera_stream.html'));
  assertTrue(!!win);
  this.getFaceGaze().onFaceGazeDisabled();
  await this.getFaceGaze().cameraStreamClosedPromise_;
  win = chrome.extension.getViews().find(
      view => view.location.href.includes('camera_stream.html'));
  assertFalse(!!win);
});

// TODO(crbug.com/348603598): Test is flaky.
AX_TEST_F('FaceGazeTest', 'DISABLED_ToggleFaceGazeGesturesShort', async function() {
  const gestureToMacroName =
      new Map()
          .set(FacialGesture.JAW_OPEN, MacroName.TOGGLE_FACEGAZE)
          .set(FacialGesture.BROW_INNER_UP, MacroName.MOUSE_CLICK_LEFT);
  const gestureToConfidence = new Map()
                                  .set(FacialGesture.JAW_OPEN, 0.3)
                                  .set(FacialGesture.BROW_INNER_UP, 0.3);
  const config = new Config()
                     .withMouseLocation({x: 600, y: 400})
                     .withGestureToMacroName(gestureToMacroName)
                     .withGestureToConfidence(gestureToConfidence)
                     .withRepeatDelayMs(1);
  await this.configureFaceGaze(config);

  // Toggle (pause) FaceGaze.
  result = new MockFaceLandmarkerResult().addGestureWithConfidence(
      MediapipeFacialGesture.JAW_OPEN, 0.9);
  this.processFaceLandmarkerResult(
      result, /*triggerMouseControllerInterval=*/ false);
  assertTrue(this.getFaceGaze().gestureHandler_.paused_);

  // Try to perform left click.
  result =
      new MockFaceLandmarkerResult()
          .addGestureWithConfidence(MediapipeFacialGesture.JAW_OPEN, 0)
          .addGestureWithConfidence(MediapipeFacialGesture.BROW_INNER_UP, 0.9);
  this.processFaceLandmarkerResult(
      result, /*triggerMouseControllerInterval=*/ false);

  // No click should be performed.
  this.assertNumMouseEvents(0);

  // Toggle (resume) FaceGaze and release mouse click gesture.
  result =
      new MockFaceLandmarkerResult()
          .addGestureWithConfidence(MediapipeFacialGesture.JAW_OPEN, 0.9)
          .addGestureWithConfidence(MediapipeFacialGesture.BROW_INNER_UP, 0);
  this.processFaceLandmarkerResult(
      result, /*triggerMouseControllerInterval=*/ false);
  assertFalse(this.getFaceGaze().gestureHandler_.paused_);
  // No click should be performed.
  this.assertNumMouseEvents(0);

  // Perform left click now that FaceGaze has resumed.
  result =
      new MockFaceLandmarkerResult()
          .addGestureWithConfidence(MediapipeFacialGesture.JAW_OPEN, 0)
          .addGestureWithConfidence(MediapipeFacialGesture.BROW_INNER_UP, 0.9);
  this.processFaceLandmarkerResult(
      result, /*triggerMouseControllerInterval=*/ false);

  // Synthetic mouse events should have been sent.
  this.assertNumMouseEvents(2);
});

AX_TEST_F('FaceGazeTest', 'ToggleFaceGazeGesturesLong', async function() {
  const gestureToMacroName =
      new Map()
          .set(FacialGesture.JAW_OPEN, MacroName.TOGGLE_FACEGAZE)
          .set(FacialGesture.BROW_INNER_UP, MacroName.MOUSE_LONG_CLICK_LEFT)
          .set(FacialGesture.EYE_SQUINT_LEFT, MacroName.KEY_PRESS_SPACE);
  const gestureToConfidence = new Map()
                                  .set(FacialGesture.JAW_OPEN, 0.3)
                                  .set(FacialGesture.BROW_INNER_UP, 0.3)
                                  .set(FacialGesture.EYE_SQUINT_LEFT, 0.3);
  const config = new Config()
                     .withMouseLocation({x: 600, y: 400})
                     .withGestureToMacroName(gestureToMacroName)
                     .withGestureToConfidence(gestureToConfidence)
                     .withRepeatDelayMs(-1);
  await this.configureFaceGaze(config);

  // Trigger a mouse press and a key down.
  let result =
      new MockFaceLandmarkerResult()
          .addGestureWithConfidence(MediapipeFacialGesture.BROW_INNER_UP, 0.9)
          .addGestureWithConfidence(
              MediapipeFacialGesture.EYE_SQUINT_LEFT, 0.9);
  this.processFaceLandmarkerResult(
      result, /*triggerMouseControllerInterval=*/ false);

  // A synthetic mouse event should have been sent.
  this.assertNumMouseEvents(1);
  this.assertMousePress(this.getMouseEvents()[0]);

  // A synthetic key event should have been sent.
  this.assertNumKeyEvents(1);
  this.assertKeyDown(this.getKeyEvents()[0]);

  // Toggle (pause) FaceGaze in the middle of long actions.
  result =
      new MockFaceLandmarkerResult()
          .addGestureWithConfidence(MediapipeFacialGesture.JAW_OPEN, 0.9)
          .addGestureWithConfidence(MediapipeFacialGesture.BROW_INNER_UP, 0.9)
          .addGestureWithConfidence(
              MediapipeFacialGesture.EYE_SQUINT_LEFT, 0.9);
  this.processFaceLandmarkerResult(
      result, /*triggerMouseControllerInterval=*/ false);
  assertTrue(this.getFaceGaze().gestureHandler_.paused_);

  // Pausing in the middle of long actions should cause them to be completed.
  // The purpose of this is to clear state.
  this.assertNumMouseEvents(2);
  this.assertMouseRelease(this.getMouseEvents()[1]);
  this.assertNumKeyEvents(2);
  this.assertKeyUp(this.getKeyEvents()[1]);

  // Release all gestures.
  result =
      new MockFaceLandmarkerResult()
          .addGestureWithConfidence(MediapipeFacialGesture.JAW_OPEN, 0)
          .addGestureWithConfidence(MediapipeFacialGesture.BROW_INNER_UP, 0)
          .addGestureWithConfidence(MediapipeFacialGesture.EYE_SQUINT_LEFT, 0);
  this.processFaceLandmarkerResult(
      result, /*triggerMouseControllerInterval=*/ false);
  // No extra mouse or key events should have come through.
  this.assertNumMouseEvents(2);
  this.assertNumKeyEvents(2);

  // Toggle (resume) FaceGaze.
  result = new MockFaceLandmarkerResult().addGestureWithConfidence(
      MediapipeFacialGesture.JAW_OPEN, 0.9);
  this.processFaceLandmarkerResult(
      result, /*triggerMouseControllerInterval=*/ false);
  assertFalse(this.getFaceGaze().gestureHandler_.paused_);
  // No extra mouse or key events should come through.
  this.assertNumMouseEvents(2);
  this.assertNumKeyEvents(2);

  // Confirm that long actions work as expected.
  result =
      new MockFaceLandmarkerResult()
          .addGestureWithConfidence(MediapipeFacialGesture.JAW_OPEN, 0)
          .addGestureWithConfidence(MediapipeFacialGesture.BROW_INNER_UP, 0.9)
          .addGestureWithConfidence(
              MediapipeFacialGesture.EYE_SQUINT_LEFT, 0.9);
  this.processFaceLandmarkerResult(
      result, /*triggerMouseControllerInterval=*/ false);

  // A mouse press should have been sent.
  this.assertNumMouseEvents(3);
  this.assertMousePress(this.getMouseEvents()[2]);

  // A key down should have been sent.
  this.assertNumKeyEvents(3);
  this.assertKeyDown(this.getKeyEvents()[2]);

  // Toggle long click gesture again to get the mouse release event.
  // Release key gesture to get the key up events.
  result =
      new MockFaceLandmarkerResult()
          .addGestureWithConfidence(MediapipeFacialGesture.BROW_INNER_UP, 0.9)
          .addGestureWithConfidence(MediapipeFacialGesture.EYE_SQUINT_LEFT, 0);
  this.processFaceLandmarkerResult(
      result, /*triggerMouseControllerInterval=*/ false);

  // Confirm that the mouse release was sent.
  this.assertNumMouseEvents(4);
  this.assertMouseRelease(this.getMouseEvents()[3]);

  // Confirm that the key up event was sent.
  this.assertNumKeyEvents(4);
  this.assertKeyUp(this.getKeyEvents()[3]);
});

AX_TEST_F('FaceGazeTest', 'ToggleFaceGazeMouseMovement', async function() {
  const gestureToMacroName =
      new Map().set(FacialGesture.JAW_OPEN, MacroName.TOGGLE_FACEGAZE);
  const gestureToConfidence = new Map().set(FacialGesture.JAW_OPEN, 0.3);
  const config = new Config()
                     .withMouseLocation({x: 600, y: 400})
                     .withBufferSize(1)
                     .withCursorControlEnabled(true)
                     .withGestureToMacroName(gestureToMacroName)
                     .withGestureToConfidence(gestureToConfidence)
                     .withRepeatDelayMs(1);
  await this.startFacegazeWithConfigAndForeheadLocation_(config, 0.1, 0.2);

  // Move the mouse.
  let result =
      new MockFaceLandmarkerResult().setNormalizedForeheadLocation(0.11, 0.21);
  this.processFaceLandmarkerResult(result);
  this.assertLatestCursorPosition({x: 360, y: 560});

  // Toggle (pause) FaceGaze.
  result = new MockFaceLandmarkerResult().addGestureWithConfidence(
      MediapipeFacialGesture.JAW_OPEN, 0.9);
  this.processFaceLandmarkerResult(result);
  assertTrue(this.getFaceGaze().mouseController_.paused_);

  // Try to move the mouse.
  result = new MockFaceLandmarkerResult()
               .setNormalizedForeheadLocation(0.50, 0.50)
               .addGestureWithConfidence(MediapipeFacialGesture.JAW_OPEN, 0);
  this.processFaceLandmarkerResult(result);
  // Cursor position should remain the same.
  this.assertLatestCursorPosition({x: 360, y: 560});

  // Toggle (resume) FaceGaze.
  result = new MockFaceLandmarkerResult().addGestureWithConfidence(
      MediapipeFacialGesture.JAW_OPEN, 0.9);
  // Don't trigger a mouse interval here because starting the MouseController
  // is an asynchronous operation and will cause flakes otherwise.
  this.processFaceLandmarkerResult(
      result, /*triggerMouseControllerInterval=*/ false);
  assertFalse(this.getFaceGaze().mouseController_.paused_);
  // Wait for the MouseController to fully start.
  await this.waitForValidMouseInterval();

  // TODO(b/330766904): Move the mouse physically and ensure FaceGaze starts
  // from that location when it's unpaused.
  // Try to move the mouse. Since the MouseController was just freshly
  // initialized, the cursor position won't change after processing this result.
  result = new MockFaceLandmarkerResult()
               .setNormalizedForeheadLocation(0.1, 0.2)
               .addGestureWithConfidence(MediapipeFacialGesture.JAW_OPEN, 0);
  this.processFaceLandmarkerResult(result);
  this.assertLatestCursorPosition({x: 360, y: 560});

  // The second result should move the mouse.
  result = new MockFaceLandmarkerResult()
               .setNormalizedForeheadLocation(0.11, 0.21)
               .addGestureWithConfidence(MediapipeFacialGesture.JAW_OPEN, 0);
  this.processFaceLandmarkerResult(result);
  this.assertLatestCursorPosition({x: 120, y: 720});
});

AX_TEST_F('FaceGazeTest', 'KeyCombinations', async function() {
  const gestureToMacroName =
      new Map().set(FacialGesture.JAW_OPEN, MacroName.CUSTOM_KEY_COMBINATION);
  const gestureToConfidence = new Map().set(FacialGesture.JAW_OPEN, 0.7);
  const config = new Config()
                     .withMouseLocation({x: 600, y: 400})
                     .withGestureToMacroName(gestureToMacroName)
                     .withGestureToConfidence(gestureToConfidence);
  await this.configureFaceGaze(config);

  // Set the gestures to key combinations preference.
  const keyCombination = {
    key: KeyCode.C,
    modifiers: {ctrl: true},
  };
  await this.setPref(
      GestureHandler.GESTURE_TO_KEY_COMBO_PREF,
      {[FacialGesture.JAW_OPEN]: JSON.stringify(keyCombination)});

  // Verify that the preference propagated to FaceGaze.
  assertEquals(this.getFaceGaze().gestureHandler_.gesturesToKeyCombos_.size, 1);

  // Jaw open for custom key press.
  let result = new MockFaceLandmarkerResult().addGestureWithConfidence(
      MediapipeFacialGesture.JAW_OPEN, 0.9);
  this.processFaceLandmarkerResult(result);
  let keyEvents = this.getKeyEvents();

  assertEquals(keyEvents.length, 1);
  assertEquals(
      keyEvents[0].type,
      chrome.accessibilityPrivate.SyntheticKeyboardEventType.KEYDOWN);
  assertEquals(keyEvents[0].keyCode, KeyCode.C);
  assertObjectEquals(keyEvents[0].modifiers, {ctrl: true});

  // Release jaw open for custom key release.
  result = new MockFaceLandmarkerResult().addGestureWithConfidence(
      MediapipeFacialGesture.JAW_OPEN, 0.1);
  this.processFaceLandmarkerResult(result);
  keyEvents = this.getKeyEvents();

  assertEquals(keyEvents.length, 2);
  assertEquals(
      keyEvents[1].type,
      chrome.accessibilityPrivate.SyntheticKeyboardEventType.KEYUP);
  assertEquals(keyEvents[1].keyCode, KeyCode.C);
  assertObjectEquals(keyEvents[1].modifiers, {ctrl: true});
});

AX_TEST_F('FaceGazeTest', 'VelocityThreshold', async function() {
  const config = new Config()
                     .withMouseLocation({x: 600, y: 400})
                     .withBufferSize(1)
                     .withCursorControlEnabled(true)
                     .withVelocityThreshold()
                     .withSpeeds(1, 1, 1, 1);
  await this.startFacegazeWithConfigAndForeheadLocation_(config, 0.1, 0.2);
  assertNullOrUndefined(
      this.mockAccessibilityPrivate.getLatestCursorPosition());

  // Manually set the velocity threshold to 1. This means that the mouse needs
  // to move by more than one pixel before it will actually be moved.
  this.getFaceGaze().mouseController_.velocityThreshold_ = 1;

  // Small movement in head location (e.g. one pixel) doesn't trigger any
  // mouse movement.
  result = new MockFaceLandmarkerResult().setNormalizedForeheadLocation(
      0.101, 0.201);
  this.processFaceLandmarkerResult(result);
  assertNullOrUndefined(
      this.mockAccessibilityPrivate.getLatestCursorPosition());

  // Large movement triggers mouse movement.
  result =
      new MockFaceLandmarkerResult().setNormalizedForeheadLocation(0.11, 0.21);
  this.processFaceLandmarkerResult(result);
  this.assertLatestCursorPosition({x: 590, y: 406});
});

AX_TEST_F('FaceGazeTest', 'BubbleTextSimple', async function() {
  const gestureToMacroName =
      new Map().set(FacialGesture.JAW_OPEN, MacroName.MOUSE_CLICK_LEFT);
  const gestureToConfidence = new Map().set(FacialGesture.JAW_OPEN, 0.6);
  const config = new Config()
                     .withMouseLocation({x: 600, y: 400})
                     .withGestureToMacroName(gestureToMacroName)
                     .withGestureToConfidence(gestureToConfidence);
  await this.configureFaceGaze(config);

  assertNullOrUndefined(this.mockAccessibilityPrivate.getFaceGazeBubbleText());

  const result = new MockFaceLandmarkerResult().addGestureWithConfidence(
      MediapipeFacialGesture.JAW_OPEN, 0.9);
  this.processFaceLandmarkerResult(result);

  assertEquals(
      'Left-click the mouse (Open your mouth wide)',
      this.mockAccessibilityPrivate.getFaceGazeBubbleText());

  this.triggerBubbleControllerTimeout();
  assertEquals('', this.mockAccessibilityPrivate.getFaceGazeBubbleText());
});

AX_TEST_F('FaceGazeTest', 'BubbleTextMultiple', async function() {
  const gestureToMacroName =
      new Map()
          .set(FacialGesture.JAW_OPEN, MacroName.MOUSE_CLICK_LEFT)
          .set(FacialGesture.BROW_INNER_UP, MacroName.MOUSE_CLICK_RIGHT);
  const gestureToConfidence = new Map()
                                  .set(FacialGesture.JAW_OPEN, 0.6)
                                  .set(FacialGesture.BROW_INNER_UP, 0.6);
  const config = new Config()
                     .withMouseLocation({x: 600, y: 400})
                     .withGestureToMacroName(gestureToMacroName)
                     .withGestureToConfidence(gestureToConfidence);
  await this.configureFaceGaze(config);

  assertNullOrUndefined(this.mockAccessibilityPrivate.getFaceGazeBubbleText());

  const result =
      new MockFaceLandmarkerResult()
          .addGestureWithConfidence(MediapipeFacialGesture.JAW_OPEN, 0.9)
          .addGestureWithConfidence(MediapipeFacialGesture.BROW_INNER_UP, 0.9);
  this.processFaceLandmarkerResult(result);

  assertEquals(
      'Right-click the mouse (Raise eyebrows), ' +
          'Left-click the mouse (Open your mouth wide)',
      this.mockAccessibilityPrivate.getFaceGazeBubbleText());

  this.triggerBubbleControllerTimeout();
  assertEquals('', this.mockAccessibilityPrivate.getFaceGazeBubbleText());
});

AX_TEST_F('FaceGazeTest', 'ToggleFaceGazeRecognizedTime', async function() {
  const gestureToMacroName =
      new Map().set(FacialGesture.JAW_OPEN, MacroName.TOGGLE_FACEGAZE);
  const gestureToConfidence = new Map().set(FacialGesture.JAW_OPEN, 0.6);
  const config = new Config()
                     .withMouseLocation({x: 600, y: 400})
                     .withGestureToMacroName(gestureToMacroName)
                     .withGestureToConfidence(gestureToConfidence)
                     .withRepeatDelayMs(20 * 1000);
  await this.configureFaceGaze(config);

  const initialResult = new MockFaceLandmarkerResult().addGestureWithConfidence(
      MediapipeFacialGesture.JAW_OPEN, 0.9);
  this.processFaceLandmarkerResult(initialResult);
  const jawOpenTime =
      this.getFaceGaze().gestureHandler_.gestureLastRecognized_.get(
          MediapipeFacialGesture.JAW_OPEN);
  assertTrue(!!jawOpenTime);
  assertTrue(this.getFaceGaze().gestureHandler_.paused_);

  for (let i = 0; i < 5; i++) {
    const result = new MockFaceLandmarkerResult().addGestureWithConfidence(
        MediapipeFacialGesture.JAW_OPEN, 0.9);
    this.processFaceLandmarkerResult(result);

    // 6 total times in quick succession still only generates one toggle event.
    assertEquals(
        jawOpenTime,
        this.getFaceGaze().gestureHandler_.gestureLastRecognized_.get(
            MediapipeFacialGesture.JAW_OPEN));
    assertTrue(this.getFaceGaze().gestureHandler_.paused_);
  }

  // Check that we can resume after pausing.
  this.getFaceGaze().gestureHandler_.repeatDelayMs_ = -1;
  const resumeResult = new MockFaceLandmarkerResult().addGestureWithConfidence(
      MediapipeFacialGesture.JAW_OPEN, 0.9);
  this.processFaceLandmarkerResult(
      resumeResult, /*triggerMouseControllerInterval=*/ false);
  assertNotEquals(
      jawOpenTime,
      this.getFaceGaze().gestureHandler_.gestureLastRecognized_.get(
          MediapipeFacialGesture.JAW_OPEN));
  assertFalse(this.getFaceGaze().gestureHandler_.paused_);
});

AX_TEST_F('FaceGazeTest', 'BubbleTextStateMessages', async function() {
  const gestureToMacroName =
      new Map()
          .set(FacialGesture.JAW_OPEN, MacroName.TOGGLE_FACEGAZE)
          .set(FacialGesture.BROW_INNER_UP, MacroName.TOGGLE_SCROLL_MODE);
  const gestureToConfidence = new Map()
                                  .set(FacialGesture.JAW_OPEN, 0.6)
                                  .set(FacialGesture.BROW_INNER_UP, 0.6);
  const config = new Config()
                     .withMouseLocation({x: 600, y: 400})
                     .withGestureToMacroName(gestureToMacroName)
                     .withGestureToConfidence(gestureToConfidence);
  await this.configureFaceGaze(config);

  assertNullOrUndefined(this.mockAccessibilityPrivate.getFaceGazeBubbleText());

  const result =
      new MockFaceLandmarkerResult()
          .addGestureWithConfidence(MediapipeFacialGesture.JAW_OPEN, 0.9)
          .addGestureWithConfidence(MediapipeFacialGesture.BROW_INNER_UP, 0.9);
  this.processFaceLandmarkerResult(result);

  assertEquals(
      'Toggle scroll mode (Raise eyebrows), ' +
          'Pause or resume face control (Open your mouth wide)',
      this.mockAccessibilityPrivate.getFaceGazeBubbleText());

  // FaceGaze should display important messages about the state after the
  // timeout has elapsed.
  this.triggerBubbleControllerTimeout();
  assertEquals(
      'FaceGaze paused, Scroll mode active',
      this.mockAccessibilityPrivate.getFaceGazeBubbleText());
});

AX_TEST_F('FaceGazeTest', 'BubbleTextStateAndActionMessages', async function() {
  const gestureToMacroName =
      new Map()
          .set(FacialGesture.JAW_OPEN, MacroName.MOUSE_CLICK_LEFT)
          .set(FacialGesture.BROW_INNER_UP, MacroName.TOGGLE_FACEGAZE);
  const gestureToConfidence = new Map()
                                  .set(FacialGesture.JAW_OPEN, 0.6)
                                  .set(FacialGesture.BROW_INNER_UP, 0.6);
  const config = new Config()
                     .withMouseLocation({x: 600, y: 400})
                     .withGestureToMacroName(gestureToMacroName)
                     .withGestureToConfidence(gestureToConfidence)
                     .withRepeatDelayMs(1);
  await this.configureFaceGaze(config);

  assertNullOrUndefined(this.mockAccessibilityPrivate.getFaceGazeBubbleText());

  let result =
      new MockFaceLandmarkerResult()
          .addGestureWithConfidence(MediapipeFacialGesture.JAW_OPEN, 0.9)
          .addGestureWithConfidence(MediapipeFacialGesture.BROW_INNER_UP, 0.9);
  this.processFaceLandmarkerResult(result);

  assertEquals(
      'Pause or resume face control (Raise eyebrows), ' +
          'Left-click the mouse (Open your mouth wide)',
      this.mockAccessibilityPrivate.getFaceGazeBubbleText());

  // FaceGaze should display important messages about the state after the
  // timeout has elapsed.
  this.triggerBubbleControllerTimeout();
  assertEquals(
      'FaceGaze paused', this.mockAccessibilityPrivate.getFaceGazeBubbleText());

  // Send another result. Note that since FaceGaze is paused, no action
  // will be taken.
  result = new MockFaceLandmarkerResult().addGestureWithConfidence(
      MediapipeFacialGesture.JAW_OPEN, 0.9);
  this.processFaceLandmarkerResult(result);

  assertEquals(
      'FaceGaze paused', this.mockAccessibilityPrivate.getFaceGazeBubbleText());
});

AX_TEST_F('FaceGazeTest', 'TurnOffActionsWhileInScrollMode', async function() {
  const gestureToMacroName =
      new Map().set(FacialGesture.JAW_OPEN, MacroName.TOGGLE_SCROLL_MODE);
  const gestureToConfidence = new Map().set(FacialGesture.JAW_OPEN, 0.6);
  const config = new Config()
                     .withMouseLocation({x: 600, y: 400})
                     .withBufferSize(1)
                     .withCursorControlEnabled(true)
                     .withGestureToMacroName(gestureToMacroName)
                     .withGestureToConfidence(gestureToConfidence);
  await this.startFacegazeWithConfigAndForeheadLocation_(config, 0.1, 0.2);

  // Toggle scroll mode on.
  const result = new MockFaceLandmarkerResult().addGestureWithConfidence(
      MediapipeFacialGesture.JAW_OPEN, 0.9);
  this.processFaceLandmarkerResult(result);
  assertTrue(this.getScrollModeController().active());

  this.triggerBubbleControllerTimeout();
  assertEquals(
      'Scroll mode active',
      this.mockAccessibilityPrivate.getFaceGazeBubbleText());

  // Turn off actions via pref.
  await this.setPref(FaceGaze.PREF_ACTIONS_ENABLED, false);

  // Ensure scroll mode automatically toggled off.
  assertFalse(this.getScrollModeController().active());
  assertEquals('', this.mockAccessibilityPrivate.getFaceGazeBubbleText());
});

AX_TEST_F(
    'FaceGazeTest', 'RemoveScrollModeActionWhileInScrollMode',
    async function() {
      const gestureToMacroName =
          new Map().set(FacialGesture.JAW_OPEN, MacroName.TOGGLE_SCROLL_MODE);
      const gestureToConfidence = new Map().set(FacialGesture.JAW_OPEN, 0.6);
      const config = new Config()
                         .withMouseLocation({x: 600, y: 400})
                         .withBufferSize(1)
                         .withCursorControlEnabled(true)
                         .withGestureToMacroName(gestureToMacroName)
                         .withGestureToConfidence(gestureToConfidence);
      await this.startFacegazeWithConfigAndForeheadLocation_(config, 0.1, 0.2);

      // Toggle scroll mode on.
      const result = new MockFaceLandmarkerResult().addGestureWithConfidence(
          MediapipeFacialGesture.JAW_OPEN, 0.9);
      this.processFaceLandmarkerResult(result);
      assertTrue(this.getScrollModeController().active());

      this.triggerBubbleControllerTimeout();
      assertEquals(
          'Scroll mode active',
          this.mockAccessibilityPrivate.getFaceGazeBubbleText());

      // Remove scroll mode action.
      await this.setPref(GestureHandler.GESTURE_TO_MACRO_PREF, {});

      // Ensure scroll mode automatically toggled off.
      assertFalse(this.getScrollModeController().active());
      assertEquals('', this.mockAccessibilityPrivate.getFaceGazeBubbleText());
    });

AX_TEST_F('FaceGazeTest', 'GesturesDisabledInScrollMode', async function() {
  const gestureToMacroName =
      new Map()
          .set(FacialGesture.JAW_OPEN, MacroName.TOGGLE_SCROLL_MODE)
          .set(FacialGesture.MOUTH_PUCKER, MacroName.MOUSE_CLICK_LEFT);
  const gestureToConfidence = new Map()
                                  .set(FacialGesture.JAW_OPEN, 0.3)
                                  .set(FacialGesture.MOUTH_PUCKER, 0.3);
  const config = new Config()
                     .withMouseLocation({x: 600, y: 400})
                     .withBufferSize(1)
                     .withCursorControlEnabled(false)
                     .withGestureToMacroName(gestureToMacroName)
                     .withGestureToConfidence(gestureToConfidence)
                     .withRepeatDelayMs(-1);
  await this.startFacegazeWithConfigAndForeheadLocation_(config, 0.1, 0.2);

  // Set the mouse position with an automation event.
  this.sendAutomationMouseEvent({mouseX: 350, mouseY: 250, eventFrom: 'user'});

  this.assertNumMouseEvents(0);

  // Turn on scroll mode.
  result = new MockFaceLandmarkerResult().addGestureWithConfidence(
      MediapipeFacialGesture.JAW_OPEN, 0.9);
  this.processFaceLandmarkerResult(result);
  assertTrue(this.getScrollModeController().active());
  this.assertNumMouseEvents(0);

  // Try to click the mouse. No additional mouse events should have been fired
  // because gestures are blocked while scroll mode is active.
  result =
      new MockFaceLandmarkerResult()
          .addGestureWithConfidence(MediapipeFacialGesture.JAW_OPEN, 0)
          .addGestureWithConfidence(MediapipeFacialGesture.MOUTH_PUCKER, 0.9);
  this.processFaceLandmarkerResult(result);
  this.assertNumMouseEvents(0);

  // Turn off scroll mode.
  result =
      new MockFaceLandmarkerResult()
          .addGestureWithConfidence(MediapipeFacialGesture.JAW_OPEN, 0.9)
          .addGestureWithConfidence(MediapipeFacialGesture.MOUTH_PUCKER, 0);
  this.processFaceLandmarkerResult(result);
  assertFalse(this.getScrollModeController().active());

  // Ensure the mouse can be clicked using a gesture.
  result =
      new MockFaceLandmarkerResult()
          .addGestureWithConfidence(MediapipeFacialGesture.JAW_OPEN, 0)
          .addGestureWithConfidence(MediapipeFacialGesture.MOUTH_PUCKER, 0.9);
  this.processFaceLandmarkerResult(result);
  this.assertNumMouseEvents(2);
  const pressEvent = this.getMouseEvents()[0];
  const releaseEvent = this.getMouseEvents()[1];
  this.assertMouseClickAt(
      {pressEvent, releaseEvent, isLeft: true, x: 350, y: 250});
});
