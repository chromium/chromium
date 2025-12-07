// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestImportManager} from '/common/testing/test_import_manager.js';
import type {FaceLandmarkerResult} from '/third_party/mediapipe/vision.js';

import {Messenger} from '../messenger.js';
import {OffscreenCommandType} from '../offscreen_command_type.js';

import {BubbleController} from './bubble_controller.js';
import {PrefNames} from './constants.js';

export interface FaceLandmarkerResultWithLatency {
  result: FaceLandmarkerResult;
  latency: number;
}

/**
 * The interval, in milliseconds, for which we request results from the
 * FaceLandmarker API. This should be frequent enough to give a real-time
 * feeling.
 */
const DETECT_FACE_LANDMARKS_INTERVAL_MS = 60;

/** Handles interaction with the webcam and FaceLandmarker. */
export class WebCamFaceLandmarker {
  private bubbleController_: BubbleController;

  // Callbacks.
  private onFaceLandmarkerResult_:
      (resultWithLatency: FaceLandmarkerResultWithLatency) => void;
  private onTrackMuted_: VoidFunction;
  private onTrackUnmuted_: VoidFunction;

  // State-related members.
  declare private intervalID_: number|null;

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
    this.intervalID_ = null;

    this.readyForTesting_ = new Promise(resolve => {
      this.setReadyForTesting_ = resolve;
    });

    Messenger.registerHandler(
        OffscreenCommandType.FACEGAZE_SW_UPDATE_BUBBLE_REMAINING_RETRIES,
        (message: {remaining: number}) => {
          const text = chrome.i18n.getMessage(
              'facegaze_connect_to_camera', [message.remaining]);
          this.bubbleController_.updateBubble(text);
        });
    Messenger.registerHandler(
        OffscreenCommandType.FACEGAZE_SW_ON_TRACK_MUTED, () => {
          this.onTrackMuted_();
        });
    Messenger.registerHandler(
        OffscreenCommandType.FACEGAZE_SW_ON_TRACK_UNMUTED, () => {
          this.onTrackUnmuted_();
        });
    Messenger.registerHandler(
        OffscreenCommandType.FACEGAZE_SW_INSTALL_ASSETS, () => {
          return this.installAssets_();
        });
    Messenger.registerHandler(
        OffscreenCommandType.FACEGAZE_SW_SET_PREF,
        (message: {name: string, value: Object}) => {
          chrome.settingsPrivate.setPref(message.name, message.value);
        });
  }

  /**
   * Initializes the FaceLandmarker, connects to the webcam, and starts
   * detecting face landmarks.
   */
  async init(): Promise<void> {
    await this.initWebCam_();
    this.startDetectingFaceLandmarks_();
  }

  private async initWebCam_(): Promise<void> {
    await Messenger.send(OffscreenCommandType.FACEGAZE_WEBCAM_INITIALIZE);
    if (chrome.runtime.lastError) {
      return Promise.reject(new Error(chrome.runtime.lastError.message));
    }
    if (this.setReadyForTesting_) {
      this.setReadyForTesting_();
    }
  }

  private startDetectingFaceLandmarks_(): void {
    this.intervalID_ = setInterval(
        () => this.detectFaceLandmarks_(), DETECT_FACE_LANDMARKS_INTERVAL_MS);
  }

  private async detectFaceLandmarks_(): Promise<void> {
    const startTime = performance.now();
    const response = await Messenger.send(
        OffscreenCommandType.FACEGAZE_WEBCAM_DETECT_LANDMARK);
    if (chrome.runtime.lastError) {
      return Promise.reject(new Error(chrome.runtime.lastError.message));
    }
    if (!response) {
      return;
    }

    const latency = performance.now() - startTime;
    // Use a callback to send the result to the main FaceGaze object.
    this.onFaceLandmarkerResult_({result: response, latency});
  }

  private async installAssets_(): Promise<any> {
    const assets = await chrome.accessibilityPrivate.installFaceGazeAssets();
    if (!assets) {
      // FaceGaze will not work unless the FaceGaze assets are successfully
      // installed. When the assets fail to install, AccessibilityManager
      // shows a notification to the user informing them of the failure and
      // to try installing again later. As a result, we should turn FaceGaze
      // off here and allow them to toggle the feature back on to retry the
      // download.
      console.error(
          `Couldn't create FaceLandmarker because FaceGaze assets couldn't
             be installed.`);

      chrome.settingsPrivate.setPref(PrefNames.FACE_GAZE_ENABLED, false);
      return;
    }
    return {
      wasm: await Messenger.arrayBufferToBase64(assets.wasm),
      model: await Messenger.arrayBufferToBase64(assets.model)
    };
  }

  stop(): void {
    Messenger.send(OffscreenCommandType.FACEGAZE_WEBCAM_STOP);
    if (this.intervalID_ !== null) {
      clearInterval(this.intervalID_);
      this.intervalID_ = null;
    }
  }

  async stopWebCamForTesting(): Promise<void> {
    await Messenger.send(OffscreenCommandType.FACEGAZE_WEBCAM_STOP_FOR_TEST);
    if (this.intervalID_ !== null) {
      clearInterval(this.intervalID_);
      this.intervalID_ = null;
    }
  }
}

TestImportManager.exportForTesting(WebCamFaceLandmarker);
