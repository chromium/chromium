// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

GEN_INCLUDE(['facegaze_test_base.js']);

/** FazeGaze ScrollModeController tests. */
FaceGazeScrollModeControllerTest = class extends FaceGazeTestBase {
  /** @override */
  testGenPreamble() {
    super.testGenPreamble();
    super.testGenPreambleCommon(
        /*extensionIdName=*/ 'kAccessibilityCommonExtensionId',
        /*failOnConsoleError=*/ true);
  }

  /** @override */
  async setUpDeferred() {
    await super.setUpDeferred();

    // The coordinates for the center of the screen.
    this.center_ = {x: 600, y: 400};

    const config = new Config()
                       .withMouseLocation(this.center_)
                       .withBufferSize(1)
                       .withCursorControlEnabled(true);
    await this.startFacegazeWithConfigAndForeheadLocation_(config, 0.1, 0.2);
  }

  toggleScrollMode() {
    this.getFaceGaze().mouseController_.toggleScrollMode();
  }

  /** @return {!ScrollModeController} */
  getScrollModeController() {
    return this.getFaceGaze().mouseController_.scrollModeController_;
  }
};

AX_TEST_F('FaceGazeScrollModeControllerTest', 'Active', async function() {
  const controller = this.getScrollModeController();
  assertNotNullNorUndefined(controller);
  assertNullOrUndefined(controller.mouseLocation_);
  assertNullOrUndefined(controller.center_);
  assertFalse(controller.active());

  this.toggleScrollMode();
  assertTrue(controller.active());
  assertNotNullNorUndefined(controller.mouseLocation_);
  assertNotNullNorUndefined(controller.center_);

  this.toggleScrollMode();
  assertFalse(controller.active());
  assertNullOrUndefined(controller.mouseLocation_);
  assertNullOrUndefined(controller.center_);
});

AX_TEST_F('FaceGazeScrollModeControllerTest', 'CenterPoint', async function() {
  const controller = this.getScrollModeController();
  this.toggleScrollMode();
  // Ensure the controller's center point is properly initialized.
  assertEquals(this.center_.x, controller.center_.x);
  assertEquals(this.center_.y, controller.center_.y);
});

AX_TEST_F(
    'FaceGazeScrollModeControllerTest', 'GetDirectionUndefined',
    async function() {
      const controller = this.getScrollModeController();

      // We should not scroll if ScrollModeController is inactive.
      assertFalse(controller.active());
      assertNullOrUndefined(controller.getDirection_({x: 1000, y: 1000}));

      this.toggleScrollMode();
      assertTrue(controller.active());

      // We should not scroll if the new location is within the threshold value,
      // e.g. if the point is on or within a circle that is centered at
      // controller.center_ and has radius of
      // ScrollModeController.DELTA_THRESHOLD.

      const pointsWithinCircle = [
        {
          x: this.center_.x,
          y: this.center_.y,
        },
        {
          x: this.center_.x + ScrollModeController.DELTA_THRESHOLD / 2,
          y: this.center_.y + ScrollModeController.DELTA_THRESHOLD / 2,
        },
      ];

      const pointsOnCircle = [
        {
          x: this.center_.x + ScrollModeController.DELTA_THRESHOLD,
          y: this.center_.y,
        },
        {
          x: this.center_.x - ScrollModeController.DELTA_THRESHOLD,
          y: this.center_.y,
        },
        {
          x: this.center_.x,
          y: this.center_.y + ScrollModeController.DELTA_THRESHOLD,
        },
        {
          x: this.center_.x,
          y: this.center_.y - ScrollModeController.DELTA_THRESHOLD,
        },
      ];

      // Test points that are outside of the circle, but are at ambiguous angles
      // of the circle, e.g. 45 degrees, 135 degrees, 225 degrees, and 315
      // degrees.

      // Use additional delta to ensure that points lie outside of the circle.
      const additionalDelta = ScrollModeController.DELTA_THRESHOLD * 2;
      const pointsAtAmbiguousAngles = [
        {
          x: this.center_.x + additionalDelta,
          y: this.center_.y + additionalDelta,
        },
        {
          x: this.center_.x + additionalDelta,
          y: this.center_.y - additionalDelta,
        },
        {
          x: this.center_.x - additionalDelta,
          y: this.center_.y + additionalDelta,
        },
        {
          x: this.center_.x - additionalDelta,
          y: this.center_.y - additionalDelta,
        },
      ];

      for (const point of pointsWithinCircle.concat(pointsOnCircle)
               .concat(pointsAtAmbiguousAngles)) {
        assertNullOrUndefined(
            controller.getDirection_(point),
            `Expected point (${point.x}, ${
                point.y}) to yield an undefined direction`);
      }

      this.toggleScrollMode();
      // We should not scroll if ScrollModeController is inactive.
      assertFalse(controller.active());
      assertNullOrUndefined(controller.getDirection_({x: 1000, y: 1000}));
    });

