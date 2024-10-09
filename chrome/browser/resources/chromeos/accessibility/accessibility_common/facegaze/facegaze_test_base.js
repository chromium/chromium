// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

GEN_INCLUDE(['../../common/testing/e2e_test_base.js']);
GEN_INCLUDE(['../../common/testing/mock_accessibility_private.js']);

/** A class that helps initialize FaceGaze with a configuration. */
class Config {
  constructor() {
    /** @type {?chrome.accessibilityPrivate.ScreenPoint} */
    this.mouseLocation = null;
    /** @type {?Map<FacialGesture, MacroName>} */
    this.gestureToMacroName = null;
    /** @type {?Map<FacialGesture, number>} */
    this.gestureToConfidence = null;
    /** @type {number} */
    this.bufferSize = -1;
    /** @type {boolean} */
    this.useMouseAcceleration = false;
    /** @type {boolean} */
    this.useLandmarkWeights = false;
    /** @type {boolean} */
    this.useVelocityThreshold = false;
    /** @type {Map<string, number>} */
    this.speeds = {up: 20, down: 20, left: 20, right: 20};
    /** @type {number} */
    this.repeatDelayMs = undefined;
    /** @type {boolean} */
    this.cursorControlEnabled = true;
    /** @type {boolean} */
    this.actionsEnabled = true;
  }

  /**
   * @param {!chrome.accessibilityPrivate.ScreenPoint} mouseLocation
   * @return {!Config}
   */
  withMouseLocation(mouseLocation) {
    this.mouseLocation = mouseLocation;
    return this;
  }

  /**
   * @param {!Map<FacialGesture, MacroName>} gestureToMacroName
   * @return {!Config}
   */
  withGestureToMacroName(gestureToMacroName) {
    this.gestureToMacroName = gestureToMacroName;
    return this;
  }

  /**
   * @param {!Map<FacialGesture, number>} gestureToConfidence
   * @return {!Config}
   */
  withGestureToConfidence(gestureToConfidence) {
    this.gestureToConfidence = gestureToConfidence;
    return this;
  }

  /**
   * @param {number} bufferSize
   * @return {!Config}
   */
  withBufferSize(bufferSize) {
    this.bufferSize = bufferSize;
    return this;
  }

  /** @return {!Config} */
  withMouseAcceleration() {
    this.useMouseAcceleration = true;
    return this;
  }

  /** @return {!Config} */
  withLandmarkWeights() {
    this.useLandmarkWeights = true;
    return this;
  }

  /** @return {!Config} */
  withVelocityThreshold() {
    this.useVelocityThreshold = true;
    return this;
  }

  /**
   * @param {number} up
   * @param {number} down
   * @param {number} left
   * @param {number} right
   * @return {!Config}
   */
  withSpeeds(up, down, left, right) {
    this.speeds = {up, down, left, right};
    return this;
  }

  /**
   * @param {number} repeatDelayMs
   * @return {!Config}
   */
  withRepeatDelayMs(repeatDelayMs) {
    this.repeatDelayMs = repeatDelayMs;
    return this;
  }

  /**
   * @param {boolean} cursorControlEnabled
   * @return {!Config}
   */
  withCursorControlEnabled(cursorControlEnabled) {
    this.cursorControlEnabled = cursorControlEnabled;
    return this;
  }

  /**
   * @param {boolean} actionsEnabled
   * @return {!Config}
   */
  withActionsEnabled(actionsEnabled) {
    this.actionsEnabled = actionsEnabled;
    return this;
  }
}

/** A class that represents a fake FaceLandmarkerResult. */
class MockFaceLandmarkerResult {
  constructor() {
    /**
     * Holds face landmark results. The landmark used by FaceGaze is the
     * forehead landmark, which corresponds to index 8.
     * @type {!Array<?Array<?<x: number, y: number, z: number>>>}
     */
    this.faceLandmarks =
        [[null, null, null, null, null, null, null, null, null]];

    /** @type {!Array<!Object>} */
    this.faceBlendshapes = [{categories: []}];
  }

