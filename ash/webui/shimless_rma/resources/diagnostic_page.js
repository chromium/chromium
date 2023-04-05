// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './base_page.js';
import './shimless_rma_shared_css.js';

import {I18nBehavior, I18nBehaviorInterface} from 'chrome://resources/ash/common/i18n_behavior.js';
import {loadTimeData} from 'chrome://resources/ash/common/load_time_data.m.js';
import {html, mixinBehaviors, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getShimlessRmaService} from './mojo_interface_provider.js';
import {ShimlessRmaServiceInterface} from './shimless_rma_types.js';
import {focusPageTitle} from './shimless_rma_util.js';

/**
 * @fileoverview
 * 'diagnostic-page' displays critical device info to help technicians review
 * the current device state.
 */

/**
 * @constructor
 * @extends {PolymerElement}
 * @implements {I18nBehaviorInterface}
 */
const DiagnosticPageBase = mixinBehaviors([I18nBehavior], PolymerElement);

/** @polymer */
export class DiagnosticPage extends DiagnosticPageBase {
  static get is() {
    return 'diagnostic-page';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      // Set by shimless_rma.js.
      allButtonsDisabled: Boolean,

      /**
       * Media track representing the front camera if present.
       * @private {?MediaStreamTrack}
       */
      frontCameraTrack_: Object,

      /**
       * Media track representing the rear camera if present.
       * @private {?MediaStreamTrack}
       */
      rearCameraTrack_: Object,

      /**
       * Media track representing the internal microphone if present.
       * @private {?MediaStreamTrack}
       */
      audioTrack_: Object,
    };
  }

  constructor() {
    if (!loadTimeData.getBoolean('diagnosticPageEnabled')) {
      return;
    }

    super();
    /** @private {ShimlessRmaServiceInterface} */
    this.shimlessRmaService_ = getShimlessRmaService();
  }

  /** @override */
  ready() {
    super.ready();

    focusPageTitle(this);

    this.loadMediaTracks_();
  }

  /**
   * Sets the media tracks for front camera, rear camera, and microphones. The
   * cameras are temporarily enabled to access their info.
   * @private
   */
  loadMediaTracks_() {
    navigator.mediaDevices
        .getUserMedia({
          video: {
            facingMode: {exact: 'user'},
          },
        })
        .then(stream => {
          this.frontCameraTrack_ = stream.getVideoTracks()[0];
          this.frontCameraTrack_.stop();
        })
        .catch(err => {});

    navigator.mediaDevices
        .getUserMedia({
          video: {
            facingMode: {exact: 'environment'},
          },
        })
        .then(stream => {
          this.rearCameraTrack_ = stream.getVideoTracks()[0];
          this.rearCameraTrack_.stop();
        })
        .catch(err => {});

    navigator.mediaDevices
        .getUserMedia({
          audio: true,
        })
        .then(stream => {
          this.audioTrack_ = stream.getAudioTracks()[0];
          this.audioTrack_.stop();
        })
        .catch(err => {});
  }

  /**
   * @param {!MediaStreamTrack} mediaTrack
   * @return {string}
   * @private
   */
  getCameraResolution_(mediaTrack) {
    const width =
        /** @type {!ConstrainDouble} */ (mediaTrack.getCapabilities().width)
            .max;
    const height =
        /** @type {!ConstrainDouble} */ (mediaTrack.getCapabilities().height)
            .max;
    return `${width} x ${height}`;
  }

  /**
   * @param {!MediaStreamTrack} mediaTrack
   * @return {string}
   * @private
   */
  getCameraFps_(mediaTrack) {
    return `${
        /** @type {!ConstrainDouble} */ (mediaTrack.getCapabilities().frameRate)
            .max} fps`;
  }

  /**
   * @param {!MediaStreamTrack} mediaTrack
   * @return {string}
   * @private
   */
  getAudioChannel_(mediaTrack) {
    const channelCount = /** @type {!ConstrainDouble} */ (
                             mediaTrack.getCapabilities().channelCount)
                             .max;
    return channelCount >= 2 ? 'Stereo' : 'Mono';
  }
}

customElements.define(DiagnosticPage.is, DiagnosticPage);
