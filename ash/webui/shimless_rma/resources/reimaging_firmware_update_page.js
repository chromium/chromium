// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './shimless_rma_shared_css.js';
import './base_page.js';

import {I18nBehavior, I18nBehaviorInterface} from 'chrome://resources/js/i18n_behavior.m.js';
import {html, mixinBehaviors, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getShimlessRmaService} from './mojo_interface_provider.js';
import {ShimlessRmaServiceInterface, StateResult} from './shimless_rma_types.js';

/**
 * @fileoverview
 * 'reimaging-firmware-update-page' allows user to reimage the firmware via
 * internet or recovery shim.
 * The reimage may be optional in which case skip reimage will be available.
 */

/**
 * @constructor
 * @extends {PolymerElement}
 * @implements {I18nBehaviorInterface}
 */
const ReimagingFirmwareUpdatePageBase =
    mixinBehaviors([I18nBehavior], PolymerElement);

/** @polymer */
export class ReimagingFirmwareUpdatePageElement extends
    ReimagingFirmwareUpdatePageBase {
  static get is() {
    return 'reimaging-firmware-update-page';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      /** @private */
      reimageMethod_: {
        type: String,
        value: '',
      },

      /** @private */
      reimageRequired_: {
        type: Boolean,
        value: true,
      },
    };
  }

  constructor() {
    super();
    /** @private {ShimlessRmaServiceInterface} */
    this.shimlessRmaService_ = getShimlessRmaService();
  }

  /** @override */
  ready() {
    super.ready();
    this.shimlessRmaService_.reimageRequired().then((result) => {
      this.reimageRequired_ =
          (result && result.required != undefined) ? result.required : true;
    });
  }

  /**
   * @param {!CustomEvent<{value: string}>} event
   * @protected
   */
  onFirmwareReimageSelectionChanged_(event) {
    this.reimageMethod_ = event.detail.value;
    const disabled = !this.reimageMethod_;
    this.dispatchEvent(new CustomEvent(
        'disable-next-button',
        {bubbles: true, composed: true, detail: disabled},
        ));
  }

  /**
   * @protected
   * @param {boolean} reimageRequired
   * @returns {string}
   */
  firmwareReimageDownloadMessage_(reimageRequired) {
    if (reimageRequired) {
      return this.i18n('firmwareReimagingDownloadReimageRequired');
    } else {
      return this.i18n('firmwareReimagingDownloadReimageNotRequired');
    }
  }

  /**
   * @protected
   * @param {boolean} reimageRequired
   * @returns {string}
   */
  firmwareReimageUsbMessage_(reimageRequired) {
    if (reimageRequired) {
      return this.i18n('firmwareReimagingRecoveryReimageRequired');
    } else {
      return this.i18n('firmwareReimagingRecoveryReimageNotRequired');
    }
  }

  /** @return {!Promise<!StateResult>} */
  onNextButtonClick() {
    if (this.reimageMethod_ === 'firmwareReimageDownload') {
      return this.shimlessRmaService_.reimageFromDownload();
    } else if (this.reimageMethod_ === 'firmwareReimageUsb') {
      return this.shimlessRmaService_.reimageFromUsb();
    } else if (this.reimageMethod_ === 'firmwareReimageSkip') {
      return this.shimlessRmaService_.reimageSkipped();
    } else {
      return Promise.reject(new Error('No reimage option selected'));
    }
  }

  /** @protected */
  linkToRecoveryClicked_() {
    // TODO(joonbug): Update with real link and check for browser leak.
    window.open('http://www.google.com');
  }
}

customElements.define(
    ReimagingFirmwareUpdatePageElement.is, ReimagingFirmwareUpdatePageElement);
