// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './shimless_rma_shared_css.js';
import './base_page.js';
import '//resources/cr_elements/cr_input/cr_input.m.js';

import {html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getShimlessRmaService} from './mojo_interface_provider.js';
import {QrCode, ShimlessRmaServiceInterface, StateResult} from './shimless_rma_types.js';

// The size of each tile in pixels.
const QR_CODE_TILE_SIZE = 5;
// Amount of padding around the QR code in pixels.
const QR_CODE_PADDING = 4 * QR_CODE_TILE_SIZE;
// Styling for filled tiles in the QR code.
const QR_CODE_FILL_STYLE = '#000000';

/**
 * @fileoverview
 * 'onboarding-enter-rsu-wp-disable-code-page' asks the user for the RSU disable
 * code.
 */
export class OnboardingEnterRsuWpDisableCodePageElement extends PolymerElement {
  static get is() {
    return 'onboarding-enter-rsu-wp-disable-code-page';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      /** @private {ShimlessRmaServiceInterface} */
      shimlessRmaService_: {
        type: Object,
        value: {},
      },

      /** @protected {number} */
      canvasSize_: {
        type: Number,
        value: 0,
      },

      /** @protected {string} */
      rsuChallenge_: {
        type: String,
        value: '',
      },

      /** @protected {string} */
      rsuCode_: {
        type: String,
        value: '',
      },

    };
  }

  /** @override */
  ready() {
    super.ready();
    this.shimlessRmaService_ = getShimlessRmaService();
    this.getRsuChallenge_();
  }

  /** @private */
  getRsuChallenge_() {
    this.shimlessRmaService_.getRsuDisableWriteProtectChallenge().then(
        (result) => {
          this.rsuChallenge_ = result.challenge;
        });
    this.shimlessRmaService_.getRsuDisableWriteProtectChallengeQrCode().then(
        this.updateQrCode_.bind(this));
  }

  /**
   * @private
   * @param {?{qrCode: QrCode}} response
   */
  updateQrCode_(response) {
    if (!response || !response.qrCode) {
      return;
    }
    this.canvasSize_ =
        response.qrCode.size * QR_CODE_TILE_SIZE + 2 * QR_CODE_PADDING;
    const context = this.getCanvasContext_();
    context.clearRect(0, 0, this.canvasSize_, this.canvasSize_);
    context.fillStyle = QR_CODE_FILL_STYLE;
    let index = 0;
    for (let x = 0; x < response.qrCode.size; x++) {
      for (let y = 0; y < response.qrCode.size; y++) {
        if (response.qrCode.data[index]) {
          context.fillRect(
              x * QR_CODE_TILE_SIZE + QR_CODE_PADDING,
              y * QR_CODE_TILE_SIZE + QR_CODE_PADDING, QR_CODE_TILE_SIZE,
              QR_CODE_TILE_SIZE);
        }
        index++;
      }
    }
  }

  /**
   * @private
   * @return {!CanvasRenderingContext2D}
   */
  getCanvasContext_() {
    return this.shadowRoot.querySelector('#qrCodeCanvas').getContext('2d');
  }

  /** @return {!Promise<!StateResult>} */
  onNextButtonClick() {
    if (this.rsuCode_) {
      return this.shimlessRmaService_.setRsuDisableWriteProtectCode(
          this.rsuCode_);
    } else {
      return Promise.reject(new Error('No RSU code set'));
    }
  }
};

customElements.define(
    OnboardingEnterRsuWpDisableCodePageElement.is,
    OnboardingEnterRsuWpDisableCodePageElement);
