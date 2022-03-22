// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './shimless_rma_shared_css.js';
import './base_page.js';
import '//resources/cr_elements/cr_radio_button/cr_radio_button.m.js';
import '//resources/cr_elements/cr_radio_group/cr_radio_group.m.js';

import {I18nBehavior, I18nBehaviorInterface} from 'chrome://resources/js/i18n_behavior.m.js';
import {html, mixinBehaviors, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getShimlessRmaService} from './mojo_interface_provider.js';
import {ShimlessRmaServiceInterface, StateResult} from './shimless_rma_types.js';
import {disableNextButton, enableNextButton} from './shimless_rma_util.js';

/**
 * @fileoverview
 * 'onboarding-choose-destination-page' allows user to select between preparing
 * the device for return to the original owner or refurbishing for a new owner.
 */

/**
 * @constructor
 * @extends {PolymerElement}
 * @implements {I18nBehaviorInterface}
 */
const OnboardingChooseDestinationPageBase =
    mixinBehaviors([I18nBehavior], PolymerElement);

/** @polymer */
export class OnboardingChooseDestinationPageElement extends
    OnboardingChooseDestinationPageBase {
  static get is() {
    return 'onboarding-choose-destination-page';
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

      /** @protected */
      destinationOwner_: {
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
  onDestinationSelectionChanged_(event) {
    this.destinationOwner_ = event.detail.value;
    const disabled = !this.destinationOwner_;
    if (disabled) {
      disableNextButton(this);
    } else {
      enableNextButton(this);
    }
  }

  /** @return {!Promise<!StateResult>} */
  onNextButtonClick() {
    if (this.destinationOwner_ === 'originalOwner') {
      return this.shimlessRmaService_.setSameOwner();
    } else if (this.destinationOwner_ === 'newOwner') {
      return this.shimlessRmaService_.setDifferentOwner();
    } else {
      return Promise.reject(new Error('No destination selected'));
    }
  }
}

customElements.define(
    OnboardingChooseDestinationPageElement.is,
    OnboardingChooseDestinationPageElement);
