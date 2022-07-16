// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './shimless_rma_shared_css.js';
import './base_page.js';

import {html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getShimlessRmaService} from './mojo_interface_provider.js';
import {FinalizationObserverInterface, FinalizationObserverReceiver, FinalizationStatus, ShimlessRmaServiceInterface, StateResult} from './shimless_rma_types.js';

// TODO(gavindodd): Update text for i18n
const openDeviceMessage = 'Open your device and reconnect the battery.';
const hwwpEnabledMessage = 'HWWP enabled.';

// TODO(gavindodd): Update text for i18n
/** @type {!Object<!FinalizationStatus, string>} */
const finalizationStatusText = {
  [FinalizationStatus.kInProgress]: 'In progress...',
  [FinalizationStatus.kComplete]: 'Complete...',
  [FinalizationStatus.kFailedBlocking]: 'Critical failure, cannot continue.',
  [FinalizationStatus.kFailedNonBlocking]: 'Failure.',
};

/**
 * @fileoverview
 * 'wrapup-finalize-page' wait for device finalization and hardware verification
 * to be completed.
 */
export class WrapupFinalizePageElement extends PolymerElement {
  static get is() {
    return 'wrapup-finalize-page';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      /** @protected */
      finalizationMessage_: {
        type: String,
        value: 'Finalizing...',
      },
    };
  }

  constructor() {
    super();
    /** @private {ShimlessRmaServiceInterface} */
    this.shimlessRmaService_ = getShimlessRmaService();
    /** @private {boolean} */
    this.finalizationComplete_ = false;
    /**
     * Receiver responsible for observing hardware write protection state.
     * @private {
     *  ?FinalizationObserverReceiver}
     */
    this.finalizationObserverReceiver_ = new FinalizationObserverReceiver(
        /** @type {!FinalizationObserverInterface} */ (this));

    this.shimlessRmaService_.observeFinalizationStatus(
        this.finalizationObserverReceiver_.$.bindNewPipeAndPassRemote());
  }

  /**
   * @param {!FinalizationStatus} status
   * @param {number} progress
   */
  onFinalizationUpdated(status, progress) {
    if (status == FinalizationStatus.kInProgress) {
      this.finalizationMessage_ = finalizationStatusText[status] + ' ' +
          Math.round(progress * 100) + '%';
    } else {
      this.finalizationMessage_ = finalizationStatusText[status];
    }
    this.finalizationComplete_ = status == FinalizationStatus.kComplete ||
        status == FinalizationStatus.kFailedNonBlocking;
  }

  /** @return {!Promise<!StateResult>} */
  onNextButtonClick() {
    if (this.finalizationComplete_) {
      return this.shimlessRmaService_.finalizationComplete();
    } else {
      return Promise.reject(new Error('Finalization is not complete.'));
    }
  }
}

customElements.define(WrapupFinalizePageElement.is, WrapupFinalizePageElement);