  /**
   * @param {number} x
   * @param {number} y
   * @param {number} z
   * @return {!MockFaceLandmarkerResult}
   */
  setNormalizedForeheadLocation(x, y, z = 0) {
    this.faceLandmarks[0][8] = {x, y, z};
    return this;
  }

  /**
   * @param {MediapipeFacialGesture} name
   * @param {number} confidence
   * @return {!MockFaceLandmarkerResult}
   */
  addGestureWithConfidence(name, confidence) {
    const data = {
      categoryName: name,
      score: confidence,
    };

    this.faceBlendshapes[0].categories.push(data);
    return this;
  }
}

/** Base class for FaceGaze tests JavaScript tests. */
FaceGazeTestBase = class extends E2ETestBase {
  constructor() {
    super();
    this.overrideIntervalFunctions_ = true;
  }

  /** @override */
  async setUpDeferred() {
    await super.setUpDeferred();
    this.mockAccessibilityPrivate = new MockAccessibilityPrivate();
    chrome.accessibilityPrivate = this.mockAccessibilityPrivate;

    this.scrollDirection = this.mockAccessibilityPrivate.ScrollDirection;

    if (this.overrideIntervalFunctions_) {
      this.intervalCallbacks_ = {};
      this.nextIntervalId_ = 1;
      this.timeoutCallbacks_ = {};
      this.nextTimeoutId_ = 1;

      // Save the original set and clear interval functions so they can be used
      // in this file.
      window.setIntervalOriginal = window.setInterval;
      window.clearIntervalOriginal = window.clearInterval;

      window.setTimeout = (callback, timeout) => {
        const id = this.nextTimeoutId_;
        ++this.nextTimeoutId_;
        this.timeoutCallbacks_[id] = callback;
        return id;
      };
      window.clearTimeout = (id) => {
        delete this.timeoutCallbacks_[id];
      };

      window.setInterval = (callback, timeout) => {
        const id = this.nextIntervalId_;
        this.nextIntervalId_++;
        this.intervalCallbacks_[id] = callback;
        return id;
      };
      window.clearInterval = (id) => {
        delete this.intervalCallbacks_[id];
      };
    }

    assertNotNullNorUndefined(accessibilityCommon);
    assertNotNullNorUndefined(BubbleController);
    assertNotNullNorUndefined(FaceGaze);
    assertNotNullNorUndefined(FacialGesture);
    assertNotNullNorUndefined(FacialGesturesToMediapipeGestures);
    assertNotNullNorUndefined(GestureDetector);
    assertNotNullNorUndefined(GestureHandler);
    assertNotNullNorUndefined(MacroName);
    assertNotNullNorUndefined(MediapipeFacialGesture);
    assertNotNullNorUndefined(MetricsUtils);
    assertNotNullNorUndefined(MouseController);
    assertNotNullNorUndefined(ScrollModeController);
    assertNotNullNorUndefined(WebCamFaceLandmarker);
    await new Promise(resolve => {
      accessibilityCommon.setFeatureLoadCallbackForTest('facegaze', resolve);
    });
  }

  /** @override */
  testGenCppIncludes() {
    super.testGenCppIncludes();
    GEN(`
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "chrome/browser/ash/accessibility/accessibility_manager.h"
#include "ui/accessibility/accessibility_features.h"
    `);
  }

  /** @override */
  testGenPreamble() {
    super.testGenPreamble();
    GEN(`base::OnceClosure load_cb =
        base::BindOnce(&ash::AccessibilityManager::EnableFaceGaze,
            base::Unretained(ash::AccessibilityManager::Get()), true);`);
  }

  /** @override */
  get featureList() {
    return {enabled: ['features::kAccessibilityFaceGaze']};
  }

  /** @return {!FaceGaze} */
  getFaceGaze() {
    return accessibilityCommon.getFaceGazeForTest();
  }

  /** @return {!ScrollModeController} */
  getScrollModeController() {
    return this.getFaceGaze().mouseController_.scrollModeController_;
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
    if (config.cursorControlEnabled && !config.useVelocityThreshold) {
      this.assertLatestCursorPosition(config.mouseLocation);
    } else {
      assertEquals(
          null, this.mockAccessibilityPrivate.getLatestCursorPosition(),
          'Expected cursor position to be null');
    }
  }

  /** @param {!Config} config */
  async configureFaceGaze(config) {
    const faceGaze = this.getFaceGaze();
    if (config.mouseLocation) {
      // TODO(b/309121742): Set the mouse location using a fake automation
      // event.
      faceGaze.mouseController_.mouseLocation_ = config.mouseLocation;
    }

    if (config.gestureToMacroName) {
      const gestureToMacroName = {};
      for (const [gesture, macroName] of config.gestureToMacroName) {
        gestureToMacroName[gesture] = macroName;
      }
      await this.setPref(
          GestureHandler.GESTURE_TO_MACRO_PREF, gestureToMacroName);
    }

    if (config.gestureToConfidence) {
      const gestureToConfidence = {};
      for (const [gesture, confidence] of config.gestureToConfidence) {
        gestureToConfidence[gesture] = confidence * 100;
      }
      await this.setPref(
          GestureHandler.GESTURE_TO_CONFIDENCE_PREF, gestureToConfidence);
    }

    if (config.bufferSize !== -1) {
      await this.setPref(
          MouseController.PREF_CURSOR_SMOOTHING, config.bufferSize);
    }

    await this.setPref(MouseController.PREF_SPD_UP, config.speeds.up);
    await this.setPref(MouseController.PREF_SPD_DOWN, config.speeds.down);
    await this.setPref(MouseController.PREF_SPD_LEFT, config.speeds.left);
    await this.setPref(MouseController.PREF_SPD_RIGHT, config.speeds.right);

    if (config.repeatDelayMs !== undefined) {
      faceGaze.gestureHandler_.repeatDelayMs_ = config.repeatDelayMs;
    }

    // Increase the bubble controller timeout to avoid flaky behavior when
    // asserting the text content of the bubble.
    BubbleController.CLEAR_BUBBLE_TIMEOUT_MS = 100 * 1000;

    faceGaze.mouseController_.setLandmarkWeightsForTesting(
        config.useLandmarkWeights);

    faceGaze.mouseController_.setVelocityThresholdForTesting(
        config.useVelocityThreshold);

    await this.setPref(
        MouseController.PREF_CURSOR_USE_ACCELERATION,
        config.useMouseAcceleration);
    assertEquals(
        faceGaze.mouseController_.useMouseAcceleration_,
        config.useMouseAcceleration);

    await this.setPref(
        FaceGaze.PREF_CURSOR_CONTROL_ENABLED, config.cursorControlEnabled);
    assertEquals(faceGaze.cursorControlEnabled_, config.cursorControlEnabled);

    await this.setPref(FaceGaze.PREF_ACTIONS_ENABLED, config.actionsEnabled);
    assertEquals(faceGaze.actionsEnabled_, config.actionsEnabled);

    if (config.cursorControlEnabled) {
      // The MouseController gets constructed and started before this test
      // fixture gets created. To make these tests work, we need to explicitly
      // restart the MouseController so that we can insert custom hooks for the
      // set/clearInterval functions, which is necessary to control timing of
      // these tests.
      await this.restartMouseController();
    }

    return new Promise(resolve => {
      faceGaze.setOnInitCallbackForTest(resolve);
    });
  }

  triggerMouseControllerInterval() {
    const intervalId = this.getFaceGaze().mouseController_.mouseInterval_;
    if (this.getFaceGaze().cursorControlEnabled_ &&
        !this.getFaceGaze().mouseController_.paused_) {
      assertNotEquals(
          -1, intervalId, 'Expected valid MouseController interval');
      assertNotNullNorUndefined(
          this.intervalCallbacks_[intervalId],
          'Expected valid MouseController callback');
      this.intervalCallbacks_[intervalId]();
    } else {
      // No work to do.
      assertEquals(-1, intervalId);
    }
  }

  triggerBubbleControllerTimeout() {
    const intervalId =
        this.getFaceGaze().bubbleController_.resetBubbleTimeoutId_;
    this.timeoutCallbacks_[intervalId]();
  }

  /**
   * @param {!MockFaceLandmarkerResult} result
   * @param {boolean} triggerMouseControllerInterval
   */
  processFaceLandmarkerResult(result, triggerMouseControllerInterval = true) {
    this.getFaceGaze().processFaceLandmarkerResult_(result);

    if (triggerMouseControllerInterval) {
      // Manually trigger the mouse interval one time.
      this.triggerMouseControllerInterval();
    }
  }

  /** Clears the timestamps at which gestures were last recognized. */
  clearGestureLastRecognizedTime() {
    this.getFaceGaze().gestureHandler_.gestureLastRecognized_.clear();
  }

  /**
   * Sends a mock automation mouse event to the Mouse Controller.
   * @param {chrome.automation.AutomationEvent} mockEvent
   */
  sendAutomationMouseEvent(mockEvent) {
    this.getFaceGaze().mouseController_.onMouseMovedHandler_.handleEvent_(
        mockEvent);
  }

  /** @param {!{x: number, y: number}} expected */
  assertLatestCursorPosition(expected) {
    const actual = this.mockAccessibilityPrivate.getLatestCursorPosition();
    assertEquals(
        expected.x, actual.x,
        'Failed to assert latest cursor position x value');
    assertEquals(
        expected.y, actual.y,
        'Failed to assert latest cursor position y value');
  }

  /** @param {number} num */
  assertNumMouseEvents(num) {
    assertEquals(
        num, this.mockAccessibilityPrivate.syntheticMouseEvents_.length);
  }

  /** @param {number} num */
  assertNumKeyEvents(num) {
    assertEquals(num, this.mockAccessibilityPrivate.syntheticKeyEvents_.length);
  }

  /** @param {!chrome.accessibilityPrivate.SyntheticMouseEvent} event */
  assertMousePress(event) {
    assertEquals(
        this.mockAccessibilityPrivate.SyntheticMouseEventType.PRESS,
        event.type);
  }

  /** @param {!chrome.accessibilityPrivate.SyntheticMouseEvent} event */
  assertMouseRelease(event) {
    assertEquals(
        this.mockAccessibilityPrivate.SyntheticMouseEventType.RELEASE,
        event.type);
  }

  /** @param {!chrome.accessibilityPrivate.SyntheticKeyboardEvent} event */
  assertKeyDown(event) {
    assertEquals(
        chrome.accessibilityPrivate.SyntheticKeyboardEventType.KEYDOWN,
        event.type);
  }

  /** @param {!chrome.accessibilityPrivate.SyntheticKeyboardEvent} event */
  assertKeyUp(event) {
    assertEquals(
        chrome.accessibilityPrivate.SyntheticKeyboardEventType.KEYUP,
        event.type);
  }

  /** @return {!Array<!chrome.accessibilityPrivate.SyntheticMouseEvent>} */
  getMouseEvents() {
    return this.mockAccessibilityPrivate.syntheticMouseEvents_;
  }

  /** @return {!Array<!chrome.accessibilityPrivate.SyntheticKeyboardEvent>} */
  getKeyEvents() {
    return this.mockAccessibilityPrivate.syntheticKeyEvents_;
  }

  async restartMouseController() {
    this.getFaceGaze().mouseController_.stop();
    await this.getFaceGaze().mouseController_.start();
    await this.waitForValidMouseInterval();
  }

  /** Waits for the mouse controller to initialize its interval function. */
  async waitForValidMouseInterval() {
    if (this.getFaceGaze().mouseController_.mouseInterval_ !== -1) {
      return;
    }

    await new Promise((resolve) => {
      const intervalId = setIntervalOriginal(() => {
        if (this.getFaceGaze().mouseController_.mouseInterval_ !== -1) {
          clearIntervalOriginal(intervalId);
          resolve();
        }
      }, 300);
    });
  }
};
