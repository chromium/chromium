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
    // The mediapipe wasm emits console messages when the FaceLandmarker is
    // initialized. Some of these messages are variable because they include
    // information about time, so we cannot set a static list of allowed
    // messages. As a result, we set `failOnConsoleError` to false.
    super.testGenPreambleCommon(
        /*extensionIdName=*/ 'kAccessibilityCommonExtensionId',
        /*failOnConsoleError=*/ false);
  }
};

// TODO(crbug.com/585323606): Flaky (times out) on MSan. Investigate and
// re-enable.
AX_TEST_F(
    'FaceGazeMediaPipeTest', 'MAYBE_CreateFaceLandmarker', async function() {
      const webCamFaceLandmarker = this.getFaceGaze().webCamFaceLandmarker_;
      await this.mockAccessibilityPrivate.initializeFaceGazeAssets();
      await Messenger.send(
          OffscreenCommandType.FACEGAZE_CREATE_FACE_LANDMARKER_FOR_TEST);
      assertTrue(await Messenger.send(
          OffscreenCommandType.FACEGAZE_HAS_FACE_LANDMARKER_FOR_TEST));
    }, `
#if defined(MEMORY_SANITIZER)
#define MAYBE_CreateFaceLandmarker DISABLED_CreateFaceLandmarker
#else
#define MAYBE_CreateFaceLandmarker CreateFaceLandmarker
#endif
`);
