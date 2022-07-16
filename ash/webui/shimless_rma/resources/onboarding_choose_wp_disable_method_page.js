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
 * 'onboarding-choose-wp-disable-method-page' allows user to select between
 * hardware or RSU write protection disable methods.
 *
 * TODO(joonbug): Change "Manual" description based on enterprise enrollment
 * status.
 */
export class OnboardingChooseWpDisableMethodPageElement extends PolymerElement {
  static get is() {
    return 'onboarding-choose-wp-disable-method-page';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      /** @private */
      hwwpMethod_: {
        type: String,
        value: '',
      },
    };
  }

  constructor() {
    super();
    /** @private {ShimlessRmaServiceInterface} */
    this.shimlessRmaService_ = getShimlessRmaService();
  }

  /**
   * @param {!CustomEvent<{value: string}>} event
   * @protected
   */
  onHwwpDisableMethodSelectionChanged_(event) {
    this.hwwpMethod_ = event.detail.value;
    const disabled = !this.hwwpMethod_;
    this.dispatchEvent(new CustomEvent(
        'disable-next-button',
        {bubbles: true, composed: true, detail: disabled},
        ));
  }

  /** @return {!Promise<!StateResult>} */
  onNextButtonClick() {
    if (this.hwwpMethod_ === 'hwwpDisableMethodManual') {
      return this.shimlessRmaService_.chooseManuallyDisableWriteProtect();
    } else if (this.hwwpMethod_ === 'hwwpDisableMethodRsu') {
      return this.shimlessRmaService_.chooseRsuDisableWriteProtect();
    } else {
      return Promise.reject(new Error('No disable method selected'));
    }
  }
}

customElements.define(
    OnboardingChooseWpDisableMethodPageElement.is,
    OnboardingChooseWpDisableMethodPageElement);
