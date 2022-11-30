// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './shimless_rma_shared_css.js';
import './base_page.js';

import {I18nBehavior, I18nBehaviorInterface} from 'chrome://resources/ash/common/i18n_behavior.js';
import {html, mixinBehaviors, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getShimlessRmaService} from './mojo_interface_provider.js';
import {ShimlessRmaServiceInterface, StateResult, WriteProtectDisableCompleteAction} from './shimless_rma_types.js';
import {enableNextButton, focusPageTitle} from './shimless_rma_util.js';

/** @type {!Object<WriteProtectDisableCompleteAction, string>} */
const disableActionTextKeys = {
  [WriteProtectDisableCompleteAction.kSkippedAssembleDevice]:
      'wpDisableReassembleNowText',
  [WriteProtectDisableCompleteAction.kCompleteAssembleDevice]:
      'wpDisableReassembleNowText',
  [WriteProtectDisableCompleteAction.kCompleteKeepDeviceOpen]:
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
      /** @protected */
      actionString_: {
        type: String,
        computed: 'getActionString_(action_)',
      },
    };
  }

  /** @override */
  ready() {
    super.ready();
    enableNextButton(this);

    focusPageTitle(this);
  }

  constructor() {
    super();
    /** @private {ShimlessRmaServiceInterface} */
    this.shimlessRmaService_ = getShimlessRmaService();
    /** @private {WriteProtectDisableCompleteAction} */
    this.action_ = WriteProtectDisableCompleteAction.kUnknown;

    this.shimlessRmaService_.getWriteProtectDisableCompleteAction().then(
        (res) => {
          if (res) {
            this.action_ = res.action;
          }
        });
  }

  /**
   * @return {string}
   * @protected
   */
  getActionString_() {
    return (this.action_ === WriteProtectDisableCompleteAction.kUnknown ||
            this.action_ === WriteProtectDisableCompleteAction.kCompleteNoOp) ?
        '' :
        this.i18n(disableActionTextKeys[this.action_]);
  }

  /** @return {!Promise<!{stateResult: !StateResult}>} */
  onNextButtonClick() {
    return this.shimlessRmaService_.confirmManualWpDisableComplete();
  }

  /**
   * @return {string}
   * @protected
   */
  getVerificationIcon_() {
    return (this.action_ === WriteProtectDisableCompleteAction.kUnknown ||
            this.action_ === WriteProtectDisableCompleteAction.kCompleteNoOp) ?
        '' :
        'shimless-icon:check';
  }
}

customElements.define(
    OnboardingWpDisableCompletePage.is, OnboardingWpDisableCompletePage);
