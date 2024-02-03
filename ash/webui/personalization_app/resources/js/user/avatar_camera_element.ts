// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview The avatar-camera component displays a camera interface to
 * allow the user to take a selfie.
 */

import 'chrome://resources/ash/common/personalization/cros_button_style.css.js';
import 'chrome://resources/ash/common/personalization/personalization_shared_icons.html.js';
import 'chrome://resources/ash/common/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/ash/common/cr_elements/cr_dialog/cr_dialog.js';
import 'chrome://resources/polymer/v3_0/paper-spinner/paper-spinner-lite.js';

import {CrDialogElement} from 'chrome://resources/ash/common/cr_elements/cr_dialog/cr_dialog.js';
import {assertInstanceof, assertNotReached} from 'chrome://resources/js/assert.js';
import {afterNextRender} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {WithPersonalizationStore} from '../personalization_store.js';

import {getTemplate} from './avatar_camera_element.html.js';
import {saveCameraImage} from './user_controller.js';
import {getUserProvider} from './user_interface_provider.js';
import {GetUserMediaProxy, getWebcamUtils} from './webcam_utils_proxy.js';


export const enum AvatarCameraMode {
  CAMERA = 'camera',
  VIDEO = 'video',
}

function getNumFrames(mode: AvatarCameraMode): number {
  switch (mode) {
    case AvatarCameraMode.CAMERA:
      return 1;
    case AvatarCameraMode.VIDEO:
      const webcamUtils = getWebcamUtils();
      return webcamUtils.CAPTURE_DURATION_MS / webcamUtils.CAPTURE_INTERVAL_MS;
    default:
      assertNotReached(`Called with impossible AvatarCameraMode: ${mode}`);
  }
}

function getCaptureSize(mode: AvatarCameraMode):
    {height: number, width: number} {
  const webcamUtils = getWebcamUtils();
  switch (mode) {
    case AvatarCameraMode.CAMERA:
      return webcamUtils.CAPTURE_SIZE;
    case AvatarCameraMode.VIDEO:
      return {
        height: webcamUtils.CAPTURE_SIZE.height / 2,
        width: webcamUtils.CAPTURE_SIZE.width / 2,
      };
    default:
      assertNotReached(`Called with impossible AvatarCameraMode: ${mode}`);
  }
}

export interface AvatarCameraElement {
  $: {dialog: CrDialogElement, webcamVideo: HTMLVideoElement};
}

