// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './shimless_rma_shared_css.js';
import './base_page.js';

import {I18nBehavior, I18nBehaviorInterface} from 'chrome://resources/js/i18n_behavior.m.js';
import {html, mixinBehaviors, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getShimlessRmaService} from './mojo_interface_provider.js';
import {HardwareWriteProtectionStateObserverInterface, HardwareWriteProtectionStateObserverReceiver, QrCode, ShimlessRmaServiceInterface, StateResult} from './shimless_rma_types.js';

// The size of each tile in pixels.
const QR_CODE_TILE_SIZE = 5;
// Amount of padding around the QR code in pixels.
const QR_CODE_PADDING = 4 * QR_CODE_TILE_SIZE;
// Styling for filled tiles in the QR code.
const QR_CODE_FILL_STYLE = '#000000';

/**
 * @fileoverview
 * 'onboarding-wait-for-manual-wp-disable-page' wait for the manual HWWP disable
 * to be completed.
 */

/**
 * @constructor
 * @extends {PolymerElement}
 * @implements {I18nBehaviorInterface}
 */
const OnboardingWaitForManualWpDisablePageBase =
    mixinBehaviors([I18nBehavior], PolymerElement);

/** @polymer */
export class OnboardingWaitForManualWpDisablePage extends
    OnboardingWaitForManualWpDisablePageBase {
  static get is() {
    return 'onboarding-wait-for-manual-wp-disable-page';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      /** @protected */
      hwwpEnabled_: {
        type: Boolean,
        value: true,
      },

      /** @protected */
      canvasSize_: {
        type: Number,
        value: 0,
      },

      /** @protected */
      helpUrl_: {
        type: String,
        value: '',
      },
    };
  }

  // TODO(gavindodd): battery_status_card.js uses created() and detached() to
  // create and close observer. Is that the pattern that should be used here?

  constructor() {
    super();
    /** @private {ShimlessRmaServiceInterface} */
    this.shimlessRmaService_ = getShimlessRmaService();
    /** @private {?HardwareWriteProtectionStateObserverReceiver} */
    this.hardwareWriteProtectionStateObserverReceiver_ =
        new HardwareWriteProtectionStateObserverReceiver(
            /** @type {!HardwareWriteProtectionStateObserverInterface} */
            (this));

    this.shimlessRmaService_.observeHardwareWriteProtectionState(
        this.hardwareWriteProtectionStateObserverReceiver_.$
            .bindNewPipeAndPassRemote());
    this.shimlessRmaService_.getWriteProtectManuallyDisabledInstructions().then(
        /*@type {!{string: displayUrl, qrCode: ?QrCode}}*/ (response) =>
            this.updateHelpInstructions_(response));
  }

  /**
   * @public
   * @param {boolean} enabled
   */
  onHardwareWriteProtectionStateChanged(enabled) {
    this.hwwpEnabled_ = enabled;

    if(!this.hidden) {
      // TODO(gavindodd): Should this automatically progress to the next state?
      this.dispatchEvent(new CustomEvent(
          'disable-next-button',
          {bubbles: true, composed: true, detail: this.hwwpEnabled_},
          ));
    }
  }

  /** @return {!Promise<!StateResult>} */
  onNextButtonClick() {
    if (!this.hwwpEnabled_) {
      return this.shimlessRmaService_.writeProtectManuallyDisabled();
    } else {
      return Promise.reject(
          new Error('Hardware Write Protection is not disabled.'));
    }
  }

  /**
   * @param {!{displayUrl: string, qrCode: ?QrCode}} response
   * @private
   */
  updateHelpInstructions_(response) {
    this.helpUrl_ = response.displayUrl;
    this.updateQrCode_(response.qrCode);
  }

  /**
   * @param {?QrCode} qrCode
   * @private
   */
  updateQrCode_(qrCode) {
    if (!qrCode) {
      return;
    }

    this.canvasSize_ = qrCode.size * QR_CODE_TILE_SIZE + 2 * QR_CODE_PADDING;
    const context = this.getCanvasContext_();
    context.clearRect(0, 0, this.canvasSize_, this.canvasSize_);
    context.fillStyle = QR_CODE_FILL_STYLE;
    let index = 0;
    for (let x = 0; x < qrCode.size; x++) {
      for (let y = 0; y < qrCode.size; y++) {
        if (qrCode.data[index]) {
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
   * @return {!CanvasRenderingContext2D}
   * @private
   */
  getCanvasContext_() {
    return this.shadowRoot.querySelector('#qrCodeCanvas').getContext('2d');
  }
}

customElements.define(
    OnboardingWaitForManualWpDisablePage.is,
    OnboardingWaitForManualWpDisablePage);
