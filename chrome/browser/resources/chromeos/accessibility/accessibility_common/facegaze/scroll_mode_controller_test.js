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
    const config = new Config()
                       .withMouseLocation({x: 600, y: 400})
                       .withBufferSize(1)
                       .withCursorControlEnabled(true);
    await this.startFacegazeWithConfigAndForeheadLocation_(config, 0.1, 0.2);
  }

  toggleScrollMode() {
    this.getFaceGaze().mouseController_.toggleScrollMode();
  }

  /** @param {boolean} enabled */
  async waitForCursorControlPref(enabled) {
    const matches = async () => {
      const pref = await this.getPref(FaceGaze.PREF_CURSOR_CONTROL_ENABLED);
      return pref.value === enabled;
    };

    const done = await matches();
    if (done) {
      return;
    }

    await new Promise((resolve) => {
      const intervalId = setIntervalOriginal(async () => {
        const done = await matches();
        if (done) {
          clearIntervalOriginal(intervalId);
          resolve();
        }
      }, 300);
    });
  }
};

AX_TEST_F('FaceGazeScrollModeControllerTest', 'Active', function() {
  const controller = this.getScrollModeController();
  assertNotNullNorUndefined(controller);
  assertNullOrUndefined(controller.scrollLocation_);
  assertNullOrUndefined(controller.screenBounds_);
  assertFalse(controller.active());

  this.toggleScrollMode();
  assertTrue(controller.active());
  assertNotNullNorUndefined(controller.scrollLocation_);
  assertNotNullNorUndefined(controller.screenBounds_);

  this.toggleScrollMode();
  assertFalse(controller.active());
  assertNullOrUndefined(controller.scrollLocation_);
  assertNullOrUndefined(controller.screenBounds_);
});

AX_TEST_F('FaceGazeScrollModeControllerTest', 'ScreenBounds', function() {
  const controller = this.getScrollModeController();
  this.toggleScrollMode();
  const bounds = this.mockAccessibilityPrivate.displayBounds_[0];
  // Ensure the controller's screen bounds are properly initialized.
  assertEquals(bounds.left, controller.screenBounds_.left);
  assertEquals(bounds.top, controller.screenBounds_.top);
  assertEquals(bounds.width, controller.screenBounds_.width);
  assertEquals(bounds.height, controller.screenBounds_.height);
});

AX_TEST_F(
    'FaceGazeScrollModeControllerTest', 'CallsApiOnValidPoint', function() {
      const controller = this.getScrollModeController();
      this.toggleScrollMode();

      const bounds = this.mockAccessibilityPrivate.displayBounds_[0];

      // Points at the edge of the screen will trigger a scroll.
      const topEdge = {x: 400, y: bounds.top};
      const bottomEdge = {x: 400, y: bounds.height};
      const leftEdge = {x: bounds.left, y: 400};
      const rightEdge = {x: bounds.width, y: 400};

      // Points at the top/bottom of the screen within the vertical cushion will
      // also trigger a scroll.
      const increment = bounds.height / 20;
      const topWithinVerticalCushion = {x: 400, y: bounds.top + increment};
      const bottomWithinVerticalCushion = {
        x: 400,
        y: bounds.height - increment,
      };

      const testCases = [
        {point: topEdge, expectedDirection: this.scrollDirection.UP},
        {point: bottomEdge, expectedDirection: this.scrollDirection.DOWN},
        {point: leftEdge, expectedDirection: this.scrollDirection.LEFT},
        {point: rightEdge, expectedDirection: this.scrollDirection.RIGHT},
        {
          point: topWithinVerticalCushion,
          expectedDirection: this.scrollDirection.UP,
        },
        {
          point: bottomWithinVerticalCushion,
          expectedDirection: this.scrollDirection.DOWN,
        },
      ];

      const mockApi = this.mockAccessibilityPrivate;
      ScrollModeController.RATE_LIMIT = 0;
      for (const testCase of testCases) {
        const {point, expectedDirection} = testCase;
        controller.scroll(point);
        // The scroll target is wherever the mouse is when scroll mode is
        // toggled.
        assertEquals(600, mockApi.getScrollAtPointTarget().x);
        assertEquals(400, mockApi.getScrollAtPointTarget().y);
        assertEquals(expectedDirection, mockApi.getScrollAtPointDirection());
        assertNotEquals(0, controller.lastScrollTime_);
      }

      assertEquals(6, mockApi.getScrollAtPointCount());
    });

