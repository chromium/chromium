// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {FaceLandmarker} from '/accessibility_common/mv2/third_party/mediapipe_task_vision/vision_bundle.mjs';
import {TestImportManager} from '/common/testing/test_import_manager.js';
import type {FaceLandmarkerOptions, FaceLandmarkerResult} from '/third_party/mediapipe/vision.js';

import {BubbleController} from './bubble_controller.js';
import {PrefNames} from './constants.js';

export interface FaceLandmarkerResultWithLatency {
  result: FaceLandmarkerResult;
  latency: number;
}

const CONNECT_TO_WEBCAM_TIMEOUT = 1000;

/**
 * The default number of times we should try to connect to the webcam. If we
 * cannot establish a connection after trying this many times, then we should
 * notify the user and turn off FaceGaze.
 */
const DEFAULT_CONNECT_TO_WEBCAM_RETRIES = 10;

/**
 * The interval, in milliseconds, for which we request results from the
 * FaceLandmarker API. This should be frequent enough to give a real-time
 * feeling.
 */
const DETECT_FACE_LANDMARKS_INTERVAL_MS = 60;

/**
 * The dimensions used for the camera stream. 192 x 192 are the dimensions
 * used by the FaceLandmarker, so frames that are larger than this must go
 * through a downsampling process, which takes extra work.
 */
const VIDEO_FRAME_DIMENSIONS = 192;

/** The wasm loader JS is checked in under this path. */
const WASM_LOADER_PATH =
    'accessibility_common/mv2/third_party/mediapipe_task_vision/' +
    'vision_wasm_internal.js';

/** Handles interaction with the webcam and FaceLandmarker. */
export class WebCamFaceLandmarker {
  // Core objects that power face landmark recognition.
  private faceLandmarker_: FaceLandmarker|null = null;
  private imageCapture_: ImageCapture|undefined;

  private bubbleController_: BubbleController;

  // Callbacks.
  private onFaceLandmarkerResult_:
      (resultWithLatency: FaceLandmarkerResultWithLatency) => void;
  private onTrackMuted_: VoidFunction;
  private onTrackUnmuted_: VoidFunction;

  // Event handlers that route to either private member functions or callbacks.
  private onTrackEndedHandler_: VoidFunction;
  private onTrackMutedHandler_: VoidFunction;
  private onTrackUnmutedHandler_: VoidFunction;

  // State-related members.
  private stopped_ = true;
  declare private intervalID_: number|null;

  // Members to track the connection to the webcam.
  private connectToWebCamRetriesRemaining_ = DEFAULT_CONNECT_TO_WEBCAM_RETRIES;
  declare private webCamConnected_: Promise<void>;
  private setWebCamConnected_?: () => void;

  // Testing-related members.
  declare private readyForTesting_: Promise<void>;
  private setReadyForTesting_?: VoidFunction;

  constructor(
      bubbleController: BubbleController,
      onFaceLandmarkerResult:
          (resultWithLatency: FaceLandmarkerResultWithLatency) => void,
      onTrackMuted: VoidFunction, onTrackUnmuted: VoidFunction) {
    this.bubbleController_ = bubbleController;
    // Save callbacks.
    this.onFaceLandmarkerResult_ = onFaceLandmarkerResult;
    this.onTrackMuted_ = onTrackMuted;
    this.onTrackUnmuted_ = onTrackUnmuted;

    // Create handlers that run the above callbacks.
    this.onTrackEndedHandler_ = () => this.onTrackEnded_();
    this.onTrackMutedHandler_ = () => {
      this.onTrackMuted_();
    };
    this.onTrackUnmutedHandler_ = () => this.onTrackUnmuted_();
    this.intervalID_ = null;

    this.webCamConnected_ = new Promise(resolve => {
      this.setWebCamConnected_ = resolve;
    });

    this.readyForTesting_ = new Promise(resolve => {
      this.setReadyForTesting_ = resolve;
    });
  }

  /**
   * Initializes the FaceLandmarker, connects to the webcam, and starts
   * detecting face landmarks.
   */
  async init(): Promise<void> {
    this.stopped_ = false;
    await this.createFaceLandmarker_();
    this.connectToWebCam_();
    await this.webCamConnected_!;
    this.startDetectingFaceLandmarks_();
  }

