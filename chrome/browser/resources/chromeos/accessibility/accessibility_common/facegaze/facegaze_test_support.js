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
   * @return {!FaceGaze}
   * @private
   */
  getFaceGaze_() {
    return accessibilityCommon.getFaceGazeForTest();
  }

  /**
   * @return {!MouseController}
   * @private
   */
  getMouseController_() {
    return this.getFaceGaze_().mouseController_;
  }

  /**
   * @return {!GestureHandler}
   * @private
   */
  getGestureHandler_() {
    return this.getFaceGaze_().gestureHandler_;
  }

  /** Cancels the MouseController interval to increase stability in tests. */
  cancelMouseControllerInterval() {
    clearInterval(this.getMouseController_().mouseInterval_);
    this.getMouseController_().mouseInterval_ = -1;
    this.notifyCcTests_();
  }

  /**
   * Sets the repeat delay on GestureHandler. This can be used to allow tests to
   * trigger the same macro multiple times in a row without waiting.
   * @param {number} delay
   */
  setGestureRepeatDelayMs(delay) {
    this.getGestureHandler_().repeatDelayMs_ = delay;
    this.notifyCcTests_();
  }

  /**
   * Gets the WebCamFaceLandmarker object off of the camera stream window.
   * @return {!webCamFaceLandmarker}
   * @private
   */
  async waitForWebCamFaceLandmarker_() {
    await this.getFaceGaze_().cameraStreamReadyPromise_;
    const window = chrome.extension.getViews().find(
        view => view.location.href.includes('camera_stream.html'));
    return window.webCamFaceLandmarker;
  }

  /** Instantiates the FaceLandmarker. */
  async createFaceLandmarker() {
    const webCamFaceLandmarker = await this.waitForWebCamFaceLandmarker_();
    await webCamFaceLandmarker.createFaceLandmarker_();
    if (webCamFaceLandmarker.faceLandmarker_) {
      this.notifyCcTests_();
    }
  }

  /**
   * @param {number} x
   * @param {number} y
   */
  async waitForCursorLocation(x, y) {
    const desktop = await new Promise(resolve => {
      chrome.automation.getDesktop(d => resolve(d));
    });

    const ready = () => {
      const mouseController = this.getMouseController_();
      return x === mouseController.mouseLocation_.x &&
          y === mouseController.mouseLocation_.y;
    };

    if (ready()) {
      // Reset `lastMouseMovedTime_` because otherwise it will prevent FaceGaze
      // from acting for a period of time.
      this.getMouseController_().lastMouseMovedTime_ = 0;
      this.notifyCcTests_();
      return;
    }

    await new Promise(resolve => {
      const onMouseMoved = () => {
        if (ready()) {
          desktop.removeEventListener(
              chrome.automation.EventType.MOUSE_MOVED, onMouseMoved);
          resolve();
        }
      };

      desktop.addEventListener(
          chrome.automation.EventType.MOUSE_MOVED, onMouseMoved);
    });

    // Reset `lastMouseMovedTime_` because otherwise it will prevent FaceGaze
    // from acting for a period of time.
    this.getMouseController_().lastMouseMovedTime_ = 0;
    this.notifyCcTests_();
  }

  /**
   * Forces FaceGaze to process a result, since tests don't have access to real
   * camera data.
   * @param {!{x: number, y: number, z: number}} foreheadLocation
   * @param {!Array<{categoryName: string, score: number}>} recognizedGestures
   */
  async processFaceLandmarkerResult(foreheadLocation, recognizedGestures) {
    const result = {
      faceBlendshapes: [{categories: []}],
      faceLandmarks: [[null, null, null, null, null, null, null, null, null]],
    };
    result.faceBlendshapes[0].categories = recognizedGestures;
    result.faceLandmarks[0][8] = foreheadLocation;
    this.getFaceGaze_().processFaceLandmarkerResult_(result);
    this.notifyCcTests_();
  }

  /**
   * The MouseController updates the mouse location at a set interval. To
   * increase test stability, the interval is canceled in tests, and must be
   * triggered manually using this method.
   */
  triggerMouseControllerInterval() {
    this.getMouseController_().updateMouseLocation_();
    this.notifyCcTests_();
  }
}

globalThis.faceGazeTestSupport = new FaceGazeTestSupport();
