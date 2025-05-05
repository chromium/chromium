// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

GEN_INCLUDE(['facegaze_test_base.js']);

FaceGazeMV2Test = class extends FaceGazeTestBase {
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
    'FaceGazeMV2Test', 'FacialGesturesInFacialGesturesToMediapipeGestures',
    () => {
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
    'FaceGazeMV2Test',
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
    'FaceGazeMV2Test',
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
                         .withBindings(gestureToMacroName, gestureToConfidence);
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
    'FaceGazeMV2Test',
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
                         .withBindings(gestureToMacroName, gestureToConfidence);
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

AX_TEST_F(
    'FaceGazeMV2Test', 'IntervalReusesForeheadLocation', async function() {
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

AX_TEST_F(
    'FaceGazeMV2Test', 'CursorPositionUpdatedOnInterval', async function() {
      const config =
          new Config().withMouseLocation({x: 600, y: 400}).withBufferSize(1);
      await this.startFacegazeWithConfigAndForeheadLocation_(config, 0.1, 0.2);

      const result =
          new MockFaceLandmarkerResult().setNormalizedForeheadLocation(
              0.2, 0.4);
      this.processFaceLandmarkerResult(
          result, /*triggerMouseControllerInterval=*/ false);

      // Cursor position doesn't change on result.
      this.assertLatestCursorPosition({x: 600, y: 400});

      // Cursor position does change after interval fired.
      this.triggerMouseControllerInterval();
      const cursorPosition = this.getLatestCursorPosition();
      assertNotEquals(600, cursorPosition.x);
      assertNotEquals(400, cursorPosition.y);
    });

AX_TEST_F('FaceGazeMV2Test', 'UpdateMouseLocation', async function() {
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
    'FaceGazeMV2Test', 'UpdatesMousePositionOnlyWhenCursorControlEnabled',
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

      assertEquals(null, this.getLatestCursorPosition());

      // Try moving back. Still nothing.
      result = new MockFaceLandmarkerResult().setNormalizedForeheadLocation(
          0.1, 0.2);
      this.processFaceLandmarkerResult(result);
      assertEquals(null, this.getLatestCursorPosition());

      // Turn on cursor control.
      await this.setPref(PrefNames.CURSOR_CONTROL_ENABLED, true);

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
      await this.setPref(PrefNames.CURSOR_CONTROL_ENABLED, false);
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
    'FaceGazeMV2Test', 'UpdateMouseLocationFromDifferentForeheadLocation',
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
    'FaceGazeMV2Test', 'UpdateMouseLocationWithScreenNotAtZero',
    async function() {
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

AX_TEST_F('FaceGazeMV2Test', 'UpdateMouseLocationWithBuffer', async function() {
  const config =
      new Config().withMouseLocation({x: 600, y: 400}).withBufferSize(6);
  await this.startFacegazeWithConfigAndForeheadLocation_(config, 0.1, 0.2);

  // Move left and down. Note that increasing the x coordinate results in
  // moving left because the image is mirrored.
  let result =
      new MockFaceLandmarkerResult().setNormalizedForeheadLocation(0.11, 0.21);
  this.processFaceLandmarkerResult(result);
  let cursorPosition = this.getLatestCursorPosition();
  assertTrue(cursorPosition.x < 600);
  assertTrue(cursorPosition.y > 400);

  // Move right and up. Due to smoothing, we don't exactly reach (600,400)
  // again, but do get closer to it.
  result =
      new MockFaceLandmarkerResult().setNormalizedForeheadLocation(0.1, 0.2);
  this.processFaceLandmarkerResult(result);
  let newCursorPosition = this.getLatestCursorPosition();
  assertTrue(newCursorPosition.x > cursorPosition.x);
  assertTrue(newCursorPosition.y < cursorPosition.y);
  assertTrue(newCursorPosition.x < 600);
  assertTrue(newCursorPosition.y > 400);

  cursorPosition = newCursorPosition;
  // Process the same result again. We move even closer to (600, 400).
  this.processFaceLandmarkerResult(result);
  newCursorPosition = this.getLatestCursorPosition();
  assertTrue(newCursorPosition.x > cursorPosition.x);
  assertTrue(newCursorPosition.y < cursorPosition.y);
  assertTrue(newCursorPosition.x < 600);
  assertTrue(newCursorPosition.y > 400);
});

AX_TEST_F(
    'FaceGazeMV2Test', 'UpdateMouseLocationWithSpeed1Move1', async function() {
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
    'FaceGazeMV2Test', 'UpdateMouseLocationWithSpeed1Move5', async function() {
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
    'FaceGazeMV2Test', 'UpdateMouseLocationWithSpeed1Move20', async function() {
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
    'FaceGazeMV2Test', 'UpdateMouseLocationWithAccelerationMove1',
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
    'FaceGazeMV2Test', 'UpdateMouseLocationWithAccelerationMove5',
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
    'FaceGazeMV2Test', 'UpdateMouseLocationWithAccelerationMove10',
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
        const initialCursorPosition = this.getLatestCursorPosition();
        const result =
            new MockFaceLandmarkerResult().setNormalizedForeheadLocation(
                xLocation, yLocation);
        this.processFaceLandmarkerResult(result);
        const cursorPosition = this.getLatestCursorPosition();
        assertEquals(-10, cursorPosition.x - initialCursorPosition.x);
        assertEquals(10, cursorPosition.y - initialCursorPosition.y);
      }
    });

AX_TEST_F(
    'FaceGazeMV2Test', 'UpdateMouseLocationWithAccelerationMove20',
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
        const initialCursorPosition = this.getLatestCursorPosition();
        const result =
            new MockFaceLandmarkerResult().setNormalizedForeheadLocation(
                xLocation, yLocation);
        this.processFaceLandmarkerResult(result);
        const cursorPosition = this.getLatestCursorPosition();
        assertEquals(-24, cursorPosition.x - initialCursorPosition.x);
        assertEquals(24, cursorPosition.y - initialCursorPosition.y);
      }
    });

AX_TEST_F(
    'FaceGazeMV2Test', 'DetectGesturesAndPerformShortActions',
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
                         .withBindings(gestureToMacroName, gestureToConfidence);
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
    'FaceGazeMV2Test', 'DetectGesturesAndPerformLongActions', async function() {
      const gestureToMacroName =
          new Map()
              .set(FacialGesture.JAW_OPEN, MacroName.MOUSE_LONG_CLICK_LEFT)
              .set(FacialGesture.BROW_INNER_UP, MacroName.MOUSE_CLICK_RIGHT);
      const gestureToConfidence = new Map()
                                      .set(FacialGesture.JAW_OPEN, 0.6)
                                      .set(FacialGesture.BROW_INNER_UP, 0.6);
      const config = new Config()
                         .withMouseLocation({x: 600, y: 400})
                         .withBindings(gestureToMacroName, gestureToConfidence)
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
    'FaceGazeMV2Test', 'SendMouseMoveFromCursorControlDuringLongClick',
    async function() {
      const gestureToMacroName = new Map().set(
          FacialGesture.JAW_OPEN, MacroName.MOUSE_LONG_CLICK_LEFT);
      const gestureToConfidence = new Map().set(FacialGesture.JAW_OPEN, 0.6);
      const config = new Config()
                         .withMouseLocation({x: 600, y: 400})
                         .withBindings(gestureToMacroName, gestureToConfidence)
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

      // Move the cursor to trigger move event. Cursor control will send another
      // two synthetic mouse events.
      result = new MockFaceLandmarkerResult().setNormalizedForeheadLocation(
          0.11, 0.21);
      this.processFaceLandmarkerResult(result);
      this.triggerMouseControllerInterval();
      this.assertLatestCursorPosition({x: 360, y: 560});

      this.assertNumMouseEvents(5);
      let moveEvent = this.getMouseEvents()[3];
      assertEquals(
          this.mockAccessibilityPrivate.SyntheticMouseEventType.MOVE,
          moveEvent.type);
      assertEquals(
          this.mockAccessibilityPrivate.SyntheticMouseEventButton.LEFT,
          moveEvent.mouseButton);
      assertEquals(360, moveEvent.x);
      assertEquals(560, moveEvent.y);
      moveEvent = this.getMouseEvents()[4];
      assertEquals(
          this.mockAccessibilityPrivate.SyntheticMouseEventType.MOVE,
          moveEvent.type);
      assertEquals(
          this.mockAccessibilityPrivate.SyntheticMouseEventButton.LEFT,
          moveEvent.mouseButton);
      assertEquals(360, moveEvent.x);
      assertEquals(560, moveEvent.y);

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
    'FaceGazeMV2Test', 'SendMouseMoveFromUserDuringLongClick',
    async function() {
      const gestureToMacroName = new Map().set(
          FacialGesture.JAW_OPEN, MacroName.MOUSE_LONG_CLICK_LEFT);
      const gestureToConfidence = new Map().set(FacialGesture.JAW_OPEN, 0.6);
      const config = new Config()
                         .withMouseLocation({x: 600, y: 400})
                         .withBindings(gestureToMacroName, gestureToConfidence)
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

      // Move the cursor to trigger move event.
      this.sendAutomationMouseEvent(
          {mouseX: 360, mouseY: 560, eventFrom: 'user'});

      this.assertNumMouseEvents(2);
      const moveEvent = this.getMouseEvents()[1];
      assertEquals(
          this.mockAccessibilityPrivate.SyntheticMouseEventType.MOVE,
          moveEvent.type);
      assertEquals(
          this.mockAccessibilityPrivate.SyntheticMouseEventButton.LEFT,
          moveEvent.mouseButton);
      assertEquals(360, moveEvent.x);
      assertEquals(560, moveEvent.y);

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

AX_TEST_F(
    'FaceGazeMV2Test', 'TurnOffActionsWhileInMiddleOfLongClick',
    async function() {
      const gestureToMacroName = new Map().set(
          FacialGesture.JAW_OPEN, MacroName.MOUSE_LONG_CLICK_LEFT);
      const gestureToConfidence = new Map().set(FacialGesture.JAW_OPEN, 0.6);
      const config = new Config()
                         .withMouseLocation({x: 600, y: 400})
                         .withBindings(gestureToMacroName, gestureToConfidence)
                         .withRepeatDelayMs(0);
      await this.configureFaceGaze(config);

      // Toggle long click.
      const result = new MockFaceLandmarkerResult().addGestureWithConfidence(
          MediapipeFacialGesture.JAW_OPEN, 0.9);
      this.processFaceLandmarkerResult(result);
      assertTrue(this.getMouseController().isLongClickActive());

      // Remove long click action.
      await this.setPref(PrefNames.ACTIONS_ENABLED, false);

      // Ensure long click automatically toggled off.
      assertFalse(this.getMouseController().isLongClickActive());
    });

AX_TEST_F(
    'FaceGazeMV2Test', 'RemoveLongClickActionWhileInMiddleOfLongClick',
    async function() {
      const gestureToMacroName = new Map().set(
          FacialGesture.JAW_OPEN, MacroName.MOUSE_LONG_CLICK_LEFT);
      const gestureToConfidence = new Map().set(FacialGesture.JAW_OPEN, 0.6);
      const config = new Config()
                         .withMouseLocation({x: 600, y: 400})
                         .withBindings(gestureToMacroName, gestureToConfidence)
                         .withRepeatDelayMs(0);
      await this.configureFaceGaze(config);

      // Toggle long click.
      const result = new MockFaceLandmarkerResult().addGestureWithConfidence(
          MediapipeFacialGesture.JAW_OPEN, 0.9);
      this.processFaceLandmarkerResult(result);
      assertTrue(this.getMouseController().isLongClickActive());

      // Remove long click action.
      await this.setPref(PrefNames.GESTURE_TO_MACRO, {});

      // Ensure long click automatically toggled off.
      assertFalse(this.getMouseController().isLongClickActive());
    });

// The BrowDown gesture is special because it is the combination of two
// separate facial gestures. This test ensures that the associated action is
// performed if either of the gestures is detected.
AX_TEST_F('FaceGazeMV2Test', 'BrowDownGesture', async function() {
  const gestureToMacroName =
      new Map().set(FacialGesture.BROWS_DOWN, MacroName.RESET_CURSOR);
  const gestureToConfidence = new Map().set(FacialGesture.BROWS_DOWN, 0.6);
  const config = new Config()
                     .withMouseLocation({x: 0, y: 0})
                     .withBindings(gestureToMacroName, gestureToConfidence);
  await this.configureFaceGaze(config);
  this.mockAccessibilityPrivate.clearCursorPosition();

  let result =
      new MockFaceLandmarkerResult()
          .addGestureWithConfidence(MediapipeFacialGesture.BROW_DOWN_LEFT, 0.3)
          .addGestureWithConfidence(
              MediapipeFacialGesture.BROW_DOWN_RIGHT, 0.3);
  this.processFaceLandmarkerResult(result);
  assertEquals(null, this.getLatestCursorPosition());

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
    'FaceGazeMV2Test', 'DoesNotPerformActionsWhenActionsDisabled',
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
                         .withBindings(gestureToMacroName, gestureToConfidence)
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
      await this.setPref(PrefNames.ACTIONS_ENABLED, true);

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
    'FaceGazeMV2Test', 'ActionsUseMouseLocationWhenCursorControlDisabled',
    async function() {
      const gestureToMacroName = new Map().set(
          FacialGesture.MOUTH_PUCKER, MacroName.MOUSE_CLICK_RIGHT);
      const gestureToConfidence =
          new Map().set(FacialGesture.MOUTH_PUCKER, 0.5);
      const config = new Config()
                         .withMouseLocation({x: 600, y: 400})
                         .withBindings(gestureToMacroName, gestureToConfidence)
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

AX_TEST_F('FaceGazeMV2Test', 'DoesNotRepeatGesturesTooSoon', async function() {
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
                     .withBindings(gestureToMacroName, gestureToConfidence)
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

  this.setGestureRepeatDelay(0);

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

  this.setGestureRepeatDelay(1000);

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

AX_TEST_F('FaceGazeMV2Test', 'DoesNotClickDuringLongClick', async function() {
  const gestureToMacroName =
      new Map()
          .set(FacialGesture.MOUTH_PUCKER, MacroName.MOUSE_LONG_CLICK_LEFT)
          .set(FacialGesture.EYE_SQUINT_LEFT, MacroName.MOUSE_CLICK_LEFT)
          .set(FacialGesture.EYE_SQUINT_RIGHT, MacroName.MOUSE_CLICK_RIGHT);
  const gestureToConfidence = new Map()
                                  .set(FacialGesture.MOUTH_PUCKER, 0.6)
                                  .set(FacialGesture.EYE_SQUINT_LEFT, 0.6)
                                  .set(FacialGesture.EYE_SQUINT_RIGHT, 0.6);
  const config = new Config()
                     .withMouseLocation({x: 600, y: 400})
                     .withBindings(gestureToMacroName, gestureToConfidence)
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

AX_TEST_F('FaceGazeMV2Test', 'KeyEvents', async function() {
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
                     .withBindings(gestureToMacroName, gestureToConfidence)
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

// TODO(crbug.com/348603598): Test is flaky.
AX_TEST_F(
    'FaceGazeMV2Test', 'DISABLED_ToggleFaceGazeGesturesShort',
    async function() {
      const gestureToMacroName =
          new Map()
              .set(FacialGesture.JAW_OPEN, MacroName.TOGGLE_FACEGAZE)
              .set(FacialGesture.BROW_INNER_UP, MacroName.MOUSE_CLICK_LEFT);
      const gestureToConfidence = new Map()
                                      .set(FacialGesture.JAW_OPEN, 0.3)
                                      .set(FacialGesture.BROW_INNER_UP, 0.3);
      const config = new Config()
                         .withMouseLocation({x: 600, y: 400})
                         .withBindings(gestureToMacroName, gestureToConfidence)
                         .withRepeatDelayMs(1);
      await this.configureFaceGaze(config);

      // Toggle (pause) FaceGaze.
      result = new MockFaceLandmarkerResult().addGestureWithConfidence(
          MediapipeFacialGesture.JAW_OPEN, 0.9);
      this.processFaceLandmarkerResult(
          result, /*triggerMouseControllerInterval=*/ false);
      assertTrue(this.getGestureHandler().paused_);

      // Try to perform left click.
      result = new MockFaceLandmarkerResult()
                   .addGestureWithConfidence(MediapipeFacialGesture.JAW_OPEN, 0)
                   .addGestureWithConfidence(
                       MediapipeFacialGesture.BROW_INNER_UP, 0.9);
      this.processFaceLandmarkerResult(
          result, /*triggerMouseControllerInterval=*/ false);

      // No click should be performed.
      this.assertNumMouseEvents(0);

      // Toggle (resume) FaceGaze and release mouse click gesture.
      result =
          new MockFaceLandmarkerResult()
              .addGestureWithConfidence(MediapipeFacialGesture.JAW_OPEN, 0.9)
              .addGestureWithConfidence(
                  MediapipeFacialGesture.BROW_INNER_UP, 0);
      this.processFaceLandmarkerResult(
          result, /*triggerMouseControllerInterval=*/ false);
      assertFalse(this.getGestureHandler().paused_);
      // No click should be performed.
      this.assertNumMouseEvents(0);

      // Perform left click now that FaceGaze has resumed.
      result = new MockFaceLandmarkerResult()
                   .addGestureWithConfidence(MediapipeFacialGesture.JAW_OPEN, 0)
                   .addGestureWithConfidence(
                       MediapipeFacialGesture.BROW_INNER_UP, 0.9);
      this.processFaceLandmarkerResult(
          result, /*triggerMouseControllerInterval=*/ false);

      // Synthetic mouse events should have been sent.
      this.assertNumMouseEvents(2);
    });

AX_TEST_F('FaceGazeMV2Test', 'ToggleFaceGazeGesturesLong', async function() {
  const gestureToMacroName =
      new Map()
          .set(FacialGesture.BROW_INNER_UP, MacroName.MOUSE_LONG_CLICK_LEFT)
          .set(FacialGesture.EYE_SQUINT_LEFT, MacroName.KEY_PRESS_SPACE);
  const gestureToConfidence = new Map()
                                  .set(FacialGesture.BROW_INNER_UP, 0.3)
                                  .set(FacialGesture.EYE_SQUINT_LEFT, 0.3);
  const config = new Config()
                     .withMouseLocation({x: 600, y: 400})
                     .withBindings(gestureToMacroName, gestureToConfidence)
                     .withRepeatDelayMs(-1);
  await this.configureFaceGaze(config);

  // Trigger a key down.
  let result = new MockFaceLandmarkerResult().addGestureWithConfidence(
      MediapipeFacialGesture.EYE_SQUINT_LEFT, 0.9);
  this.processFaceLandmarkerResult(
      result, /*triggerMouseControllerInterval=*/ false);

  // A synthetic key event should have been sent.
  this.assertNumKeyEvents(1);
  this.assertKeyDown(this.getKeyEvents()[0]);

  // Trigger a mouse press.
  result = new MockFaceLandmarkerResult().addGestureWithConfidence(
      MediapipeFacialGesture.BROW_INNER_UP, 0.9);
  this.processFaceLandmarkerResult(
      result, /*triggerMouseControllerInterval=*/ false);

  // A synthetic mouse event should have been sent.
  this.assertNumMouseEvents(1);
  this.assertMousePress(this.getMouseEvents()[0]);

  // Stop FaceGaze in the middle of long actions.
  this.getMouseController().stop();
  this.getGestureHandler().stop();

  // Stopping in the middle of long actions should cause them to be completed.
  // The purpose of this is to clear state.
  this.assertNumMouseEvents(2);
  this.assertMouseRelease(this.getMouseEvents()[1]);
  this.assertNumKeyEvents(2);
  this.assertKeyUp(this.getKeyEvents()[1]);

  // Release all gestures.
  result =
      new MockFaceLandmarkerResult()
          .addGestureWithConfidence(MediapipeFacialGesture.BROW_INNER_UP, 0)
          .addGestureWithConfidence(MediapipeFacialGesture.EYE_SQUINT_LEFT, 0);
  this.processFaceLandmarkerResult(
      result, /*triggerMouseControllerInterval=*/ false);
  // No extra mouse or key events should have come through.
  this.assertNumMouseEvents(2);
  this.assertNumKeyEvents(2);

  // Resume FaceGaze.
  this.getMouseController().start();
  this.getGestureHandler().start();

  // No extra mouse or key events should come through.
  this.assertNumMouseEvents(2);
  this.assertNumKeyEvents(2);

  // Confirm that long actions work as expected.
  result = new MockFaceLandmarkerResult().addGestureWithConfidence(
      MediapipeFacialGesture.EYE_SQUINT_LEFT, 0.9);
  this.processFaceLandmarkerResult(
      result, /*triggerMouseControllerInterval=*/ false);

  // A key down should have been sent.
  this.assertNumKeyEvents(3);
  this.assertKeyDown(this.getKeyEvents()[2]);

  result = new MockFaceLandmarkerResult().addGestureWithConfidence(
      MediapipeFacialGesture.BROW_INNER_UP, 0.9);
  this.processFaceLandmarkerResult(
      result, /*triggerMouseControllerInterval=*/ false);

  // A mouse press should have been sent.
  this.assertNumMouseEvents(3);
  this.assertMousePress(this.getMouseEvents()[2]);

  // Toggle long click gesture again to get the mouse release event.
  // Release key gesture to get the key up events.
  result = new MockFaceLandmarkerResult().addGestureWithConfidence(
      MediapipeFacialGesture.BROW_INNER_UP, 0.9);
  this.processFaceLandmarkerResult(
      result, /*triggerMouseControllerInterval=*/ false);

  result = new MockFaceLandmarkerResult().addGestureWithConfidence(
      MediapipeFacialGesture.EYE_SQUINT_LEFT, 0.0);
  this.processFaceLandmarkerResult(
      result, /*triggerMouseControllerInterval=*/ false);

  // Confirm that the mouse release was sent.
  this.assertNumMouseEvents(4);
  this.assertMouseRelease(this.getMouseEvents()[3]);

  // Confirm that the key up event was sent.
  this.assertNumKeyEvents(4);
  this.assertKeyUp(this.getKeyEvents()[3]);
});

AX_TEST_F('FaceGazeMV2Test', 'ToggleFaceGazeMouseMovement', async function() {
  const gestureToMacroName =
      new Map().set(FacialGesture.JAW_OPEN, MacroName.TOGGLE_FACEGAZE);
  const gestureToConfidence = new Map().set(FacialGesture.JAW_OPEN, 0.3);
  const config = new Config()
                     .withMouseLocation({x: 600, y: 400})
                     .withBufferSize(1)
                     .withCursorControlEnabled(true)
                     .withBindings(gestureToMacroName, gestureToConfidence)
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
  assertTrue(this.getMouseController().paused_);

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
  assertFalse(this.getMouseController().paused_);
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

AX_TEST_F('FaceGazeMV2Test', 'KeyCombinations', async function() {
  const gestureToMacroName =
      new Map().set(FacialGesture.JAW_OPEN, MacroName.CUSTOM_KEY_COMBINATION);
  const gestureToConfidence = new Map().set(FacialGesture.JAW_OPEN, 0.7);
  const config = new Config()
                     .withMouseLocation({x: 600, y: 400})
                     .withBindings(gestureToMacroName, gestureToConfidence);
  await this.configureFaceGaze(config);

  // Set the gestures to key combinations preference.
  const keyCombination = {
    key: KeyCode.C,
    keyDisplay: 'c',
    modifiers: {ctrl: true},
  };
  await this.setPref(
      PrefNames.GESTURE_TO_KEY_COMBO,
      {[FacialGesture.JAW_OPEN]: JSON.stringify(keyCombination)});

  // Verify that the preference propagated to FaceGaze.
  assertEquals(this.getGestureHandler().gesturesToKeyCombos_.size, 1);

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

AX_TEST_F('FaceGazeMV2Test', 'KeyCombinationsRepeat', async function() {
  const gestureToMacroName =
      new Map().set(FacialGesture.JAW_OPEN, MacroName.CUSTOM_KEY_COMBINATION);
  const gestureToConfidence = new Map().set(FacialGesture.JAW_OPEN, 0.7);
  const config = new Config()
                     .withMouseLocation({x: 600, y: 400})
                     .withBindings(gestureToMacroName, gestureToConfidence);
  await this.configureFaceGaze(config);

  // Set the gestures to key combinations preference.
  const keyCombination = {
    key: KeyCode.V,
    keyDisplay: 'v',
    modifiers: {ctrl: true},
  };
  await this.setPref(
      PrefNames.GESTURE_TO_KEY_COMBO,
      {[FacialGesture.JAW_OPEN]: JSON.stringify(keyCombination)});

  // Verify that the preference propagated to FaceGaze.
  assertEquals(this.getGestureHandler().gesturesToKeyCombos_.size, 1);

  // Jaw open for custom key press.
  let result = new MockFaceLandmarkerResult().addGestureWithConfidence(
      MediapipeFacialGesture.JAW_OPEN, 0.9);
  this.processFaceLandmarkerResult(result);
  let keyEvents = this.getKeyEvents();

  // Check the first event.
  this.assertNumKeyEvents(1);
  assertEquals(
      keyEvents[0].type,
      chrome.accessibilityPrivate.SyntheticKeyboardEventType.KEYDOWN);
  assertEquals(keyEvents[0].keyCode, KeyCode.V);
  assertObjectEquals(keyEvents[0].modifiers, {ctrl: true});
  assertFalse(keyEvents[0].repeat);

  // Manually check and call the last setInterval callback, which should have
  // been the one just set by KeyPressMacro.
  assertNotEquals(this.intervalCallbacks_.length, 0);
  assertNotNullNorUndefined(
      this.intervalCallbacks_[this.intervalCallbacks_.length - 1]);
  this.intervalCallbacks_[this.intervalCallbacks_.length - 1]();

  keyEvents = this.getKeyEvents();

  // Additional event should have been fired.
  this.assertNumKeyEvents(2);
  assertEquals(
      keyEvents[1].type,
      chrome.accessibilityPrivate.SyntheticKeyboardEventType.KEYDOWN);
  assertEquals(keyEvents[1].keyCode, KeyCode.V);
  assertObjectEquals(keyEvents[1].modifiers, {ctrl: true});
  assertTrue(keyEvents[1].repeat);

  // Release jaw open for custom key release.
  result = new MockFaceLandmarkerResult().addGestureWithConfidence(
      MediapipeFacialGesture.JAW_OPEN, 0.1);
  this.processFaceLandmarkerResult(result);

  this.assertNumKeyEvents(3);
  assertEquals(
      keyEvents[2].type,
      chrome.accessibilityPrivate.SyntheticKeyboardEventType.KEYUP);
  assertEquals(keyEvents[2].keyCode, KeyCode.V);
  assertObjectEquals(keyEvents[2].modifiers, {ctrl: true});
  assertNullOrUndefined(keyEvents[2].repeat);
});

AX_TEST_F('FaceGazeMV2Test', 'VelocityThreshold', async function() {
  const config = new Config()
                     .withMouseLocation({x: 600, y: 400})
                     .withBufferSize(1)
                     .withCursorControlEnabled(true)
                     .withVelocityThreshold()
                     .withSpeeds(1, 1, 1, 1);
  await this.startFacegazeWithConfigAndForeheadLocation_(config, 0.1, 0.2);
  assertNullOrUndefined(this.getLatestCursorPosition());

  // Manually set the velocity threshold to 1. This means that the mouse needs
  // to move by more than one pixel before it will actually be moved.
  this.getMouseController().velocityThreshold_ = 1;

  // Small movement in head location (e.g. one pixel) doesn't trigger any
  // mouse movement.
  result = new MockFaceLandmarkerResult().setNormalizedForeheadLocation(
      0.101, 0.201);
  this.processFaceLandmarkerResult(result);
  assertNullOrUndefined(this.getLatestCursorPosition());

  // Large movement triggers mouse movement.
  result =
      new MockFaceLandmarkerResult().setNormalizedForeheadLocation(0.11, 0.21);
  this.processFaceLandmarkerResult(result);
  this.assertLatestCursorPosition({x: 590, y: 406});
});

AX_TEST_F('FaceGazeMV2Test', 'BubbleTextSimple', async function() {
  const gestureToMacroName =
      new Map().set(FacialGesture.JAW_OPEN, MacroName.MOUSE_CLICK_LEFT);
  const gestureToConfidence = new Map().set(FacialGesture.JAW_OPEN, 0.6);
  const config = new Config()
                     .withMouseLocation({x: 600, y: 400})
                     .withBindings(gestureToMacroName, gestureToConfidence);
  await this.configureFaceGaze(config);

  assertNullOrUndefined(this.getBubbleText());

  const result = new MockFaceLandmarkerResult().addGestureWithConfidence(
      MediapipeFacialGesture.JAW_OPEN, 0.9);
  this.processFaceLandmarkerResult(result);

  assertEquals(
      'Left-click the mouse (Open your mouth wide)', this.getBubbleText());
  assertFalse(this.getBubbleIsWarning());

  this.triggerBubbleControllerTimeout();
  assertEquals(this.getDefaultBubbleText(), this.getBubbleText());
});

AX_TEST_F('FaceGazeMV2Test', 'BubbleTextMultiple', async function() {
  const gestureToMacroName =
      new Map()
          .set(FacialGesture.JAW_OPEN, MacroName.MOUSE_CLICK_LEFT)
          .set(FacialGesture.BROW_INNER_UP, MacroName.MOUSE_CLICK_RIGHT);
  const gestureToConfidence = new Map()
                                  .set(FacialGesture.JAW_OPEN, 0.6)
                                  .set(FacialGesture.BROW_INNER_UP, 0.6);
  const config = new Config()
                     .withMouseLocation({x: 600, y: 400})
                     .withBindings(gestureToMacroName, gestureToConfidence);
  await this.configureFaceGaze(config);

  assertNullOrUndefined(this.getBubbleText());

  const result =
      new MockFaceLandmarkerResult()
          .addGestureWithConfidence(MediapipeFacialGesture.JAW_OPEN, 0.9)
          .addGestureWithConfidence(MediapipeFacialGesture.BROW_INNER_UP, 0.9);
  this.processFaceLandmarkerResult(result);

  assertEquals(
      'Right-click the mouse (Raise eyebrows), ' +
          'Left-click the mouse (Open your mouth wide)',
      this.getBubbleText());
  assertFalse(this.getBubbleIsWarning());

  this.triggerBubbleControllerTimeout();
  assertEquals(this.getDefaultBubbleText(), this.getBubbleText());
});

AX_TEST_F('FaceGazeMV2Test', 'BubbleTextKeyCombination', async function() {
  const gestureToMacroName =
      new Map().set(FacialGesture.JAW_OPEN, MacroName.CUSTOM_KEY_COMBINATION);
  const gestureToConfidence = new Map().set(FacialGesture.JAW_OPEN, 0.7);
  const config = new Config()
                     .withMouseLocation({x: 600, y: 400})
                     .withBindings(gestureToMacroName, gestureToConfidence);
  await this.configureFaceGaze(config);

  assertNullOrUndefined(this.getBubbleText());

  // Set the gestures to key combinations preference.
  const keyCombination = {
    key: KeyCode.C,
    keyDisplay: 'c',
    modifiers: {ctrl: true},
  };
  await this.setPref(
      PrefNames.GESTURE_TO_KEY_COMBO,
      {[FacialGesture.JAW_OPEN]: JSON.stringify(keyCombination)});

  // Verify that the preference propagated to FaceGaze.
  assertEquals(this.getGestureHandler().gesturesToKeyCombos_.size, 1);

  // Jaw open for custom key press.
  let result = new MockFaceLandmarkerResult().addGestureWithConfidence(
      MediapipeFacialGesture.JAW_OPEN, 0.9);
  this.processFaceLandmarkerResult(result);

  assertEquals(
      'Custom key combination: ctrl + c (Open your mouth wide)',
      this.getBubbleText());
  assertFalse(this.getBubbleIsWarning());

  // Message should persist while the gesture and key press is still being held.
  this.triggerBubbleControllerTimeout();
  assertEquals(
      'Custom key combination: ctrl + c (Open your mouth wide)',
      this.getBubbleText());

  // Release jaw open for custom key release.
  result = new MockFaceLandmarkerResult().addGestureWithConfidence(
      MediapipeFacialGesture.JAW_OPEN, 0.1);
  this.processFaceLandmarkerResult(result);

  this.triggerBubbleControllerTimeout();
  assertEquals(this.getDefaultBubbleText(), this.getBubbleText());
});

AX_TEST_F(
    'FaceGazeMV2Test', 'BubbleTextKeyCombinationAdditionalGesture',
    async function() {
      const gestureToMacroName =
          new Map()
              .set(FacialGesture.BROW_INNER_UP, MacroName.MOUSE_CLICK_LEFT)
              .set(FacialGesture.JAW_OPEN, MacroName.CUSTOM_KEY_COMBINATION);
      const gestureToConfidence = new Map()
                                      .set(FacialGesture.BROW_INNER_UP, 0.6)
                                      .set(FacialGesture.JAW_OPEN, 0.7);
      const config = new Config()
                         .withMouseLocation({x: 600, y: 400})
                         .withBindings(gestureToMacroName, gestureToConfidence);
      await this.configureFaceGaze(config);

      assertNullOrUndefined(this.getBubbleText());

      // Set the gestures to key combinations preference.
      const keyCombination = {
        key: KeyCode.C,
        keyDisplay: 'c',
        modifiers: {ctrl: true},
      };
      await this.setPref(
          PrefNames.GESTURE_TO_KEY_COMBO,
          {[FacialGesture.JAW_OPEN]: JSON.stringify(keyCombination)});

      // Verify that the preference propagated to FaceGaze.
      assertEquals(this.getGestureHandler().gesturesToKeyCombos_.size, 1);

      // Jaw open for custom key press.
      let result = new MockFaceLandmarkerResult().addGestureWithConfidence(
          MediapipeFacialGesture.JAW_OPEN, 0.9);
      this.processFaceLandmarkerResult(result);

      assertEquals(
          'Custom key combination: ctrl + c (Open your mouth wide)',
          this.getBubbleText());

      // Trigger a mouse press and maintain key press.
      result =
          new MockFaceLandmarkerResult()
              .addGestureWithConfidence(MediapipeFacialGesture.JAW_OPEN, 0.9)
              .addGestureWithConfidence(
                  MediapipeFacialGesture.BROW_INNER_UP, 0.9);
      this.processFaceLandmarkerResult(result);

      // Newly triggered action should populate message.
      assertEquals(
          'Left-click the mouse (Raise eyebrows)', this.getBubbleText());

      // Message for key combo should persist while the gesture and key press is
      // still being held.
      this.triggerBubbleControllerTimeout();
      assertEquals(
          'Custom key combination: ctrl + c (Open your mouth wide)',
          this.getBubbleText());

      // Release jaw open for custom key release.
      result = new MockFaceLandmarkerResult().addGestureWithConfidence(
          MediapipeFacialGesture.JAW_OPEN, 0.1);
      this.processFaceLandmarkerResult(result);

      this.triggerBubbleControllerTimeout();
      assertEquals(this.getDefaultBubbleText(), this.getBubbleText());
    });

AX_TEST_F(
    'FaceGazeMV2Test', 'BubbleTextKeyCombinationAdditionalState',
    async function() {
      const gestureToMacroName =
          new Map()
              .set(FacialGesture.BROW_INNER_UP, MacroName.TOGGLE_SCROLL_MODE)
              .set(FacialGesture.JAW_OPEN, MacroName.CUSTOM_KEY_COMBINATION);
      const gestureToConfidence = new Map()
                                      .set(FacialGesture.BROW_INNER_UP, 0.6)
                                      .set(FacialGesture.JAW_OPEN, 0.7);
      const config = new Config()
                         .withMouseLocation({x: 600, y: 400})
                         .withBindings(gestureToMacroName, gestureToConfidence);
      await this.configureFaceGaze(config);

      assertNullOrUndefined(this.getBubbleText());

      // Set the gestures to key combinations preference.
      const keyCombination = {
        key: KeyCode.C,
        keyDisplay: 'c',
        modifiers: {ctrl: true},
      };
      await this.setPref(
          PrefNames.GESTURE_TO_KEY_COMBO,
          {[FacialGesture.JAW_OPEN]: JSON.stringify(keyCombination)});

      // Verify that the preference propagated to FaceGaze.
      assertEquals(this.getGestureHandler().gesturesToKeyCombos_.size, 1);

      // Jaw open for custom key press.
      let result = new MockFaceLandmarkerResult().addGestureWithConfidence(
          MediapipeFacialGesture.JAW_OPEN, 0.9);
      this.processFaceLandmarkerResult(result);

      assertEquals(
          'Custom key combination: ctrl + c (Open your mouth wide)',
          this.getBubbleText());

      // Toggle scroll mode and maintain key press.
      result =
          new MockFaceLandmarkerResult()
              .addGestureWithConfidence(MediapipeFacialGesture.JAW_OPEN, 0.9)
              .addGestureWithConfidence(
                  MediapipeFacialGesture.BROW_INNER_UP, 0.9);
      this.processFaceLandmarkerResult(result);

      // Newly triggered action should populate message.
      assertEquals('Enter scroll mode (Raise eyebrows)', this.getBubbleText());

      // Message for key combo and state should persist while the gesture and
      // key press is still being held.
      this.triggerBubbleControllerTimeout();
      assertEquals(
          'Custom key combination: ctrl + c (Open your mouth wide), ' +
              'Scroll mode active. Raise eyebrows to exit. Other ' +
              'gestures temporarily unavailable.',
          this.getBubbleText());

      // Release jaw open for custom key release.
      result = new MockFaceLandmarkerResult().addGestureWithConfidence(
          MediapipeFacialGesture.JAW_OPEN, 0.1);
      this.processFaceLandmarkerResult(result);

      assertEquals(
          'Scroll mode active. Raise eyebrows to exit. Other gestures ' +
              'temporarily unavailable.',
          this.getBubbleText());
    });

AX_TEST_F('FaceGazeMV2Test', 'ToggleFaceGazeRecognizedTime', async function() {
  const gestureToMacroName =
      new Map().set(FacialGesture.JAW_OPEN, MacroName.TOGGLE_FACEGAZE);
  const gestureToConfidence = new Map().set(FacialGesture.JAW_OPEN, 0.6);
  const config = new Config()
                     .withMouseLocation({x: 600, y: 400})
                     .withBindings(gestureToMacroName, gestureToConfidence)
                     .withRepeatDelayMs(20 * 1000);
  await this.configureFaceGaze(config);

  const initialResult = new MockFaceLandmarkerResult().addGestureWithConfidence(
      MediapipeFacialGesture.JAW_OPEN, 0.9);
  this.processFaceLandmarkerResult(initialResult);
  const jawOpenTime =
      this.getGestureLastRecognized(MediapipeFacialGesture.JAW_OPEN);
  assertTrue(!!jawOpenTime);
  assertTrue(this.getGestureHandler().paused_);

  for (let i = 0; i < 5; i++) {
    const result = new MockFaceLandmarkerResult().addGestureWithConfidence(
        MediapipeFacialGesture.JAW_OPEN, 0.9);
    this.processFaceLandmarkerResult(result);

    // 6 total times in quick succession still only generates one toggle event.
    assertEquals(
        jawOpenTime,
        this.getGestureLastRecognized(MediapipeFacialGesture.JAW_OPEN));
    assertTrue(this.getGestureHandler().paused_);
  }

  // Check that we can resume after pausing.
  this.setGestureRepeatDelay(-1);
  const resumeResult = new MockFaceLandmarkerResult().addGestureWithConfidence(
      MediapipeFacialGesture.JAW_OPEN, 0.9);
  this.processFaceLandmarkerResult(
      resumeResult, /*triggerMouseControllerInterval=*/ false);
  assertNotEquals(
      jawOpenTime,
      this.getGestureLastRecognized(MediapipeFacialGesture.JAW_OPEN));
  assertFalse(this.getGestureHandler().paused_);
});

AX_TEST_F('FaceGazeMV2Test', 'BubbleTextStateMessages', async function() {
  const gestureToMacroName =
      new Map().set(FacialGesture.JAW_OPEN, MacroName.TOGGLE_FACEGAZE);
  const gestureToConfidence = new Map().set(FacialGesture.JAW_OPEN, 0.6);
  const config = new Config()
                     .withMouseLocation({x: 600, y: 400})
                     .withBindings(gestureToMacroName, gestureToConfidence);
  await this.configureFaceGaze(config);

  assertNullOrUndefined(this.getBubbleText());

  const result = new MockFaceLandmarkerResult().addGestureWithConfidence(
      MediapipeFacialGesture.JAW_OPEN, 0.9);
  this.processFaceLandmarkerResult(result);

  assertEquals(
      'Pause face control (Open your mouth wide)', this.getBubbleText());
  assertFalse(this.getBubbleIsWarning());

  // FaceGaze should display important messages about the state after the
  // timeout has elapsed.
  this.triggerBubbleControllerTimeout();
  assertEquals(
      'Face control paused. Open your mouth wide to resume. Other gestures temporarily unavailable.',
      this.getBubbleText());
  assertTrue(this.getBubbleIsWarning());
});

AX_TEST_F('FaceGazeMV2Test', 'BubbleTextLongClickStateMessage', async function() {
  const gestureToMacroName =
      new Map().set(FacialGesture.JAW_OPEN, MacroName.MOUSE_LONG_CLICK_LEFT);
  const gestureToConfidence = new Map().set(FacialGesture.JAW_OPEN, 0.3);
  const config = new Config()
                     .withMouseLocation({x: 600, y: 400})
                     .withBindings(gestureToMacroName, gestureToConfidence)
                     .withRepeatDelayMs(0);
  await this.configureFaceGaze(config);

  assertNullOrUndefined(this.getBubbleText());

  // Trigger a long click.
  const result = new MockFaceLandmarkerResult().addGestureWithConfidence(
      MediapipeFacialGesture.JAW_OPEN, 0.9);
  this.processFaceLandmarkerResult(result, false);

  assertEquals(
      'Start drag and drop (Open your mouth wide)', this.getBubbleText());
  assertFalse(this.getBubbleIsWarning());

  // FaceGaze should display important messages about the state after the
  // timeout has elapsed.
  this.triggerBubbleControllerTimeout();
  assertTrue(this.getMouseController().isLongClickActive());
  assertEquals(
      'Drag and drop in progress. Open your mouth wide to end. Other gestures temporarily unavailable.',
      this.getBubbleText());
  assertTrue(this.getBubbleIsWarning());

  // Finish drag and drop action.
  this.processFaceLandmarkerResult(result);
  assertEquals(
      'End drag and drop (Open your mouth wide)', this.getBubbleText());
  assertFalse(this.getBubbleIsWarning());

  this.triggerBubbleControllerTimeout();
  assertEquals(this.getDefaultBubbleText(), this.getBubbleText());
  assertFalse(this.getBubbleIsWarning());
});

AX_TEST_F('FaceGazeMV2Test', 'BubbleTextDictationStateMessage', async function() {
  const gestureToMacroName =
      new Map().set(FacialGesture.JAW_OPEN, MacroName.TOGGLE_DICTATION);
  const gestureToConfidence = new Map().set(FacialGesture.JAW_OPEN, 0.3);
  const config = new Config()
                     .withMouseLocation({x: 600, y: 400})
                     .withBindings(gestureToMacroName, gestureToConfidence)
                     .withRepeatDelayMs(0);
  await this.configureFaceGaze(config);

  assertNullOrUndefined(this.getBubbleText());

  // Toggle dictation.
  const result = new MockFaceLandmarkerResult().addGestureWithConfidence(
      MediapipeFacialGesture.JAW_OPEN, 0.9);
  this.processFaceLandmarkerResult(result, false);

  // Make FaceGaze think dictation is active.
  this.getGestureHandler().isDictationActive_ = () => true;

  assertEquals('Start dictation (Open your mouth wide)', this.getBubbleText());
  assertFalse(this.getBubbleIsWarning());

  // Make bubble controller think that Dictation is active.
  this.getFaceGaze().bubbleController_.getState_ = () => {
    return {dictation: FacialGesture.JAW_OPEN};
  };

  // FaceGaze should display important messages about the state after the
  // timeout has elapsed.
  this.triggerBubbleControllerTimeout();
  assertEquals(
      'Dictation active. Open your mouth wide to stop. Other gestures temporarily unavailable.',
      this.getBubbleText());
  assertTrue(this.getBubbleIsWarning());

  // Toggle dictation off.
  this.processFaceLandmarkerResult(result);
  this.getFaceGaze().bubbleController_.getState_ = () => {
    return {};
  };

  // Make FaceGaze think dictation is off.
  this.getGestureHandler().isDictationActive_ = () => false;
  assertEquals('Stop dictation (Open your mouth wide)', this.getBubbleText());
  assertFalse(this.getBubbleIsWarning());
  this.triggerBubbleControllerTimeout();
  assertEquals(this.getDefaultBubbleText(), this.getBubbleText());
  assertFalse(this.getBubbleIsWarning());
});

AX_TEST_F('FaceGazeMV2Test', 'BubbleTextStateAndActionMessages', async function() {
  const gestureToMacroName =
      new Map()
          .set(FacialGesture.JAW_OPEN, MacroName.MOUSE_CLICK_LEFT)
          .set(FacialGesture.BROW_INNER_UP, MacroName.TOGGLE_FACEGAZE);
  const gestureToConfidence = new Map()
                                  .set(FacialGesture.JAW_OPEN, 0.6)
                                  .set(FacialGesture.BROW_INNER_UP, 0.6);
  const config = new Config()
                     .withMouseLocation({x: 600, y: 400})
                     .withBindings(gestureToMacroName, gestureToConfidence)
                     .withRepeatDelayMs(0);
  await this.configureFaceGaze(config);

  assertNullOrUndefined(this.getBubbleText());

  let result =
      new MockFaceLandmarkerResult()
          .addGestureWithConfidence(MediapipeFacialGesture.JAW_OPEN, 0.9)
          .addGestureWithConfidence(MediapipeFacialGesture.BROW_INNER_UP, 0.9);
  this.processFaceLandmarkerResult(result);

  assertEquals(
      'Pause face control (Raise eyebrows), ' +
          'Left-click the mouse (Open your mouth wide)',
      this.getBubbleText());
  assertFalse(this.getBubbleIsWarning());

  // FaceGaze should display important messages about the state after the
  // timeout has elapsed.
  this.triggerBubbleControllerTimeout();
  assertEquals(
      'Face control paused. Raise eyebrows to resume. Other gestures temporarily unavailable.',
      this.getBubbleText());
  assertTrue(this.getBubbleIsWarning());

  // Send another result. Note that since FaceGaze is paused, no action
  // will be taken.
  result = new MockFaceLandmarkerResult().addGestureWithConfidence(
      MediapipeFacialGesture.JAW_OPEN, 0.9);
  this.processFaceLandmarkerResult(result);

  assertEquals(
      'Face control paused. Raise eyebrows to resume. Other gestures temporarily unavailable.',
      this.getBubbleText());
  assertTrue(this.getBubbleIsWarning());

  result = new MockFaceLandmarkerResult().addGestureWithConfidence(
      MediapipeFacialGesture.BROW_INNER_UP, 0.9);
  this.processFaceLandmarkerResult(
      result, /*triggerMouseControllerInterval=*/ false);
  assertEquals('Resume face control (Raise eyebrows)', this.getBubbleText());
  assertFalse(this.getBubbleIsWarning());
});

AX_TEST_F('FaceGazeMV2Test', 'TurnOffActionsWhileInScrollMode', async function() {
  const gestureToMacroName =
      new Map().set(FacialGesture.JAW_OPEN, MacroName.TOGGLE_SCROLL_MODE);
  const gestureToConfidence = new Map().set(FacialGesture.JAW_OPEN, 0.6);
  const config = new Config()
                     .withMouseLocation({x: 600, y: 400})
                     .withBufferSize(1)
                     .withCursorControlEnabled(true)
                     .withBindings(gestureToMacroName, gestureToConfidence);
  await this.startFacegazeWithConfigAndForeheadLocation_(config, 0.1, 0.2);

  // Toggle scroll mode on.
  const result = new MockFaceLandmarkerResult().addGestureWithConfidence(
      MediapipeFacialGesture.JAW_OPEN, 0.9);
  this.processFaceLandmarkerResult(result);
  assertTrue(this.getScrollModeController().active());

  this.triggerBubbleControllerTimeout();
  assertEquals(
      'Scroll mode active. Open your mouth wide to exit. Other gestures temporarily unavailable.',
      this.getBubbleText());

  // Turn off actions via pref.
  await this.setPref(PrefNames.ACTIONS_ENABLED, false);

  // Ensure scroll mode automatically toggled off.
  assertFalse(this.getScrollModeController().active());
  assertEquals(this.getDefaultBubbleText(), this.getBubbleText());
});

AX_TEST_F(
    'FaceGazeMV2Test', 'RemoveScrollModeActionWhileInScrollMode',
    async function() {
      const gestureToMacroName =
          new Map().set(FacialGesture.JAW_OPEN, MacroName.TOGGLE_SCROLL_MODE);
      const gestureToConfidence = new Map().set(FacialGesture.JAW_OPEN, 0.6);
      const config = new Config()
                         .withMouseLocation({x: 600, y: 400})
                         .withBufferSize(1)
                         .withCursorControlEnabled(true)
                         .withBindings(gestureToMacroName, gestureToConfidence);
      await this.startFacegazeWithConfigAndForeheadLocation_(config, 0.1, 0.2);

      // Toggle scroll mode on.
      const result = new MockFaceLandmarkerResult().addGestureWithConfidence(
          MediapipeFacialGesture.JAW_OPEN, 0.9);
      this.processFaceLandmarkerResult(result);
      assertTrue(this.getScrollModeController().active());

      this.triggerBubbleControllerTimeout();
      assertEquals(
          'Scroll mode active. Open your mouth wide to exit. Other gestures temporarily unavailable.',
          this.getBubbleText());

      // Remove scroll mode action.
      await this.setPref(PrefNames.GESTURE_TO_MACRO, {});

      // Ensure scroll mode automatically toggled off.
      assertFalse(this.getScrollModeController().active());
      assertEquals(this.getDefaultBubbleText(), this.getBubbleText());
    });

AX_TEST_F('FaceGazeMV2Test', 'GesturesDisabledInScrollMode', async function() {
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
                     .withBindings(gestureToMacroName, gestureToConfidence)
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

// Ensures that localization works for all gestures and macros that get
// displayed in the bubble UI.
AX_TEST_F('FaceGazeMV2Test', 'BubbleTextLocalization', async function() {
  const gestures = [
    FacialGesture.BROW_INNER_UP,
    FacialGesture.BROWS_DOWN,
    FacialGesture.EYE_SQUINT_LEFT,
    FacialGesture.EYE_SQUINT_RIGHT,
    FacialGesture.EYES_BLINK,
    FacialGesture.EYES_LOOK_DOWN,
    FacialGesture.EYES_LOOK_LEFT,
    FacialGesture.EYES_LOOK_RIGHT,
    FacialGesture.EYES_LOOK_UP,
    FacialGesture.JAW_LEFT,
    FacialGesture.JAW_OPEN,
    FacialGesture.JAW_RIGHT,
    FacialGesture.MOUTH_FUNNEL,
    FacialGesture.MOUTH_LEFT,
    FacialGesture.MOUTH_PUCKER,
    FacialGesture.MOUTH_RIGHT,
    FacialGesture.MOUTH_SMILE,
    FacialGesture.MOUTH_UPPER_UP,
  ];

  const macroNames = [
    MacroName.CUSTOM_KEY_COMBINATION,
    MacroName.KEY_PRESS_DOWN,
    MacroName.KEY_PRESS_LEFT,
    MacroName.KEY_PRESS_MEDIA_PLAY_PAUSE,
    MacroName.KEY_PRESS_RIGHT,
    MacroName.KEY_PRESS_SCREENSHOT,
    MacroName.KEY_PRESS_SPACE,
    MacroName.KEY_PRESS_TOGGLE_OVERVIEW,
    MacroName.KEY_PRESS_UP,
    MacroName.MOUSE_CLICK_LEFT,
    MacroName.MOUSE_CLICK_LEFT_DOUBLE,
    MacroName.MOUSE_CLICK_RIGHT,
    MacroName.MOUSE_LONG_CLICK_LEFT,
    MacroName.RESET_CURSOR,
    MacroName.TOGGLE_DICTATION,
    MacroName.TOGGLE_FACEGAZE,
    MacroName.TOGGLE_SCROLL_MODE,
    MacroName.TOGGLE_VIRTUAL_KEYBOARD,
  ];

  // Set the gestures to key combinations preference.
  const keyCombination = {
    key: KeyCode.C,
    keyDisplay: 'c',
    modifiers: {ctrl: true},
  };
  await this.setPref(
      PrefNames.GESTURE_TO_KEY_COMBO,
      {[FacialGesture.BROW_INNER_UP]: JSON.stringify(keyCombination)});

  const gestureHandler = this.getGestureHandler();

  let lastText = '';
  for (const macroName of macroNames) {
    // Create mock macro using a placeholder gesture.
    const currentText = BubbleController.getDisplayTextForMacro_(
        gestureHandler.macroFromName_(macroName, FacialGesture.BROW_INNER_UP));
    assertNotEquals(currentText, '');
    assertNotEquals(currentText, lastText);
    lastText = currentText;
  }

  lastText = '';
  for (const gesture of gestures) {
    const currentText = BubbleController.getDisplayTextForGesture_(gesture);
    assertNotEquals(currentText, '');
    assertNotEquals(currentText, lastText);
    lastText = currentText;
  }
});

AX_TEST_F(
    'FaceGazeMV2Test', 'GesturesDisabledDuringDictation', async function() {
      const gestureToMacroName =
          new Map()
              .set(FacialGesture.BROW_INNER_UP, MacroName.MOUSE_CLICK_LEFT)
              .set(FacialGesture.JAW_OPEN, MacroName.TOGGLE_DICTATION);
      const gestureToConfidence = new Map()
                                      .set(FacialGesture.BROW_INNER_UP, 0.3)
                                      .set(FacialGesture.JAW_OPEN, 0.3);
      const config = new Config()
                         .withMouseLocation({x: 600, y: 400})
                         .withBufferSize(1)
                         .withCursorControlEnabled(false)
                         .withBindings(gestureToMacroName, gestureToConfidence)
                         .withRepeatDelayMs(0);
      await this.startFacegazeWithConfigAndForeheadLocation_(config, 0.1, 0.2);

      const gestureHandler = this.getGestureHandler();

      // Make FaceGaze think that Dictation is active.
      gestureHandler.isDictationActive_ = () => true;

      // Ensure the only action allowed is TOGGLE_DICTATION.
      let result = gestureHandler.detectMacros(
          new MockFaceLandmarkerResult()
              .addGestureWithConfidence(
                  MediapipeFacialGesture.BROW_INNER_UP, 0.9)
              .addGestureWithConfidence(MediapipeFacialGesture.JAW_OPEN, 0.9));
      assertEquals(result.macros.length, 1);
      assertEquals(result.macros[0].getName(), MacroName.TOGGLE_DICTATION);

      // Make FaceGaze think that Dictation is inactive.
      gestureHandler.isDictationActive_ = () => false;

      // Ensure no restrictions are applied.
      result = gestureHandler.detectMacros(
          new MockFaceLandmarkerResult()
              .addGestureWithConfidence(
                  MediapipeFacialGesture.BROW_INNER_UP, 0.9)
              .addGestureWithConfidence(MediapipeFacialGesture.JAW_OPEN, 0.9));
      assertEquals(result.macros.length, 2);
    });

AX_TEST_F('FaceGazeMV2Test', 'BlinkDoesNotTriggerEyeSquint', async function() {
  const gestureToMacroName =
      new Map()
          .set(FacialGesture.EYES_BLINK, MacroName.MOUSE_CLICK_LEFT)
          .set(FacialGesture.EYE_SQUINT_LEFT, MacroName.TOGGLE_SCROLL_MODE)
          .set(FacialGesture.EYE_SQUINT_RIGHT, MacroName.TOGGLE_FACEGAZE);
  const gestureToConfidence = new Map()
                                  .set(FacialGesture.EYES_BLINK, 0.6)
                                  .set(FacialGesture.EYE_SQUINT_LEFT, 0.6)
                                  .set(FacialGesture.EYE_SQUINT_RIGHT, 0.6);
  const config = new Config()
                     .withMouseLocation({x: 600, y: 400})
                     .withBindings(gestureToMacroName, gestureToConfidence)
                     .withRepeatDelayMs(0);
  await this.configureFaceGaze(config);

  const gestureHandler = this.getGestureHandler();

  // If eye squint on one side occurs at same time as a blink or squint on the
  // wrong side, then the gesture should not register as an eye squint on the
  // intended side.
  let result = gestureHandler.detectMacros(
      new MockFaceLandmarkerResult()
          .addGestureWithConfidence(MediapipeFacialGesture.EYE_SQUINT_LEFT, 0.9)
          .addGestureWithConfidence(MediapipeFacialGesture.EYE_BLINK_LEFT, 0.6)
          .addGestureWithConfidence(
              MediapipeFacialGesture.EYE_BLINK_RIGHT, 0.6));
  assertEquals(result.macros.length, 1);

  result = gestureHandler.detectMacros(
      new MockFaceLandmarkerResult()
          .addGestureWithConfidence(MediapipeFacialGesture.EYE_SQUINT_LEFT, 0.9)
          .addGestureWithConfidence(
              MediapipeFacialGesture.EYE_SQUINT_RIGHT, 0.6));
  assertEquals(result.macros.length, 0);

  result = gestureHandler.detectMacros(
      new MockFaceLandmarkerResult()
          .addGestureWithConfidence(
              MediapipeFacialGesture.EYE_SQUINT_RIGHT, 0.9)
          .addGestureWithConfidence(MediapipeFacialGesture.EYE_BLINK_LEFT, 0.6)
          .addGestureWithConfidence(
              MediapipeFacialGesture.EYE_BLINK_RIGHT, 0.6));
  assertEquals(result.macros.length, 1);

  result = gestureHandler.detectMacros(
      new MockFaceLandmarkerResult()
          .addGestureWithConfidence(
              MediapipeFacialGesture.EYE_SQUINT_RIGHT, 0.9)
          .addGestureWithConfidence(
              MediapipeFacialGesture.EYE_SQUINT_LEFT, 0.6));
  assertEquals(result.macros.length, 0);

  // Low confidence levels of blink or squint on the wrong side should register
  // as a squint on the intended side.
  result = gestureHandler.detectMacros(
      new MockFaceLandmarkerResult()
          .addGestureWithConfidence(MediapipeFacialGesture.EYE_SQUINT_LEFT, 0.9)
          .addGestureWithConfidence(MediapipeFacialGesture.EYE_BLINK_LEFT, 0.3)
          .addGestureWithConfidence(
              MediapipeFacialGesture.EYE_BLINK_RIGHT, 0.3));
  assertEquals(result.macros.length, 1);

  result = gestureHandler.detectMacros(
      new MockFaceLandmarkerResult()
          .addGestureWithConfidence(MediapipeFacialGesture.EYE_SQUINT_LEFT, 0.9)
          .addGestureWithConfidence(
              MediapipeFacialGesture.EYE_SQUINT_RIGHT, 0.3));
  assertEquals(result.macros.length, 1);

  result = gestureHandler.detectMacros(
      new MockFaceLandmarkerResult()
          .addGestureWithConfidence(
              MediapipeFacialGesture.EYE_SQUINT_RIGHT, 0.9)
          .addGestureWithConfidence(MediapipeFacialGesture.EYE_BLINK_LEFT, 0.3)
          .addGestureWithConfidence(
              MediapipeFacialGesture.EYE_BLINK_RIGHT, 0.3));
  assertEquals(result.macros.length, 1);

  result = gestureHandler.detectMacros(
      new MockFaceLandmarkerResult()
          .addGestureWithConfidence(
              MediapipeFacialGesture.EYE_SQUINT_RIGHT, 0.9)
          .addGestureWithConfidence(
              MediapipeFacialGesture.EYE_SQUINT_LEFT, 0.3));
  assertEquals(result.macros.length, 1);
});

AX_TEST_F(
    'FaceGazeMV2Test', 'InvalidTimeDurationGestureNotDetected',
    async function() {
      const gestureToMacroName =
          new Map().set(FacialGesture.EYES_BLINK, MacroName.MOUSE_CLICK_LEFT);
      const gestureToConfidence = new Map().set(FacialGesture.EYES_BLINK, 0.6);

      // Set min duration very long so no duration should trigger action.
      const config = new Config()
                         .withMouseLocation({x: 600, y: 400})
                         .withBindings(gestureToMacroName, gestureToConfidence)
                         .withRepeatDelayMs(0)
                         .withMinDurationMs(30 * 1000);
      await this.configureFaceGaze(config);

      const gestureHandler = this.getGestureHandler();

      let result = new MockFaceLandmarkerResult()
                       .addGestureWithConfidence(
                           MediapipeFacialGesture.EYE_BLINK_LEFT, 0.6)
                       .addGestureWithConfidence(
                           MediapipeFacialGesture.EYE_BLINK_RIGHT, 0.6);
      this.processFaceLandmarkerResult(result);
      assertEquals(1, gestureHandler.gestureTimer_.gestureStart_.size);
      this.assertNumMouseEvents(0);

      // Second landmarker result will give the gesture a duration but it should
      // be too short to trigger the action.
      result = new MockFaceLandmarkerResult()
                   .addGestureWithConfidence(
                       MediapipeFacialGesture.EYE_BLINK_LEFT, 0.6)
                   .addGestureWithConfidence(
                       MediapipeFacialGesture.EYE_BLINK_RIGHT, 0.6);
      this.processFaceLandmarkerResult(result);
      assertEquals(1, gestureHandler.gestureTimer_.gestureStart_.size);
      this.assertNumMouseEvents(0);
    });

AX_TEST_F(
    'FaceGazeMV2Test', 'ValidTimeDurationGestureDetected', async function() {
      const gestureToMacroName =
          new Map().set(FacialGesture.EYES_BLINK, MacroName.MOUSE_CLICK_LEFT);
      const gestureToConfidence = new Map().set(FacialGesture.EYES_BLINK, 0.6);

      // Set min duration very short so actions with any duration should be
      // recognized. It is possible for a gesture to execute so quickly during a
      // test that the duration appears to be 0 ms, so set the min duration
      // threshold to -1 to ensure gestures with duration of 0 ms are
      // recognized.
      const config = new Config()
                         .withMouseLocation({x: 600, y: 400})
                         .withBindings(gestureToMacroName, gestureToConfidence)
                         .withRepeatDelayMs(0)
                         .withMinDurationMs(-1);
      await this.configureFaceGaze(config);

      const gestureHandler = this.getGestureHandler();
      assertEquals(-1, gestureHandler.gestureTimer_.minDurationMs_);

      let result = new MockFaceLandmarkerResult()
                       .addGestureWithConfidence(
                           MediapipeFacialGesture.EYE_BLINK_LEFT, 0.6)
                       .addGestureWithConfidence(
                           MediapipeFacialGesture.EYE_BLINK_RIGHT, 0.6);
      this.processFaceLandmarkerResult(result);
      assertEquals(1, gestureHandler.gestureTimer_.gestureStart_.size);
      this.assertNumMouseEvents(0);

      // Second landmarker result will give the gesture a duration, which should
      // trigger the action since the min duration is set to 0.
      result = new MockFaceLandmarkerResult()
                   .addGestureWithConfidence(
                       MediapipeFacialGesture.EYE_BLINK_LEFT, 0.6)
                   .addGestureWithConfidence(
                       MediapipeFacialGesture.EYE_BLINK_RIGHT, 0.6);
      this.processFaceLandmarkerResult(result);
      assertEquals(1, gestureHandler.gestureTimer_.gestureStart_.size);
      this.assertNumMouseEvents(2);

      // Check that the start times are cleared if no gestures are detected at
      // any confidence levels.
      result = new MockFaceLandmarkerResult();
      this.processFaceLandmarkerResult(result);
      assertEquals(0, gestureHandler.gestureTimer_.gestureStart_.size);
      this.assertNumMouseEvents(2);
    });

AX_TEST_F(
    'FaceGazeMV2Test', 'ValidTimeDurationGestureDetectedAfterInvalid',
    async function() {
      const gestureToMacroName =
          new Map().set(FacialGesture.EYES_BLINK, MacroName.MOUSE_CLICK_LEFT);
      const gestureToConfidence = new Map().set(FacialGesture.EYES_BLINK, 0.6);

      // Set min duration very short so actions with any duration should be
      // recognized. It is possible for a gesture to execute so quickly during a
      // test that the duration appears to be 0 ms, so set the min duration
      // threshold to -1 to ensure gestures with duration of 0 ms are
      // recognized.
      const config = new Config()
                         .withMouseLocation({x: 600, y: 400})
                         .withBindings(gestureToMacroName, gestureToConfidence)
                         .withRepeatDelayMs(0)
                         .withMinDurationMs(-1);
      await this.configureFaceGaze(config);
      const gestureHandler = this.getGestureHandler();
      assertEquals(-1, gestureHandler.gestureTimer_.minDurationMs_);

      let result = new MockFaceLandmarkerResult()
                       .addGestureWithConfidence(
                           MediapipeFacialGesture.EYE_BLINK_LEFT, 0.6)
                       .addGestureWithConfidence(
                           MediapipeFacialGesture.EYE_BLINK_RIGHT, 0.6);
      this.processFaceLandmarkerResult(result);
      assertEquals(1, gestureHandler.gestureTimer_.gestureStart_.size);
      this.assertNumMouseEvents(0);

      // Clear out the landmarker.
      result = new MockFaceLandmarkerResult()
                   .addGestureWithConfidence(
                       MediapipeFacialGesture.EYE_BLINK_LEFT, 0.0)
                   .addGestureWithConfidence(
                       MediapipeFacialGesture.EYE_BLINK_RIGHT, 0.0);
      this.processFaceLandmarkerResult(result);
      assertEquals(0, gestureHandler.gestureTimer_.gestureStart_.size);
      this.assertNumMouseEvents(0);

      // Start a new gesture.
      result = new MockFaceLandmarkerResult()
                   .addGestureWithConfidence(
                       MediapipeFacialGesture.EYE_BLINK_LEFT, 0.6)
                   .addGestureWithConfidence(
                       MediapipeFacialGesture.EYE_BLINK_RIGHT, 0.6);
      this.processFaceLandmarkerResult(result);
      assertEquals(1, gestureHandler.gestureTimer_.gestureStart_.size);
      this.assertNumMouseEvents(0);

      // This landmarker result should give the gesture a duration and trigger a
      // mouse click.
      result = new MockFaceLandmarkerResult()
                   .addGestureWithConfidence(
                       MediapipeFacialGesture.EYE_BLINK_LEFT, 0.6)
                   .addGestureWithConfidence(
                       MediapipeFacialGesture.EYE_BLINK_RIGHT, 0.6);
      this.processFaceLandmarkerResult(result);
      assertEquals(1, gestureHandler.gestureTimer_.gestureStart_.size);
      this.assertNumMouseEvents(2);
    });

AX_TEST_F(
    'FaceGazeMV2Test', 'ValidTimeDurationGestureTriggersActionMultiple',
    async function() {
      const gestureToMacroName =
          new Map().set(FacialGesture.EYES_BLINK, MacroName.MOUSE_CLICK_LEFT);
      const gestureToConfidence = new Map().set(FacialGesture.EYES_BLINK, 0.6);

      // Set min duration very short so actions with any duration should be
      // recognized. It is possible for a gesture to execute so quickly during a
      // test that the duration appears to be 0 ms, so set the min duration
      // threshold to -1 to ensure gestures with duration of 0 ms are
      // recognized.
      const config = new Config()
                         .withMouseLocation({x: 600, y: 400})
                         .withBindings(gestureToMacroName, gestureToConfidence)
                         .withRepeatDelayMs(0)
                         .withMinDurationMs(-1);
      await this.configureFaceGaze(config);
      const gestureHandler = this.getGestureHandler();
      assertEquals(-1, gestureHandler.gestureTimer_.minDurationMs_);

      let result = new MockFaceLandmarkerResult()
                       .addGestureWithConfidence(
                           MediapipeFacialGesture.EYE_BLINK_LEFT, 0.6)
                       .addGestureWithConfidence(
                           MediapipeFacialGesture.EYE_BLINK_RIGHT, 0.6);
      this.processFaceLandmarkerResult(result);
      assertEquals(1, gestureHandler.gestureTimer_.gestureStart_.size);
      this.assertNumMouseEvents(0);

      // This landmarker result should give the gesture a duration and trigger a
      // mouse click.
      result = new MockFaceLandmarkerResult()
                   .addGestureWithConfidence(
                       MediapipeFacialGesture.EYE_BLINK_LEFT, 0.6)
                   .addGestureWithConfidence(
                       MediapipeFacialGesture.EYE_BLINK_RIGHT, 0.6);
      this.processFaceLandmarkerResult(result);
      assertEquals(1, gestureHandler.gestureTimer_.gestureStart_.size);
      this.assertNumMouseEvents(2);

      // Clear out the landmarker result.
      result = new MockFaceLandmarkerResult()
                   .addGestureWithConfidence(
                       MediapipeFacialGesture.EYE_BLINK_LEFT, 0.0)
                   .addGestureWithConfidence(
                       MediapipeFacialGesture.EYE_BLINK_RIGHT, 0.0);
      this.processFaceLandmarkerResult(result);
      assertEquals(0, gestureHandler.gestureTimer_.gestureStart_.size);
      this.assertNumMouseEvents(2);

      // Start a new gesture.
      result = new MockFaceLandmarkerResult()
                   .addGestureWithConfidence(
                       MediapipeFacialGesture.EYE_BLINK_LEFT, 0.6)
                   .addGestureWithConfidence(
                       MediapipeFacialGesture.EYE_BLINK_RIGHT, 0.6);
      this.processFaceLandmarkerResult(result);
      assertEquals(1, gestureHandler.gestureTimer_.gestureStart_.size);
      this.assertNumMouseEvents(2);

      // This landmarker result should give the gesture a duration and trigger a
      // mouse click.
      result = new MockFaceLandmarkerResult()
                   .addGestureWithConfidence(
                       MediapipeFacialGesture.EYE_BLINK_LEFT, 0.6)
                   .addGestureWithConfidence(
                       MediapipeFacialGesture.EYE_BLINK_RIGHT, 0.6);
      this.processFaceLandmarkerResult(result);
      assertEquals(1, gestureHandler.gestureTimer_.gestureStart_.size);
      this.assertNumMouseEvents(4);
    });

AX_TEST_F('FaceGazeMV2Test', 'PrecisionClickMouseEvents', async function() {
  const gestureToMacroName =
      new Map().set(FacialGesture.JAW_OPEN, MacroName.MOUSE_CLICK_LEFT);
  const gestureToConfidence = new Map().set(FacialGesture.JAW_OPEN, 0.3);
  const config = new Config()
                     .withBindings(gestureToMacroName, gestureToConfidence)
                     .withRepeatDelayMs(0)
                     .withPrecisionEnabled(/*speedFactor=*/ 50);
  await this.configureFaceGaze(config);

  // Start precision click by performing gesture assigned to left click.
  let result = new MockFaceLandmarkerResult().addGestureWithConfidence(
      MediapipeFacialGesture.JAW_OPEN, 0.9);
  this.processFaceLandmarkerResult(result, false);
  assertTrue(this.getMouseController().isPrecisionActive());
  // Ensure no mouse events were sent.
  assertEquals(this.getMouseEvents().length, 0);

  // Perform the gesture again to left-click.
  result = new MockFaceLandmarkerResult().addGestureWithConfidence(
      MediapipeFacialGesture.JAW_OPEN, 0.9);
  this.processFaceLandmarkerResult(result, false);
  assertFalse(this.getMouseController().isPrecisionActive());

  const mouseEvents = this.getMouseEvents();
  assertEquals(mouseEvents.length, 2);
  this.assertMousePress(mouseEvents[0]);
  this.assertMouseRelease(mouseEvents[1]);
  assertEquals(
      chrome.accessibilityPrivate.SyntheticMouseEventButton.LEFT,
      mouseEvents[0].mouseButton);
  assertEquals(
      chrome.accessibilityPrivate.SyntheticMouseEventButton.LEFT,
      mouseEvents[1].mouseButton);
});

AX_TEST_F('FaceGazeMV2Test', 'PrecisionClickBubbleText', async function() {
  const gestureToMacroName =
      new Map().set(FacialGesture.JAW_OPEN, MacroName.MOUSE_CLICK_LEFT);
  const gestureToConfidence = new Map().set(FacialGesture.JAW_OPEN, 0.3);
  const config = new Config()
                     .withBindings(gestureToMacroName, gestureToConfidence)
                     .withRepeatDelayMs(0)
                     .withPrecisionEnabled(/*speedFactor=*/ 50);
  await this.configureFaceGaze(config);

  assertNullOrUndefined(this.getBubbleText());

  // Start precision click by performing gesture assigned to left click.
  let result = new MockFaceLandmarkerResult().addGestureWithConfidence(
      MediapipeFacialGesture.JAW_OPEN, 0.9);
  this.processFaceLandmarkerResult(result, false);
  assertTrue(this.getMouseController().isPrecisionActive());
  assertEquals(
      'Start precision click (Open your mouth wide)', this.getBubbleText());

  // FaceGaze should display important messages about the state after the
  // timeout has elapsed.
  this.triggerBubbleControllerTimeout();
  assertEquals(
      'Mouse speed reduced, Open your mouth wide again to click',
      this.getBubbleText());

  // Perform the gesture again to left-click.
  result = new MockFaceLandmarkerResult().addGestureWithConfidence(
      MediapipeFacialGesture.JAW_OPEN, 0.9);
  this.processFaceLandmarkerResult(result, false);
  assertFalse(this.getMouseController().isPrecisionActive());
  assertEquals(
      'Left-click the mouse (Open your mouth wide)', this.getBubbleText());

  this.triggerBubbleControllerTimeout();
  assertEquals(this.getDefaultBubbleText(), this.getBubbleText());
});

AX_TEST_F('FaceGazeMV2Test', 'PrecisionClickMouseMovement', async function() {
  const gestureToMacroName =
      new Map().set(FacialGesture.JAW_OPEN, MacroName.MOUSE_CLICK_LEFT);
  const gestureToConfidence = new Map().set(FacialGesture.JAW_OPEN, 0.3);
  const config = new Config()
                     .withMouseLocation({x: 600, y: 400})
                     .withBufferSize(1)
                     .withCursorControlEnabled(true)
                     .withBindings(gestureToMacroName, gestureToConfidence)
                     .withRepeatDelayMs(0)
                     .withPrecisionEnabled(/*speedFactor=*/ 50);
  await this.startFacegazeWithConfigAndForeheadLocation_(config, 0.1, 0.2);

  // Update the head position by 0.1 in each direction. Notice there is a
  // significant delta in each direction.
  let result =
      new MockFaceLandmarkerResult().setNormalizedForeheadLocation(0.11, 0.21);
  this.processFaceLandmarkerResult(result);
  this.assertLatestCursorPosition({x: 360, y: 560});

  // Start precision click by performing gesture assigned to left click.
  result = new MockFaceLandmarkerResult().addGestureWithConfidence(
      MediapipeFacialGesture.JAW_OPEN, 0.9);
  this.processFaceLandmarkerResult(result);
  assertTrue(this.getMouseController().isPrecisionActive());

  // Update the head position by the same delta. Notice that the cursor delta in
  // each direction is cut in half (due to the precision speed factor).
  result =
      new MockFaceLandmarkerResult().setNormalizedForeheadLocation(0.12, 0.22);
  this.processFaceLandmarkerResult(result);
  this.assertLatestCursorPosition({x: 240, y: 640});
});

AX_TEST_F(
    'FaceGazeMV2Test', 'TurnOffActionsDuringPrecisionClick', async function() {
      const gestureToMacroName =
          new Map().set(FacialGesture.JAW_OPEN, MacroName.MOUSE_CLICK_LEFT);
      const gestureToConfidence = new Map().set(FacialGesture.JAW_OPEN, 0.6);
      const config = new Config()
                         .withBindings(gestureToMacroName, gestureToConfidence)
                         .withPrecisionEnabled(/*speedFactor=*/ 50);
      await this.configureFaceGaze(config);

      // Start precision click by performing gesture assigned to left click.
      const result = new MockFaceLandmarkerResult().addGestureWithConfidence(
          MediapipeFacialGesture.JAW_OPEN, 0.9);
      this.processFaceLandmarkerResult(result, false);
      assertTrue(this.getMouseController().isPrecisionActive());

      this.triggerBubbleControllerTimeout();
      assertEquals(
          'Mouse speed reduced, Open your mouth wide again to click',
          this.getBubbleText());

      // Turn off actions via pref.
      await this.setPref(PrefNames.ACTIONS_ENABLED, false);

      // Ensure precision click is automatically toggled off.
      assertFalse(this.getMouseController().isPrecisionActive());
      assertEquals(this.getDefaultBubbleText(), this.getBubbleText());
    });

AX_TEST_F(
    'FaceGazeMV2Test', 'TurnOffCursorControlDuringPrecisionClick',
    async function() {
      const gestureToMacroName =
          new Map().set(FacialGesture.JAW_OPEN, MacroName.MOUSE_CLICK_LEFT);
      const gestureToConfidence = new Map().set(FacialGesture.JAW_OPEN, 0.6);
      const config = new Config()
                         .withBindings(gestureToMacroName, gestureToConfidence)
                         .withPrecisionEnabled(/*speedFactor=*/ 50);
      await this.configureFaceGaze(config);

      // Start precision click by performing gesture assigned to left click.
      const result = new MockFaceLandmarkerResult().addGestureWithConfidence(
          MediapipeFacialGesture.JAW_OPEN, 0.9);
      this.processFaceLandmarkerResult(result, false);
      assertTrue(this.getMouseController().isPrecisionActive());

      this.triggerBubbleControllerTimeout();
      assertEquals(
          'Mouse speed reduced, Open your mouth wide again to click',
          this.getBubbleText());

      // Turn off cursor control via pref.
      await this.setPref(PrefNames.CURSOR_CONTROL_ENABLED, false);

      // Ensure precision click is automatically toggled off.
      assertFalse(this.getMouseController().isPrecisionActive());
      assertEquals(this.getDefaultBubbleText(), this.getBubbleText());
    });

AX_TEST_F('FaceGazeMV2Test', 'PrecisionClickAndScrollMode', async function() {
  const gestureToMacroName =
      new Map()
          .set(FacialGesture.JAW_OPEN, MacroName.MOUSE_CLICK_LEFT)
          .set(FacialGesture.BROW_INNER_UP, MacroName.TOGGLE_SCROLL_MODE);
  const gestureToConfidence = new Map()
                                  .set(FacialGesture.JAW_OPEN, 0.6)
                                  .set(FacialGesture.BROW_INNER_UP, 0.6);
  const config = new Config()
                     .withMouseLocation({x: 600, y: 400})
                     .withBufferSize(1)
                     .withCursorControlEnabled(true)
                     .withBindings(gestureToMacroName, gestureToConfidence)
                     .withPrecisionEnabled(/*speedFactor=*/ 50);
  await this.startFacegazeWithConfigAndForeheadLocation_(config, 0.1, 0.2);

  // Start precision click by performing gesture assigned to left click.
  let result = new MockFaceLandmarkerResult().addGestureWithConfidence(
      MediapipeFacialGesture.JAW_OPEN, 0.9);
  this.processFaceLandmarkerResult(result);
  assertTrue(this.getMouseController().isPrecisionActive());

  assertEquals(
      'Start precision click (Open your mouth wide)', this.getBubbleText());

  this.triggerBubbleControllerTimeout();
  assertEquals(
      'Mouse speed reduced, Open your mouth wide again to click',
      this.getBubbleText());

  // Toggle scroll mode on. This should automatically stop precision click.
  result = new MockFaceLandmarkerResult().addGestureWithConfidence(
      MediapipeFacialGesture.BROW_INNER_UP, 0.9);
  this.processFaceLandmarkerResult(result);
  assertFalse(this.getMouseController().isPrecisionActive());
  assertTrue(this.getScrollModeController().active());
  assertEquals('Enter scroll mode (Raise eyebrows)', this.getBubbleText());

  // Only the scroll mode state message should be present.
  this.triggerBubbleControllerTimeout();
  assertEquals(
      'Scroll mode active. Raise eyebrows to exit. Other gestures ' +
          'temporarily unavailable.',
      this.getBubbleText());
});

AX_TEST_F(
    'FaceGazeMV2Test', 'OtherGesturesDontStartPrecisionClick',
    async function() {
      const gestureToMacroName =
          new Map()
              .set(FacialGesture.JAW_OPEN, MacroName.RESET_CURSOR)
              .set(FacialGesture.BROW_INNER_UP, MacroName.TOGGLE_DICTATION);
      const gestureToConfidence = new Map()
                                      .set(FacialGesture.JAW_OPEN, 0.6)
                                      .set(FacialGesture.BROW_INNER_UP, 0.6);
      const config = new Config()
                         .withBindings(gestureToMacroName, gestureToConfidence)
                         .withRepeatDelayMs(0)
                         .withPrecisionEnabled(/*speedFactor=*/ 50);
      await this.configureFaceGaze(config);

      let result = new MockFaceLandmarkerResult().addGestureWithConfidence(
          MediapipeFacialGesture.JAW_OPEN, 0.9);
      this.processFaceLandmarkerResult(result, false);
      assertFalse(this.getMouseController().isPrecisionActive());

      result = new MockFaceLandmarkerResult().addGestureWithConfidence(
          MediapipeFacialGesture.BROW_INNER_UP, 0.9);
      this.processFaceLandmarkerResult(result, false);
      assertFalse(this.getMouseController().isPrecisionActive());
    });

AX_TEST_F('FaceGazeMV2Test', 'PrecisionRightClickBubbleText', async function() {
  const gestureToMacroName =
      new Map().set(FacialGesture.JAW_OPEN, MacroName.MOUSE_CLICK_RIGHT);
  const gestureToConfidence = new Map().set(FacialGesture.JAW_OPEN, 0.3);
  const config = new Config()
                     .withBindings(gestureToMacroName, gestureToConfidence)
                     .withRepeatDelayMs(0)
                     .withPrecisionEnabled(/*speedFactor=*/ 50);
  await this.configureFaceGaze(config);

  assertNullOrUndefined(this.getBubbleText());

  // Start precision click by performing gesture assigned to right click.
  let result = new MockFaceLandmarkerResult().addGestureWithConfidence(
      MediapipeFacialGesture.JAW_OPEN, 0.9);
  this.processFaceLandmarkerResult(result, false);
  assertTrue(this.getMouseController().isPrecisionActive());
  assertEquals(
      'Start precision click (Open your mouth wide)', this.getBubbleText());

  // FaceGaze should display important messages about the state after the
  // timeout has elapsed.
  this.triggerBubbleControllerTimeout();
  assertEquals(
      'Mouse speed reduced, Open your mouth wide again to click',
      this.getBubbleText());

  // Perform the gesture again to right-click.
  result = new MockFaceLandmarkerResult().addGestureWithConfidence(
      MediapipeFacialGesture.JAW_OPEN, 0.9);
  this.processFaceLandmarkerResult(result, false);
  assertFalse(this.getMouseController().isPrecisionActive());
  assertEquals(
      'Right-click the mouse (Open your mouth wide)', this.getBubbleText());

  this.triggerBubbleControllerTimeout();
  assertEquals(this.getDefaultBubbleText(), this.getBubbleText());
});

AX_TEST_F(
    'FaceGazeMV2Test', 'PrecisionRightClickMouseEvents', async function() {
      const gestureToMacroName =
          new Map().set(FacialGesture.JAW_OPEN, MacroName.MOUSE_CLICK_RIGHT);
      const gestureToConfidence = new Map().set(FacialGesture.JAW_OPEN, 0.3);
      const config = new Config()
                         .withBindings(gestureToMacroName, gestureToConfidence)
                         .withRepeatDelayMs(0)
                         .withPrecisionEnabled(/*speedFactor=*/ 50);
      await this.configureFaceGaze(config);

      // Start precision click by performing gesture assigned to right click.
      let result = new MockFaceLandmarkerResult().addGestureWithConfidence(
          MediapipeFacialGesture.JAW_OPEN, 0.9);
      this.processFaceLandmarkerResult(result, false);
      assertTrue(this.getMouseController().isPrecisionActive());
      // Ensure no mouse events were sent.
      assertEquals(this.getMouseEvents().length, 0);

      // Perform the gesture again to right-click.
      result = new MockFaceLandmarkerResult().addGestureWithConfidence(
          MediapipeFacialGesture.JAW_OPEN, 0.9);
      this.processFaceLandmarkerResult(result, false);
      assertFalse(this.getMouseController().isPrecisionActive());

      const mouseEvents = this.getMouseEvents();
      assertEquals(mouseEvents.length, 2);
      this.assertMousePress(mouseEvents[0]);
      this.assertMouseRelease(mouseEvents[1]);
      assertEquals(
          chrome.accessibilityPrivate.SyntheticMouseEventButton.RIGHT,
          mouseEvents[0].mouseButton);
      assertEquals(
          chrome.accessibilityPrivate.SyntheticMouseEventButton.RIGHT,
          mouseEvents[1].mouseButton);
    });

AX_TEST_F('FaceGazeMV2Test', 'InvalidResult', async function() {
  const config = new Config();
  await this.configureFaceGaze(config);

  assertNullOrUndefined(this.getBubbleText());

  // Send an invalid result.
  let result = new MockFaceLandmarkerResult().invalidate();
  this.processFaceLandmarkerResult(result, false);
  assertEquals(
      `Can’t access camera. Turn on camera and make sure it isn’t blocked.`,
      this.getBubbleText());

  // Send a valid result.
  result = new MockFaceLandmarkerResult().addGestureWithConfidence(
      MediapipeFacialGesture.MOUTH_PUCKER, 0.2);
  this.processFaceLandmarkerResult(result, false);
  assertEquals(this.getDefaultBubbleText(), this.getBubbleText());
});

// Verifies that FaceGaze can handle scenarios where the camera is muted, which
// happens when the screen has been locked for a short amount of time, and then
// unmuted, which happens when the user signs back in.
AX_TEST_F('FaceGazeMV2Test', 'CameraMutedAndUnmuted', async function() {
  const config = new Config();
  await this.configureFaceGaze(config);

  // Mute the camera.
  this.getFaceGaze().webCamFaceLandmarker_.onTrackMutedHandler_();
  assertEquals(
      `Camera unavailable. Make sure you are signed in and camera is on.`,
      this.getBubbleText());

  // Unmute the camera.
  this.getFaceGaze().webCamFaceLandmarker_.onTrackUnmutedHandler_();
  assertEquals(this.getDefaultBubbleText(), this.getBubbleText());
});

// Verifies that FaceGaze can handle cases where no camera is available.
AX_TEST_F('FaceGazeMV2Test', 'NoCamera', async function() {
  // Pretend that there is no available camera.
  globalThis.navigator = {};
  navigator.mediaDevices = {};
  navigator.mediaDevices.getUserMedia = () => {
    throw new Error('Requested device not found');
  };

  // Verify initial state.
  const webCamFaceLandmarker = this.getFaceGaze().webCamFaceLandmarker_;
  assertEquals(10, webCamFaceLandmarker.connectToWebCamRetriesRemaining_);

  // Attempt to connect to the webcam. This should cause a message to appear
  // in the UI and queue up another attempt.
  webCamFaceLandmarker.connectToWebCam_();
  assertEquals(
      'Trying to connect to camera. Face control will turn off in 10 seconds.',
      this.getBubbleText());
  assertEquals(9, webCamFaceLandmarker.connectToWebCamRetriesRemaining_);

  // Pretend that the timeout has elapsed so that we try to reconnect to the
  // webcam.
  this.runLatestTimeout();
  assertEquals(
      'Trying to connect to camera. Face control will turn off in 9 seconds.',
      this.getBubbleText());
  assertEquals(8, webCamFaceLandmarker.connectToWebCamRetriesRemaining_);

  // Mock out the setPref API.
  let latestPref;
  let latestValue;
  chrome.settingsPrivate = {};
  chrome.settingsPrivate.setPref = (pref, value) => {
    latestPref = pref;
    latestValue = value;
  };

  // Pretend that we've exhausted our retry limit. The next failed attempt
  // will cause FaceGaze to be turned off.
  webCamFaceLandmarker.connectToWebCamRetriesRemaining_ = 0;
  this.runLatestTimeout();
  assertEquals('settings.a11y.face_gaze.enabled', latestPref);
  assertFalse(latestValue);
});
