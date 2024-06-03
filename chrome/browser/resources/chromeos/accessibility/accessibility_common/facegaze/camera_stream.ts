// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {FaceLandmarkerOptions, FaceLandmarkerResult} from '/third_party/mediapipe/vision.js';
import {DrawingUtils, FaceLandmarker} from 'chrome-extension://egfdjlfmgnehecnclamagfafdccgfndp/accessibility_common/third_party/mediapipe_task_vision/vision_bundle.mjs';

import {MouseController} from './mouse_controller.js';

/**
 * Handles interaction with the webcam and FaceLandmarker.
 * TODO(b/309121742): Move the FaceLandmarker logic from this class into the
 * background context once we are able to retrieve camera data from C++.
 */
export class WebCamFaceLandmarker {
  private faceLandmarker_: FaceLandmarker|null = null;
  declare private intervalID_: number|null;
  private drawingUtils_: DrawingUtils|undefined;
  private weights_: Map<string, number> = new Map();
  constructor() {
    this.intervalID_ = null;
  }

  /**
   * Initializes the FaceLandmarker, connects to the webcam, and starts
   * detecting face landmarks.
   */
  async init(): Promise<void> {
    this.addListenersToWeights_();
    await this.createFaceLandmarker_();
    await this.connectToWebCam_();
    this.startDetectingFaceLandmarks_();
  }

  /**
   * This is a placeholder UI that allows devs and dogfooders to adjust what
   * type of facial tracking works best for them.
   */
  private addListenersToWeights_(): void {
    const listener = (): void => {
      let forehead =
          (document.getElementById('foreheadWeight') as HTMLInputElement)
              .valueAsNumber;
      let foreheadTop =
          (document.getElementById('foreheadTopWeight') as HTMLInputElement)
              .valueAsNumber;
      let noseTip =
          (document.getElementById('noseTipWeight') as HTMLInputElement)
              .valueAsNumber;
      let leftTemple =
          (document.getElementById('leftTempleWeight') as HTMLInputElement)
              .valueAsNumber;
      let rightTemple =
          (document.getElementById('rightTempleWeight') as HTMLInputElement)
              .valueAsNumber;
      let rotation =
          (document.getElementById('rotationWeight') as HTMLInputElement)
              .valueAsNumber;
      const sum = forehead + foreheadTop + noseTip + leftTemple + rightTemple +
          rotation;
      forehead /= sum;
      foreheadTop /= sum;
      noseTip /= sum;
      leftTemple /= sum;
      rightTemple /= sum;
      rotation /= sum;
      const weights =
          {forehead, foreheadTop, noseTip, leftTemple, rightTemple, rotation};
      this.weights_ = new Map(Object.entries(weights));
      chrome.runtime.sendMessage(
          undefined, {type: 'updateLandmarkWeights', weights});
    };
    document.getElementById('foreheadWeight')!.addEventListener(
        'input', listener);
    document.getElementById('foreheadTopWeight')!.addEventListener(
        'input', listener);
    document.getElementById('noseTipWeight')!.addEventListener(
        'input', listener);
    document.getElementById('leftTempleWeight')!.addEventListener(
        'input', listener);
    document.getElementById('rightTempleWeight')!.addEventListener(
        'input', listener);
    document.getElementById('rotationWeight')!.addEventListener(
        'input', listener);
    // Execute the listener once.
    listener();
  }

  private async createFaceLandmarker_(): Promise<void> {
    let proceed: Function|undefined;
    chrome.accessibilityPrivate.installFaceGazeAssets(async assets => {
      if (!assets) {
        throw new Error(
            `Couldn't create FaceLandmarker because FaceGaze assets couldn't be
              installed.`);
      }

      // Create a blob to hold the wasm contents.
      const blob = new Blob([assets.wasm]);
      const customFileset = {
        // The wasm loader JS is checked in, so specify the path.
        wasmLoaderPath:
            '../third_party/mediapipe_task_vision/vision_wasm_internal.js',
        // The wasm is stored in a blob, so pass a URL to the blob.
        wasmBinaryPath: URL.createObjectURL(blob),
      };

      // Create the FaceLandmarker and set options.
      this.faceLandmarker_ = await FaceLandmarker.createFromModelBuffer(
          customFileset, new Uint8Array(assets.model));
      const options: FaceLandmarkerOptions = {
        outputFaceBlendshapes: true,
        outputFacialTransformationMatrixes: true,
        runningMode: 'VIDEO',
        numFaces: 1,
      };
      this.faceLandmarker_!.setOptions(options);
      proceed!();
    });

    return new Promise(resolve => {
      proceed = resolve;
    });
  }

