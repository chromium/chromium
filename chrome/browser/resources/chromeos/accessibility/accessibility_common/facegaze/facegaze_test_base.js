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
    /** @type {?Map<FacialGesture, Action>} */
    this.gestureToAction = null;
    /** @type {?Map<FacialGesture, number>} */
    this.gestureToConfidence = null;
    /** @type {number} */
    this.bufferSize = -1;
    /** @type {boolean} */
    this.useMouseAcceleration = false;
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
   * @param {!Map<FacialGesture, Action>} gestureToAction
   * @return {!Config}
   */
  withGestureToAction(gestureToAction) {
    this.gestureToAction = gestureToAction;
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

  /**
   * @return {!Config}
   */
  withMouseAcceleration() {
    this.useMouseAcceleration = true;
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
   * @param {string} name
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
  /** @override */
  async setUpDeferred() {
    await super.setUpDeferred();
    this.mockAccessibilityPrivate = new MockAccessibilityPrivate();
    chrome.accessibilityPrivate = this.mockAccessibilityPrivate;

    this.intervalCallbacks_ = {};
    this.nextCallbackId_ = 1;

    window.setInterval = (callback, timeout) => {
      const id = this.nextCallbackId_;
      this.nextCallbackId_++;
      this.intervalCallbacks_[id] = callback;
      return id;
    };
    window.clearInterval = (id) => {
      delete this.intervalCallbacks_[id];
    };

    // Re-initialize AccessibilityCommon with mock AccessibilityPrivate API.
    const module =
        await import('/accessibility_common/accessibility_common_loader.js');
    await importModule(
        ['Action', 'FaceGaze'], '/accessibility_common/facegaze/facegaze.js');
    await importModule(
        ['FacialGesture'],
        '/accessibility_common/facegaze/gesture_detector.js');
    accessibilityCommon = new module.AccessibilityCommon();
    assertNotNullNorUndefined(accessibilityCommon);
    assertNotNullNorUndefined(Action);
    assertNotNullNorUndefined(FaceGaze);
    assertNotNullNorUndefined(FacialGesture);
    accessibilityCommon.faceGaze_ = new FaceGaze();
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

  /** @param {!Config} config */
  configureFaceGaze(config) {
    const faceGaze = this.getFaceGaze();
    if (config.mouseLocation) {
      // TODO(b/309121742): Set the mouse location using a fake automation
      // event.
      faceGaze.mouseController_.mouseLocation_ = config.mouseLocation;
    }

    if (config.gestureToAction) {
      faceGaze.gestureToAction_ = new Map(config.gestureToAction);
    }

    if (config.gestureToConfidence) {
      faceGaze.gestureToConfidence_ = new Map(config.gestureToConfidence);
    }

    if (config.bufferSize !== -1) {
      faceGaze.mouseController_.targetBufferSize_ = config.bufferSize;
    }

    faceGaze.mouseController_.useMouseAcceleration_ =
        config.useMouseAcceleration;

    return new Promise(resolve => {
      faceGaze.setOnInitCallbackForTest(resolve);
    });
  }

  triggerMouseControllerInterval() {
    const intervalId = this.getFaceGaze().mouseController_.mouseInterval_;
    assertNotEquals(-1, intervalId);
    assertNotNullNorUndefined(this.intervalCallbacks_[intervalId]);
    this.intervalCallbacks_[intervalId]();
  }

  /**
   * @param {!MockFaceLandmarkerResult} result
   * @param {boolean} triggerMouseControllerInterval
   */
  processFaceLandmarkerResult(result, triggerMouseControllerInterval) {
    this.getFaceGaze().processFaceLandmarkerResult_(result);

    if (triggerMouseControllerInterval) {
      // Manually trigger the mouse interval one time.
      this.triggerMouseControllerInterval();
    }
  }
};
