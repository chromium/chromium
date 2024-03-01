// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** A class that provides test support for C++ tests. */
class FaceGazeTestSupport {
  constructor() {
    this.notifyCcTests_();
  }

  /**
   * Notifies C++ tests, which wait for the JS side to call
   * `chrome.test.sendScriptResult`, that they can continue.
   * @private
   */
  notifyCcTests_() {
    chrome.test.sendScriptResult('ready');
  }

  /**
   * Gets the WebCamFaceLandmarker object off of the camera stream window.
   * @return {!webCamFaceLandmarker}
   */
  async waitForWebCamFaceLandmarker() {
    await accessibilityCommon.getFaceGazeForTest().cameraStreamReadyPromise_;
    const window = chrome.extension.getViews().find(
        view => view.location.href.includes('camera_stream.html'));
    return window.webCamFaceLandmarker;
  }

  /** Instantiates the FaceLandmarker. */
  async createFaceLandmarker() {
    const webCamFaceLandmarker = await this.waitForWebCamFaceLandmarker();
    await webCamFaceLandmarker.createFaceLandmarker_();
    if (webCamFaceLandmarker.faceLandmarker_) {
      this.notifyCcTests_();
    }
  }
}

globalThis.faceGazeTestSupport = new FaceGazeTestSupport();