  private async connectToWebCam_(): Promise<void> {
    const constraints = {video: true};
    const stream = await navigator.mediaDevices.getUserMedia(constraints);
    const videoElement =
        document.getElementById('cameraStream') as HTMLMediaElement;
    videoElement.srcObject = stream;
  }

  private startDetectingFaceLandmarks_(): void {
    this.intervalID_ = setInterval(
        () => this.detectFaceLandmarks_(),
        WebCamFaceLandmarker.DETECT_FACE_LANDMARKS_INTERVAL_MS);
  }

  private detectFaceLandmarks_(): void {
    if (!this.faceLandmarker_) {
      return;
    }

    const videoElement =
        document.getElementById('cameraStream') as HTMLVideoElement;
    const startTime = performance.now();
    const result = this.faceLandmarker_.detectForVideo(
        /*videoFrame=*/ videoElement, /*timestamp=*/ performance.now());
    const latency = performance.now() - startTime;
    // Send result to the background page for processing.
    chrome.runtime.sendMessage(
        undefined, {type: 'faceLandmarkerResult', result, latency});

    this.displayFaceLandmarkerResult_(result);
  }

  private displayFaceLandmarkerResult_(result: FaceLandmarkerResult): void {
    if (!result || !result.faceLandmarks || !result.faceLandmarks[0]) {
      return;
    }
    if (!result.facialTransformationMatrixes ||
        result.facialTransformationMatrixes.length === 0) {
      return;
    }
    const overlay = document.getElementById('overlay') as HTMLCanvasElement;
    const ctx = overlay.getContext('2d')!;
    if (!this.drawingUtils_) {
      const video = document.getElementById('cameraStream');
      overlay.width = video!.offsetWidth;
      overlay.height = video!.offsetHeight;
      this.drawingUtils_ = new DrawingUtils(ctx);
    }

    ctx.clearRect(0, 0, overlay.width, overlay.height);

    for (const landmarks of result.faceLandmarks) {
      this.drawingUtils_.drawConnectors(
          landmarks, FaceLandmarker.FACE_LANDMARKS_CONTOURS,
          {color: '#F5005760', lineWidth: 2});
    }

    // Draw landmarks.
    let avgX = 0;
    let avgY = 0;
    const landmarks = MouseController.LANDMARK_INDICES;
    for (const landmark of landmarks) {
      let landmarkLocation;
      if (landmark.name === 'rotation') {
        landmarkLocation =
            MouseController.calculateRotationFromFacialTransformationMatrix(
                result.facialTransformationMatrixes[0]);
        ctx.strokeStyle = '#FF9500';
      } else {
        landmarkLocation = result.faceLandmarks[0][landmark.index];
        ctx.strokeStyle = '#FFEA00';
      }
      if (!landmarkLocation) {
        return;
      }
      const x = landmarkLocation.x;
      const y = landmarkLocation.y;
      let weight = this.weights_.get(landmark.name);
      if (!weight) {
        weight = 0;
      }
      avgX += (x * weight);
      avgY += (y * weight);
      ctx.beginPath();
      ctx.arc(x * overlay.width, y * overlay.height, 2, 0, 2 * Math.PI);
      ctx.stroke();
    }

    // Draw averaged position.
    ctx.beginPath();
    ctx.arc(avgX * overlay.width, avgY * overlay.height, 2, 0, 2 * Math.PI);
    ctx.strokeStyle = '#00FF95';
    ctx.stroke();
  }
}
export namespace WebCamFaceLandmarker {
  /**
   * The interval, in milliseconds, for which we request results from the
   * FaceLandmarker API. This should be frequent enough to give a real-time
   * feeling.
   */
  export const DETECT_FACE_LANDMARKS_INTERVAL_MS = 60;
}

declare global {
  var webCamFaceLandmarker: WebCamFaceLandmarker;
}

document.addEventListener('DOMContentLoaded', () => {
  globalThis.webCamFaceLandmarker = new WebCamFaceLandmarker();
  const button = document.getElementById('startButton');
  button?.addEventListener(
      'click', () => globalThis.webCamFaceLandmarker.init());
  button?.removeAttribute('hidden');
  chrome.runtime.sendMessage(undefined, {type: 'cameraStreamReadyForTesting'});
});