  private async createFaceLandmarker_(): Promise<void> {
    let proceed: Function|undefined;
    chrome.accessibilityPrivate.installFaceGazeAssets(async assets => {
      if (!assets) {
        // FaceGaze will not work unless the FaceGaze assets are successfully
        // installed. When the assets fail to install, AccessibilityManager
        // shows a notification to the user informing them of the failure and to
        // try installing again later. As a result, we should turn FaceGaze off
        // here and allow them to toggle the feature back on to retry the
        // download.
        console.error(
            `Couldn't create FaceLandmarker because FaceGaze assets couldn't be
              installed.`);

        chrome.settingsPrivate.setPref(PrefNames.FACE_GAZE_ENABLED, false);
        return;
      }

      // Create a blob to hold the wasm contents.
      const blob = new Blob([assets.wasm]);
      const customFileset = {
        // The wasm loader JS is checked in, so specify the path.
        wasmLoaderPath: chrome.runtime.getURL(WASM_LOADER_PATH),
        // The wasm is stored in a blob, so pass a URL to the blob.
        wasmBinaryPath: URL.createObjectURL(blob),
      };

      // Create the FaceLandmarker and set options.
      this.faceLandmarker_ = await FaceLandmarker.createFromModelBuffer(
          customFileset, new Uint8Array(assets.model));
      const options: FaceLandmarkerOptions = {
        outputFaceBlendshapes: true,
        outputFacialTransformationMatrixes: true,
        runningMode: 'IMAGE',
        numFaces: 1,
      };
      this.faceLandmarker_!.setOptions(options);
      if (this.setReadyForTesting_) {
        this.setReadyForTesting_();
      }
      proceed!();
    });

    return new Promise(resolve => {
      proceed = resolve;
    });
  }

  private async connectToWebCam_(): Promise<void> {
    const constraints = {
      video: {
        height: VIDEO_FRAME_DIMENSIONS,
        width: VIDEO_FRAME_DIMENSIONS,
        facingMode: 'user',
      },
    };

    let stream;
    try {
      stream = await navigator.mediaDevices.getUserMedia(constraints);
    } catch (error) {
      if (this.connectToWebCamRetriesRemaining_ > 0) {
        const message = chrome.i18n.getMessage(
            'facegaze_connect_to_camera',
            [this.connectToWebCamRetriesRemaining_]);
        this.bubbleController_.updateBubble(message);
        this.connectToWebCamRetriesRemaining_ -= 1;
        setTimeout(() => this.connectToWebCam_(), CONNECT_TO_WEBCAM_TIMEOUT);
      } else {
        chrome.settingsPrivate.setPref(PrefNames.FACE_GAZE_ENABLED, false);
      }

      return;
    }

    const tracks = stream.getVideoTracks();
    // It is possible for FaceGaze to be turned off before getUserMedia()
    // completes. If FaceGaze has stopped when we finish this promise, then
    // clean up the webcam resources so the webcam does not stay on.
    if (this.stopped_) {
      tracks[0].stop();
      return;
    }

    this.imageCapture_ = new ImageCapture(tracks[0]);
    this.imageCapture_.track.addEventListener(
        'ended', this.onTrackEndedHandler_);
    this.imageCapture_.track.addEventListener(
        'mute', this.onTrackMutedHandler_);
    this.imageCapture_.track.addEventListener(
        'unmute', this.onTrackUnmutedHandler_);

    // Once we make it here, we know that the webcam is connected.
    this.connectToWebCamRetriesRemaining_ = DEFAULT_CONNECT_TO_WEBCAM_RETRIES;
    this.setWebCamConnected_!();
  }

  private onTrackEnded_(): void {
    if (this.imageCapture_) {
      // Tell MediaStreamTrack that we are no longer using this ended track.
      this.imageCapture_.track.stop();
      this.removeEventListeners_();
    }
    this.imageCapture_ = undefined;
    this.connectToWebCam_();
  }

  private startDetectingFaceLandmarks_(): void {
    this.intervalID_ = setInterval(
        () => this.detectFaceLandmarks_(), DETECT_FACE_LANDMARKS_INTERVAL_MS);
  }

  private async detectFaceLandmarks_(): Promise<void> {
    if (!this.faceLandmarker_) {
      return;
    }

    let frame;
    try {
      frame = await this.imageCapture_!.grabFrame();
    } catch (error) {
      // grabFrame() can occasionally return an error, so in these cases, we
      // should handle the error and simply return instead of trying to process
      // the frame.
      return;
    }

    const startTime = performance.now();
    const result = this.faceLandmarker_.detect(/*image=*/ frame);
    const latency = performance.now() - startTime;
    // Use a callback to send the result to the main FaceGaze object.
    this.onFaceLandmarkerResult_({result, latency});
  }

  private removeEventListeners_(): void {
    if (this.imageCapture_) {
      this.imageCapture_.track.removeEventListener(
          'ended', this.onTrackEndedHandler_);
      this.imageCapture_.track.removeEventListener(
          'mute', this.onTrackMutedHandler_);
      this.imageCapture_.track.removeEventListener(
          'unmute', this.onTrackUnmutedHandler_);
    }
  }

  stop(): void {
    this.stopped_ = true;
    if (this.imageCapture_) {
      this.removeEventListeners_();
      this.imageCapture_.track.stop();
      this.imageCapture_ = undefined;
    }
    this.faceLandmarker_ = null;
    if (this.intervalID_ !== null) {
      clearInterval(this.intervalID_);
      this.intervalID_ = null;
    }
  }
}

TestImportManager.exportForTesting(WebCamFaceLandmarker);