AX_TEST_F(
    'FaceGazeScrollModeControllerTest', 'SkipsApiOnInvalidPoint', function() {
      const controller = this.getScrollModeController();
      this.toggleScrollMode();
      controller.scroll({x: 600, y: 400});

      const mockApi = this.mockAccessibilityPrivate;
      assertEquals(0, mockApi.getScrollAtPointCount());
      assertNullOrUndefined(mockApi.getScrollAtPointTarget());
      assertNullOrUndefined(mockApi.getScrollAtPointDirection());
    });

AX_TEST_F(
    'FaceGazeScrollModeControllerTest', 'RespectsPhysicalMouseEvents',
    function() {
      const controller = this.getScrollModeController();
      const mockApi = this.mockAccessibilityPrivate;
      const bounds = mockApi.displayBounds_[0];
      const topEdge = {x: 400, y: bounds.top};
      ScrollModeController.RATE_LIMIT = 0;

      this.toggleScrollMode();
      controller.scroll(topEdge);
      assertEquals(1, mockApi.getScrollAtPointCount());
      // The scroll target is wherever the mouse is when scroll mode is toggled.
      assertEquals(600, mockApi.getScrollAtPointTarget().x);
      assertEquals(400, mockApi.getScrollAtPointTarget().y);

      // Simulate a mouse moved event, which happens if the user moves the mouse
      // using the touchpad or an external mouse.
      this.sendAutomationMouseEvent(
          {mouseX: 350, mouseY: 250, eventFrom: 'user'});

      // Ensure the scroll target is updated to respect the new mouse location.
      controller.scroll(topEdge);
      assertEquals(2, mockApi.getScrollAtPointCount());
      assertEquals(350, mockApi.getScrollAtPointTarget().x);
      assertEquals(250, mockApi.getScrollAtPointTarget().y);
    });

AX_TEST_F(
    'FaceGazeScrollModeControllerTest', 'CursorControlOriginallyOff',
    async function() {
      // Force cursor control off.
      await this.setPref(FaceGaze.PREF_CURSOR_CONTROL_ENABLED, false);
      await this.waitForCursorControlPref(false);

      // Ensure cursor control is toggled on automatically.
      this.toggleScrollMode();
      await this.waitForCursorControlPref(true);

      // Ensure cursor control is reset to its original value.
      this.toggleScrollMode();
      await this.waitForCursorControlPref(false);
    });

AX_TEST_F(
    'FaceGazeScrollModeControllerTest', 'CursorControlOriginallyOn',
    async function() {
      await this.waitForCursorControlPref(true);

      this.toggleScrollMode();
      await this.waitForCursorControlPref(true);

      // Ensure cursor control is still on, since it was on when scroll mode was
      // originally toggled.
      this.toggleScrollMode();
      await this.waitForCursorControlPref(true);
    });

AX_TEST_F(
    'FaceGazeScrollModeControllerTest',
    'AutoTogglesCursorControlIfCursorControlModifiedDuringScrollMode',
    async function() {
      await this.waitForCursorControlPref(true);

      this.toggleScrollMode();
      await this.waitForCursorControlPref(true);

      // Manually turn cursor control off.
      await this.setPref(FaceGaze.PREF_CURSOR_CONTROL_ENABLED, false);
      await this.waitForCursorControlPref(false);

      // Cursor control will be turned back on, since it was on when scroll mode
      // was originally toggled.
      this.toggleScrollMode();
      await this.waitForCursorControlPref(true);
    });
