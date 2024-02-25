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

  /**
   * Installs mock accessibility private into the camera stream context, as
   * the default setup only installs it into the background context.
   */
  installMockAccessibilityPrivate() {
    const window = chrome.extension.getViews().find(
        view => view.location.href.includes('camera_stream.html'));
    assertTrue(!!window);

    window.chrome.accessibilityPrivate = this.mockAccessibilityPrivate;
  }

  /** Gets the webCamFaceLandmarker object from the camera stream context. */
  getWebCamFaceLandmarker() {
    const window = chrome.extension.getViews().find(
        view => view.location.href.includes('camera_stream.html'));

    return window ? window.webCamFaceLandmarker : null;
  }

  /** @return {!webCamFaceLandmarker} */
  async waitForWebCamFaceLandmarker() {
    if (this.getWebCamFaceLandmarker()) {
      return this.getWebCamFaceLandmarker();
    }

    return new Promise(resolve => {
      const id = setInterval(() => {
        if (this.getWebCamFaceLandmarker()) {
          clearInterval(id);
          resolve(this.getWebCamFaceLandmarker());
        }
      }, 1000);
    });
  }
};

AX_TEST_F('FaceGazeMediaPipeTest', 'CreateFaceLandmarker', async function() {
  const webCamFaceLandmarker = await this.waitForWebCamFaceLandmarker();
  this.installMockAccessibilityPrivate();
  await this.mockAccessibilityPrivate.initializeFaceGazeAssets();
  await webCamFaceLandmarker.createFaceLandmarker_();
  assertTrue(!!webCamFaceLandmarker.faceLandmarker_);
});
