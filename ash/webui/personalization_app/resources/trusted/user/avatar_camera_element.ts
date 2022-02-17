// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview The avatar-camera component displays a camera interface to
 * allow the user to take a selfie.
 */

import 'chrome://resources/cr_elements/cr_dialog/cr_dialog.m.js';

import * as webcamUtils from 'chrome://resources/cr_elements/chromeos/cr_picture/webcam_utils.js';
import {CrDialogElement} from 'chrome://resources/cr_elements/cr_dialog/cr_dialog.m.js';
import {html} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {WithPersonalizationStore} from '../personalization_store.js';

import {saveCameraImage} from './user_controller.js';
import {getUserProvider} from './user_interface_provider.js';

export interface AvatarCamera {
  $: {dialog: CrDialogElement; webcamVideo: HTMLVideoElement;};
}
export class AvatarCamera extends WithPersonalizationStore {
  static get is() {
    return 'avatar-camera';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      /** Keep track of the open handle to the webcam. */
      cameraStream_: {
        type: Object,
        value: null,
      }
    };
  }

  // Static to mock out easier in tests.
  static webcamUtils = webcamUtils;

  // Static to mock out easier in tests.
  static getUserMedia(): Promise<MediaStream> {
    return navigator.mediaDevices.getUserMedia({
      audio: false,
      video: AvatarCamera.webcamUtils.kDefaultVideoConstraints,
    });
  }

  private cameraStream_: MediaStream|null;

  connectedCallback() {
    super.connectedCallback();
    this.startCamera_();
  }

  disconnectedCallback() {
    super.disconnectedCallback();
    this.stopCamera_();
  }

  private async startCamera_() {
    this.stopCamera_();
    try {
      this.cameraStream_ = await AvatarCamera.getUserMedia();
      if (!this.isConnected) {
        // User closed the camera UI while waiting for the camera to start.
        this.stopCamera_();
        return;
      }
      const video = this.$.webcamVideo;
      // Display the webcam feed to the user by binding it to |video|.
      video.srcObject = this.cameraStream_;
    } catch (e) {
      console.error('Unable to start camera', e);
      this.stopCamera_();
    }
  }

  /**
   * If the camera is active, stop all the active media. Safe to call even if
   * the camera is off.
   */
  private stopCamera_() {
    AvatarCamera.webcamUtils.stopMediaTracks(this.cameraStream_);
    this.cameraStream_ = null;
  }

  private async takePhoto_() {
    const frames = await AvatarCamera.webcamUtils.captureFrames(
        this.$.webcamVideo, AvatarCamera.webcamUtils.CAPTURE_SIZE,
        AvatarCamera.webcamUtils.CAPTURE_INTERVAL_MS,
        /*num_frames=*/ 1);

    const pngBinary = AvatarCamera.webcamUtils.convertFramesToPngBinary(frames);

    saveCameraImage(pngBinary, getUserProvider());

    // Close the dialog after saving the image.
    this.$.dialog.close();
  }
}

customElements.define(AvatarCamera.is, AvatarCamera);
