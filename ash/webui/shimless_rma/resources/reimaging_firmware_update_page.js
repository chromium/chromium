// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './shimless_rma_shared_css.js';
import './base_page.js';

import {html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getShimlessRmaService} from './mojo_interface_provider.js';
import {ShimlessRmaServiceInterface, StateResult} from './shimless_rma_types.js';

/**
 * @fileoverview
 * 'reimaging-firmware-update-page' allows user to reimage the firmware via
 * internet or recovery shim.
 * The reimage may be optional in which case skip reimage will be available.
 */
export class ReimagingFirmwareUpdatePageElement extends PolymerElement {
  static get is() {
    return 'reimaging-firmware-update-page';
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

      /** @private {string} */
      reimageMethod_: {
        type: String,
        value: '',
      },

      /** @private {boolean} */
      reimageRequired_: {
        type: Boolean,
        value: true,
      },
    };
  }

  /** @override */
  ready() {
    super.ready();
    this.shimlessRmaService_ = getShimlessRmaService();
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
  }

  /**
   * @protected
   * @param {boolean} reimageRequired
   * @returns {string}
   */
  firmwareReimageDownloadMessage_(reimageRequired) {
    // TODO(gavindodd): Update text for i18n
    if (reimageRequired) {
      return 'Download the firmware image';
    } else {
      return 'Yes, download the firmware image';
    }
  }

  /**
   * @protected
   * @param {boolean} reimageRequired
   * @returns {string}
   */
  firmwareReimageUsbMessage_(reimageRequired) {
    // TODO(gavindodd): Update text for i18n
    if (reimageRequired) {
      return 'Use the Chromebook Recovery Utility';
    } else {
      return 'Yes, use the Chromebook Recovery Utility';
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
};

customElements.define(
    ReimagingFirmwareUpdatePageElement.is, ReimagingFirmwareUpdatePageElement);
