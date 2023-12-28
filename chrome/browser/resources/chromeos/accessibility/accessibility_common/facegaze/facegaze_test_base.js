// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

GEN_INCLUDE(['../../common/testing/e2e_test_base.js']);
GEN_INCLUDE(['../../common/testing/mock_accessibility_private.js']);

/** A class that represents a fake FaceLandmarkerResult. */
class MockFaceLandmarkerResult {
  constructor() {
    /**
     * Holds face landmark results. The landmark used by FaceGaze is the
     * forehead landmark, which corresponds to index 8.
     * @type {!Array<?Array<?<x: number, y: number, z: number>>>}
     */
    this.faceLandmarks = [[null, null, null, null, null, null, null, null, []]];

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

    // Re-initialize AccessibilityCommon with mock AccessibilityPrivate API.
    const module =
        await import('/accessibility_common/accessibility_common_loader.js');
    await importModule(
        ['Action', 'FaceGaze', 'FacialGesture'],
        '/accessibility_common/facegaze/facegaze.js');
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

  /**
   * @param {!FacialGesture} gesture
   * @return {?Action}
   */
  getActionForGesture(gesture) {
    const data = this.getFaceGaze().gestureToActionData_.get(gesture);
    if (!data) {
      return null;
    }

    return data.action;
  }

  /**
   * TODO(b/309121742): Set the mouse location using a fake automation event.
   * @param {!chrome.accessibilityPrivate.ScreenPoint} location
   */
  setMouseLocation(location) {
    this.getFaceGaze().mouseLocation_ = location;
  }

  /** @param {!MockFaceLandmarkerResult} result */
  processFaceLandmarkerResult(result) {
    this.getFaceGaze().processFaceLandmarkerResult_(result);
  }
};
