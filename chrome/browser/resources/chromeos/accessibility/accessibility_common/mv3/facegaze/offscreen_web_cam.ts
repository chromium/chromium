// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {FaceLandmarkerOptions, FaceLandmarkerResult} from '/third_party/mediapipe/vision.js';
import {FaceLandmarker} from 'chrome-extension://egfdjlfmgnehecnclamagfafdccgfndp/accessibility_common/mv3/third_party/mediapipe_task_vision/vision_bundle.mjs';

import {Messenger} from '../messenger.js';
import {OffscreenCommandType} from '../offscreen_command_type.js';

import {PrefNames} from './constants.js';

const CONNECT_TO_WEBCAM_TIMEOUT = 1000;

/**
 * The default number of times we should try to connect to the webcam. If we
 * cannot establish a connection after trying this many times, then we should
 * notify the user and turn off FaceGaze.
 */
const DEFAULT_CONNECT_TO_WEBCAM_RETRIES = 10;

/**
 * The dimensions used for the camera stream. 192 x 192 are the dimensions
 * used by the FaceLandmarker, so frames that are larger than this must go
 * through a downsampling process, which takes extra work.
 */
const VIDEO_FRAME_DIMENSIONS = 192;

/** The wasm loader JS is checked in under this path. */
const WASM_LOADER_PATH =
    'accessibility_common/mv3/third_party/mediapipe_task_vision/' +
    'vision_wasm_internal.js';

/** A helper class to support test. */
class TestSupport {
  private owner_: OffscreenWebCam;
  private timeoutCallbacks_: {[key: number]: VoidFunction} = {};
  private nextTimeoutId_ = 1;

  constructor(owner: OffscreenWebCam) {
    this.owner_ = owner;

    Messenger.registerHandler(
        OffscreenCommandType.FACEGAZE_CREATE_FACE_LANDMARKER_FOR_TEST,
        () => this.createFaceLandmarker());
    Messenger.registerHandler(
        OffscreenCommandType.FACEGAZE_MOCK_NO_CAMERA_FOR_TEST,
        () => this.mockNoCamera_());
    Messenger.registerHandler(
        OffscreenCommandType.FACEGAZE_MOCK_TIMEOUT_FOR_TEST,
        () => this.mockTimeout());
    Messenger.registerHandler(
        OffscreenCommandType.FACEGAZE_MOCK_RUN_LATEST_TIMEOUT_FOR_TEST,
        () => this.runLatestTimeout());
    Messenger.registerHandler(
        OffscreenCommandType.FACEGAZE_CONNECT_TO_WEB_CAM_FOR_TEST,
        () => this.connectToWebCam());
    Messenger.registerHandler(
        OffscreenCommandType.FACEGAZE_GET_CAMERA_RETRIES_FOR_TEST,
        () => Promise.resolve(this.getWebCamRetriesRemaining()));
    Messenger.registerHandler(
        OffscreenCommandType.FACEGAZE_SET_CAMERA_RETRIES_FOR_TEST,
        (message: {retries: number}) =>
            this.setWebCamRetriesRemaining(message));
    Messenger.registerHandler(
        OffscreenCommandType.FACEGAZE_HAS_FACE_LANDMARKER_FOR_TEST,
        () => Promise.resolve(this.hasFaceLandmarker()));
    Messenger.registerHandler(
        OffscreenCommandType.FACEGAZE_WEBCAM_STOP_FOR_TEST,
        () => this.stopForTest());
  }

  mockNoCamera_(): void {
    // Pretend that there is no available camera.
    Object.defineProperty(navigator, 'mediaDevices', {
      value: {
        getUserMedia: () => {
          return Promise.reject(new Error('Requested device not found'));
        },
      },
      writable: true,
    });
  }

  mockTimeout(): void {
    globalThis.setTimeout =
        (handler: TimerHandler, _timeout?: number, ...args: any[]): number => {
          const id = this.nextTimeoutId_;
          ++this.nextTimeoutId_;
          this.timeoutCallbacks_[id] = () => (handler as Function)(...args);
          return id;
        };
    globalThis.clearTimeout = (id: number|undefined): void => {
      if (id !== undefined) {
        delete this.timeoutCallbacks_[id];
      }
    };
  }

  runLatestTimeout(): void {
    const latestId = this.nextTimeoutId_ - 1;
    if (this.timeoutCallbacks_[latestId]) {
      this.timeoutCallbacks_[latestId]();
      delete this.timeoutCallbacks_[latestId];
    }
  }

  async connectToWebCam(): Promise<void> {
    // @ts-ignore Private member access.
    await this.owner_.connectToWebCam_();
  }

  getWebCamRetriesRemaining(): number {
    // @ts-ignore Private member access.
    return this.owner_.connectToWebCamRetriesRemaining_;
  }

  setWebCamRetriesRemaining(message: {retries: number}): void {
    // @ts-ignore Private member access.
    this.owner_.connectToWebCamRetriesRemaining_ = message.retries;
  }