export class AvatarCameraElement extends WithPersonalizationStore {
  static get is() {
    return 'avatar-camera';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      /**
       * Set mode property to switch between camera and video.
       */
      mode: {
        type: String,
        value: AvatarCameraMode.CAMERA,
      },

      /** Keep track of the open handle to the webcam. */
      cameraStream_: {
        type: Object,
        value: null,
      },

      /**
       * Store a reference to the captured png data to know if the user has
       * captured an image yet.
       */
      pngBinary_: {
        type: Object,
        value: null,
      },

      /** Show the image as a blob to avoid URL length limits. */
      previewBlobUrl_: {
        type: String,
        computed: 'computePreviewBlobUrl_(pngBinary_)',
        observer: 'onPreviewBlobUrlChanged_',
      },

      captureInProgress_: {
        type: Boolean,
        value: false,
      },
    };
  }

  mode: AvatarCameraMode;
  private cameraStream_: MediaStream|null;
  private pngBinary_: Uint8Array|null;
  private previewBlobUrl_: string|null;
  private captureInProgress_: boolean;

  override connectedCallback() {
    super.connectedCallback();
    this.startCamera_();
  }

  override disconnectedCallback() {
    super.disconnectedCallback();
    this.stopCamera_();
  }

  private computePreviewBlobUrl_(): string|null {
    if (!this.pngBinary_) {
      return null;
    }

    assertInstanceof(
        this.pngBinary_, Uint8Array,
        'Preview binary should be a png uint8array');

    const blob = new Blob([this.pngBinary_], {type: 'image/png'});
    return URL.createObjectURL(blob);
  }

  private onPreviewBlobUrlChanged_(_: string|null, old: string|null) {
    if (old) {
      // Revoke the last one to free memory.
      URL.revokeObjectURL(old);
    }
  }

  private async startCamera_() {
    this.stopCamera_();

    try {
      this.cameraStream_ = await GetUserMediaProxy.getInstance().getUserMedia();
      if (!this.isConnected) {
        // User closed the camera UI while waiting for the camera to start.
        this.stopCamera_();
        return;
      }
      const video = this.$.webcamVideo;
      // Display the webcam feed to the user by binding it to |video|.
      video.srcObject = this.cameraStream_;
      await new Promise((resolve) => afterNextRender(this, resolve));
      this.shadowRoot!.getElementById('takePhoto')!.focus();
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
    getWebcamUtils().stopMediaTracks(this.cameraStream_);
    this.cameraStream_ = null;
    this.pngBinary_ = null;
  }

  private async takePhoto_() {
    const webcamUtils = getWebcamUtils();

    try {
      this.captureInProgress_ = true;
      // Let the animation start smoothly before beginning the capture.
      await new Promise(resolve => requestAnimationFrame(resolve));
      const frames = await webcamUtils.captureFrames(
          this.$.webcamVideo, getCaptureSize(this.mode),
          webcamUtils.CAPTURE_INTERVAL_MS, getNumFrames(this.mode));

      this.pngBinary_ = webcamUtils.convertFramesToPngBinary(frames);
      await new Promise(resolve => afterNextRender(this, resolve));
      this.shadowRoot!.getElementById('clearPhoto')!.focus();
    } catch (e) {
      console.error('Failed to capture from webcam', e);
    } finally {
      this.captureInProgress_ = false;
    }
  }

  private confirmPhoto_() {
    assertInstanceof(
        this.pngBinary_, Uint8Array,
        'Preview image binary must be set to confirm photo');

    saveCameraImage(this.pngBinary_, getUserProvider());
    this.pngBinary_ = null;
    // Close the camera interface when an image is confirmed.
    this.$.dialog.close();
  }

  private clearPhoto_() {
    this.pngBinary_ = null;
  }

  private showLoading_(): boolean {
    return !this.cameraStream_ && !this.previewBlobUrl_;
  }

  private showSvgMask_(): boolean {
    return this.showCameraFeed_() || !!this.previewBlobUrl_;
  }

  private showCameraFeed_(): boolean {
    return !!this.cameraStream_ && !this.previewBlobUrl_;
  }

  private showTakePhotoButton_(): boolean {
    return this.showCameraFeed_() && !this.captureInProgress_;
  }

  private showLoadingSpinnerButton_(): boolean {
    return this.mode === AvatarCameraMode.VIDEO && this.showCameraFeed_() &&
        this.captureInProgress_;
  }

  private showFooter_(): boolean {
    return this.showCameraFeed_() || !!this.previewBlobUrl_;
  }

  private getTakePhotoIcon_(mode: AvatarCameraMode): string {
    return mode === AvatarCameraMode.VIDEO ? 'personalization:loop' :
                                             'personalization:camera_compact';
  }

  private getTakePhotoText_(mode: AvatarCameraMode): string {
    return mode === AvatarCameraMode.VIDEO ? this.i18n('takeWebcamVideo') :
                                             this.i18n('takeWebcamPhoto');
  }

  private getConfirmText_(mode: AvatarCameraMode): string {
    return mode === AvatarCameraMode.VIDEO ? this.i18n('confirmWebcamVideo') :
                                             this.i18n('confirmWebcamPhoto');
  }
}

customElements.define(AvatarCameraElement.is, AvatarCameraElement);

declare global {
  interface HTMLElementTagNameMap {
    'avatar-camera': AvatarCameraElement;
  }
}
