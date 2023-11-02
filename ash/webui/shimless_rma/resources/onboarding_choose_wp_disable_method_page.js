// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './shimless_rma_shared_css.js';
import './base_page.js';

import {I18nBehavior, I18nBehaviorInterface} from 'chrome://resources/ash/common/i18n_behavior.js';
import {html, mixinBehaviors, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getShimlessRmaService} from './mojo_interface_provider.js';
import {ShimlessRmaServiceInterface, StateResult} from './shimless_rma_types.js';
import {disableNextButton, enableNextButton, focusPageTitle} from './shimless_rma_util.js';

/**
 * @fileoverview
 * 'onboarding-choose-wp-disable-method-page' allows user to select between
 * hardware or RSU write protection disable methods.
 *
 * TODO(gavindodd): Change "Manual" description based on enterprise enrollment
 * status.
 */

/**
 * @constructor
 * @extends {PolymerElement}
 * @implements {I18nBehaviorInterface}
 */
const OnboardingChooseWpDisableMethodPageBase =
    mixinBehaviors([I18nBehavior], PolymerElement);

/** @polymer */
export class OnboardingChooseWpDisableMethodPage extends
    OnboardingChooseWpDisableMethodPageBase {
  static get is() {
    return 'onboarding-choose-wp-disable-method-page';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      /**
       * Set by shimless_rma.js.
       * @type {boolean}
       */
      allButtonsDisabled: Boolean,

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

  /** @override */
  ready() {
    super.ready();

    focusPageTitle(this);
  }

  /**
   * @param {!CustomEvent<{value: string}>} event
   * @protected
   */
  onHwwpDisableMethodSelectionChanged_(event) {
    this.hwwpMethod_ = event.detail.value;
    const disabled = !this.hwwpMethod_;
    if (disabled) {
      disableNextButton(this);
    } else {
      enableNextButton(this);
    }
  }

  /** @return {!Promise<!{stateResult: !StateResult}>} */
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
    OnboardingChooseWpDisableMethodPage.is,
    OnboardingChooseWpDisableMethodPage);
