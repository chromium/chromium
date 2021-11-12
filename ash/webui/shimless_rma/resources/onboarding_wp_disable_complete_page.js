// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './shimless_rma_shared_css.js';
import './base_page.js';

import {I18nBehavior, I18nBehaviorInterface} from 'chrome://resources/js/i18n_behavior.m.js';
import {html, mixinBehaviors, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getShimlessRmaService} from './mojo_interface_provider.js';
import {ShimlessRmaServiceInterface, StateResult, WriteProtectDisableCompleteState} from './shimless_rma_types.js';

/** @type {!Object<!WriteProtectDisableCompleteState, string>} */
const disableStateTextKeys = {
  [WriteProtectDisableCompleteState.kSkippedAssembleDevice]:
      'wpDisableSkippedText',
  [WriteProtectDisableCompleteState.kCompleteAssembleDevice]:
      'wpDisableReassembleNowText',
  [WriteProtectDisableCompleteState.kCompleteKeepDeviceOpen]:
      'wpDisableLeaveDisassembledText',
};

/**
 * @fileoverview
 * 'onboarding-wp-disable-complete-page' notifies the user that manual HWWP
 * disable was successful, and what steps must be taken next.
 */

/**
 * @constructor
 * @extends {PolymerElement}
 * @implements {I18nBehaviorInterface}
 */
const OnboardingWpDisableCompletePageBase =
    mixinBehaviors([I18nBehavior], PolymerElement);

/** @polymer */
export class OnboardingWpDisableCompletePage extends
    OnboardingWpDisableCompletePageBase {
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
    this.dispatchEvent(new CustomEvent(
        'disable-next-button',
        {bubbles: true, composed: true, detail: false},
        ));
  }

  constructor() {
    super();
    /** @private {ShimlessRmaServiceInterface} */
    this.shimlessRmaService_ = getShimlessRmaService();
    this.shimlessRmaService_.getWriteProtectDisableCompleteState().then(
        (res) => {
          if (res) {
            this.state_ = res.state;
          }
        });
  }

  /**
   * @protected
   * @return {string}
   */
  getStatusString_() {
    if (this.state_ === WriteProtectDisableCompleteState.kUnknown) {
      return '';
    } else {
      return this.i18n(disableStateTextKeys[this.state_]);
    }
  }

  /** @return {!Promise<!StateResult>} */
  onNextButtonClick() {
    return this.shimlessRmaService_.confirmManualWpDisableComplete();
  }
}

customElements.define(
    OnboardingWpDisableCompletePage.is, OnboardingWpDisableCompletePage);
