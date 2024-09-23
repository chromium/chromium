// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

GEN_INCLUDE(['facegaze_test_base.js']);

/** FazeGaze MediaPipe tests. */
FaceGazeMediaPipeTest = class extends FaceGazeTestBase {
  /** @override */
  async setUpDeferred() {
    this.overrideIntervalFunctions_ = false;
    await super.setUpDeferred();
  }

  /** @override */
  testGenPreamble() {
    super.testGenPreamble();
    // TODO(b/309121742): change `failOnConsoleError` to true and specify
    // allowed messages from mediapipe wasm.
    super.testGenPreambleCommon(
        /*extensionIdName=*/ 'kAccessibilityCommonExtensionId',
        /*failOnConsoleError=*/ false);
  }
};

AX_TEST_F('FaceGazeMediaPipeTest', 'CreateFaceLandmarker', async function() {
  const webCamFaceLandmarker = this.getFaceGaze().webCamFaceLandmarker_;
  await this.mockAccessibilityPrivate.initializeFaceGazeAssets();
  await webCamFaceLandmarker.createFaceLandmarker_();
  assertTrue(!!webCamFaceLandmarker.faceLandmarker_);
});