AX_TEST_F(
    'FaceGazeScrollModeControllerTest', 'GetDirectionValid', async function() {
      const controller = this.getScrollModeController();
      this.toggleScrollMode();

      // We should scroll if the new location is outside the threshold value,
      // e.g. if the point is outside a circle that is centered at
      // controller.center_ and has radius of
      // ScrollModeController.DELTA_THRESHOLD.

      const additionalDelta = ScrollModeController.DELTA_THRESHOLD * 2;

      const pointsUp = [
        // Creates an angle of 45.1 degrees with the positive x-axis.
        {
          x: this.center_.x + additionalDelta - 1,
          y: this.center_.y - additionalDelta,
        },
        // Creates an angle of 90 degrees with the positive x-axis.
        {
          x: this.center_.x,
          y: this.center_.y - additionalDelta,
        },
        // Creates an angle of 134.8 degrees with the positive x-axis.
        {
          x: this.center_.x - additionalDelta + 1,
          y: this.center_.y - additionalDelta,
        },
      ];

      for (const point of pointsUp) {
        assertEquals(
            this.scrollDirection.UP, controller.getDirection_(point),
            `Expected point (${point.x}, ${
                point.y}) to yield a direction of UP`);
      }

      const pointsDown = [
        // Creates an angle of -45.1 degrees with the positive x-axis.
        {
          x: this.center_.x + additionalDelta - 1,
          y: this.center_.y + additionalDelta,
        },
        // Creates an angle of -90 degrees with the positive x-axis.
        {
          x: this.center_.x,
          y: this.center_.y + additionalDelta,
        },
        // Creates an angle of -134.8 degrees with the positive x-axis.
        {
          x: this.center_.x - additionalDelta + 1,
          y: this.center_.y + additionalDelta,
        },
      ];

      for (const point of pointsDown) {
        assertEquals(
            this.scrollDirection.DOWN, controller.getDirection_(point),
            `Expected point (${point.x}, ${
                point.y}) to yield a direction of DOWN`);
      }

      const pointsLeft = [
        // Creates an angle of 135.1 degrees with the positive x-axis.
        {
          x: this.center_.x - additionalDelta - 1,
          y: this.center_.y - additionalDelta,
        },
        // Creates an angle of 180 degrees with the positive x-axis.
        {
          x: this.center_.x - additionalDelta,
          y: this.center_.y,
        },
        // Creates an angle of -135.1 degrees with the positive x-axis.
        {
          x: this.center_.x - additionalDelta - 1,
          y: this.center_.y + additionalDelta,
        },
      ];

      for (const point of pointsLeft) {
        assertEquals(
            this.scrollDirection.LEFT, controller.getDirection_(point),
            `Expected point (${point.x}, ${
                point.y}) to yield a direction of LEFT`);
      }

      const pointsRight = [
        // Creates an angle of 0 degrees with the positive x-axis.
        {
          x: this.center_.x + additionalDelta,
          y: this.center_.y,
        },
        // Creates an angle of 44.8 degrees with the positive x-axis.
        {
          x: this.center_.x + additionalDelta,
          y: this.center_.y - additionalDelta + 1,
        },
        // Creates an angle of -44.8 degrees with the positive x-axis.
        {
          x: this.center_.x + additionalDelta,
          y: this.center_.y + additionalDelta - 1,
        },
      ];

      for (const point of pointsRight) {
        assertEquals(
            this.scrollDirection.RIGHT, controller.getDirection_(point),
            `Expected point (${point.x}, ${
                point.y}) to yield a direction of RIGHT`);
      }
    });

AX_TEST_F(
    'FaceGazeScrollModeControllerTest', 'CallsApiOnValidPoint',
    async function() {
      const controller = this.getScrollModeController();
      this.toggleScrollMode();
      const point = {
        x: this.center_.x + ScrollModeController.DELTA_THRESHOLD + 1,
        y: this.center_.y,
      };

      controller.scroll(point);
      const mockApi = this.mockAccessibilityPrivate;
      assertEquals(1, mockApi.getScrollAtPointCount());
      // The scroll target is wherever the mouse is when scroll mode is toggled.
      // In this case, it's the center.
      assertEquals(this.center_.x, mockApi.getScrollAtPointTarget().x);
      assertEquals(this.center_.y, mockApi.getScrollAtPointTarget().y);
      assertEquals(
          this.scrollDirection.RIGHT, mockApi.getScrollAtPointDirection());
      assertNotEquals(0, controller.lastScrollTime_);
    });

AX_TEST_F(
    'FaceGazeScrollModeControllerTest', 'SkipsApiOnInvalidPoint',
    async function() {
      const controller = this.getScrollModeController();
      this.toggleScrollMode();
      controller.scroll(this.center_);

      const mockApi = this.mockAccessibilityPrivate;
      assertEquals(0, mockApi.getScrollAtPointCount());
      assertNullOrUndefined(mockApi.getScrollAtPointTarget());
      assertNullOrUndefined(mockApi.getScrollAtPointDirection());
    });
