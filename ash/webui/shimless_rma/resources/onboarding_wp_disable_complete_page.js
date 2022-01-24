// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './shimless_rma_shared_css.js';
import './base_page.js';

import {html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getShimlessRmaService} from './mojo_interface_provider.js';
import {ShimlessRmaServiceInterface, StateResult, WriteProtectDisableCompleteState} from './shimless_rma_types.js';

// TODO(gavindodd): Update text for i18n
/** @type {!Object<!WriteProtectDisableCompleteState, string>} */
const disableStateText = {
  [WriteProtectDisableCompleteState.kSkippedAssembleDevice]:
      'Write protection is already disabled. If the device is disassembled ' +
      'you may reassemble it now.',
  [WriteProtectDisableCompleteState.kCompleteAssembleDevice]:
      'Write protection disable complete, you can reassemble the device.',
  [WriteProtectDisableCompleteState.kCompleteKeepDeviceOpen]:
      'Write protection disable complete, you must keep the device ' +
      'disassembled.',
};

/**
 * @fileoverview
 * 'onboarding-wp-disable-complete-page' notifies the user that manual HWWP
 * disable was successful, and what steps must be taken next.
 */
export class OnboardingWpDisableCompletePage extends PolymerElement {
  static get is() {
    return 'onboarding-wp-disable-complete-page';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      /** @protected {!WriteProtectDisableCompleteState} */
      state_: {
        type: Object,
        value: WriteProtectDisableCompleteState.kUnknown,
      },

      /** @protected */
      statusString_: {
        type: String,
        computed: 'getStatusString_(state_)',
      },
    };
  }

  /** @override */
  ready() {
    super.ready();
    this.shimlessRmaService_.getWriteProtectDisableCompleteState().then(
        (res) => {
          if (res) {
            this.state_ = res.state;
          }
        });

    this.dispatchEvent(new CustomEvent(
        'disable-next-button',
        {bubbles: true, composed: true, detail: false},
        ));
  }

  constructor() {
    super();
    /** @private {ShimlessRmaServiceInterface} */
    this.shimlessRmaService_ = getShimlessRmaService();
  }

  /**
   * @protected
   * @return {string}
   */
  getStatusString_() {
    // TODO(gavindodd): Update text for i18n
    return disableStateText[this.state_];
  }

  /** @return {!Promise<!StateResult>} */
  onNextButtonClick() {
    return this.shimlessRmaService_.confirmManualWpDisableComplete();
  }
}

customElements.define(
    OnboardingWpDisableCompletePage.is, OnboardingWpDisableCompletePage);