  async createFaceLandmarker(): Promise<void> {
    // @ts-ignore Private member access.
    return this.owner_.createFaceLandmarker_();
  }

  hasFaceLandmarker(): boolean {
    // @ts-ignore Private member access.
    return !!this.owner_.faceLandmarker_;
  }

  stopForTest(): void {
    // @ts-ignore Private member access.
    return this.owner_.stopImageCaptureTrack_();
  }
}

/**
 * Offscreen way to manage the web cam and detect landmarkers.
 */
class OffscreenWebCam {
  // Face landmarker.
  private faceLandmarker_: FaceLandmarker|null = null;

  // State-related members.
  private stopped_ = true;

  // Members to track the connection to the webcam.
  private connectToWebCamRetriesRemaining_ = DEFAULT_CONNECT_TO_WEBCAM_RETRIES;
  declare private webCamConnected_: Promise<void>;
  private setWebCamConnected_?: () => void;

  // Core objects that power face landmark recognition.
  private imageCapture_: ImageCapture|undefined;

  // Event handlers that route to either private member functions or callbacks.
  private onTrackEndedHandler_: VoidFunction;
  private onTrackMutedHandler_: VoidFunction;
  private onTrackUnmutedHandler_: VoidFunction;

  declare private testSupport_: TestSupport;

  constructor() {
    // Create handlers that run the above callbacks.
    this.onTrackEndedHandler_ = () => this.onTrackEnded_();
    this.onTrackMutedHandler_ = () =>
        Messenger.send(OffscreenCommandType.FACEGAZE_SW_ON_TRACK_MUTED);
    this.onTrackUnmutedHandler_ = () =>
        Messenger.send(OffscreenCommandType.FACEGAZE_SW_ON_TRACK_UNMUTED);

    Messenger.registerHandler(
        OffscreenCommandType.FACEGAZE_WEBCAM_DETECT_LANDMARK,
        () => this.detectFaceLandmarks_());
    Messenger.registerHandler(
        OffscreenCommandType.FACEGAZE_WEBCAM_INITIALIZE,
        () => this.initialize_());
    Messenger.registerHandler(
        OffscreenCommandType.FACEGAZE_WEBCAM_STOP, () => this.stop_());

    this.testSupport_ = new TestSupport(this);
  }

  private async initialize_(): Promise<void> {
    // Ensure nothing is running in case this is not the first initialize call
    // since offscreen document could outlive the service worker.
    this.stop_();

    // Start a new session.
    this.stopped_ = false;
    this.webCamConnected_ = new Promise(resolve => {
      this.setWebCamConnected_ = resolve;
    });

    await this.createFaceLandmarker_();
    await this.connectToWebCam_();
    return this.webCamConnected_;
  }

  private async createFaceLandmarker_(): Promise<void> {
    const reply =
        await Messenger.send(OffscreenCommandType.FACEGAZE_SW_INSTALL_ASSETS);
    const assets = {
      wasm: await Messenger.base64ToArrayBuffer(reply.wasm),
      model: await Messenger.base64ToArrayBuffer(reply.model)
    };

    // Create a blob to hold the wasm contents.
    const blob = new Blob([assets.wasm], {type: 'application/wasm'});
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
        Messenger.send(
            OffscreenCommandType.FACEGAZE_SW_UPDATE_BUBBLE_REMAINING_RETRIES,
            {'remaining': this.connectToWebCamRetriesRemaining_});

        this.connectToWebCamRetriesRemaining_ -= 1;
        setTimeout(() => this.connectToWebCam_(), CONNECT_TO_WEBCAM_TIMEOUT);
      } else {
        Messenger.send(
            OffscreenCommandType.FACEGAZE_SW_SET_PREF,
            {'name': PrefNames.FACE_GAZE_ENABLED, 'value': false});
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

  private async detectFaceLandmarks_(): Promise<FaceLandmarkerResult|null> {
    if (!this.faceLandmarker_ || !this.imageCapture_) {
      return null;
    }

    let frame;
    try {
      frame = await this.imageCapture_!.grabFrame();
    } catch (error) {
      // grabFrame() can occasionally return an error, so in these cases, we
      // should handle the error and simply return instead of trying to process
      // the frame.
      return null;
    }

    return this.faceLandmarker_.detect(/*image=*/ frame);
  }

  private stop_(): void {
    this.stopped_ = true;
    this.stopImageCaptureTrack_();
    this.faceLandmarker_ = null;
  }

  private onTrackEnded_(): void {
    this.stopImageCaptureTrack_();
    this.connectToWebCam_();
  }

  private stopImageCaptureTrack_(): void {
    // Disconnect from the webcam by resetting `imageCapture_`.
    if (this.imageCapture_) {
      this.removeEventListeners_();
      this.imageCapture_.track.stop();
      this.imageCapture_ = undefined;
    }
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
}

export {OffscreenWebCam};
