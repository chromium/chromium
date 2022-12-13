// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './base_page.js';
import './shimless_rma_shared_css.js';
import '//resources/cr_elements/cr_radio_button/cr_radio_button.js';
import '//resources/cr_elements/cr_radio_group/cr_radio_group.js';

import {I18nBehavior, I18nBehaviorInterface} from 'chrome://resources/ash/common/i18n_behavior.js';
import {assert} from 'chrome://resources/ash/common/assert.js';
import {html, mixinBehaviors, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getShimlessRmaService} from './mojo_interface_provider.js';
import {ShimlessRmaServiceInterface, StateResult} from './shimless_rma_types.js';
import {enableNextButton, focusPageTitle} from './shimless_rma_util.js';

/**
 * @fileoverview
 * 'onboarding-choose-wipe-device-page' allows user to select between wiping all
 * the device data at the end of the RMA process or preserving it.
 */

/**
 * @constructor
 * @extends {PolymerElement}
 * @implements {I18nBehaviorInterface}
 */
const OnboardingChooseWipeDevicePageBase =
    mixinBehaviors([I18nBehavior], PolymerElement);

/**
 * Supported options for the wipe device state.
 * @enum {string}
 */
const WipeDeviceOption = {
  WIPE_DEVICE: 'wipeDevice',
  PRESERVE_DATA: 'preserveData',
};

/** @polymer */
export class OnboardingChooseWipeDevicePage extends
    OnboardingChooseWipeDevicePageBase {
  static get is() {
    return 'onboarding-choose-wipe-device-page';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      /**
       * Used to refer to the enum values in HTML file.
       * @protected {?WipeDeviceOption}
       */
      wipeDeviceOption_: {
        type: Object,
        value: WipeDeviceOption,
      },

      // Set by shimless_rma.js.
      allButtonsDisabled: Boolean,

      /** @protected */
      selectedWipeDeviceOption_: {
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
  onOptionChanged_(event) {
    this.selectedWipeDeviceOption_ =
        /** @type {!WipeDeviceOption} */ (event.detail.value);

    // Enable the next button when an option is chosen.
    enableNextButton(this);
  }

  /** @return {!Promise<!{stateResult: !StateResult}>} */
  onNextButtonClick() {
    assert(!!this.selectedWipeDeviceOption_);
    return this.shimlessRmaService_.setWipeDevice(
        this.selectedWipeDeviceOption_ === WipeDeviceOption.WIPE_DEVICE);
  }
}

customElements.define(
    OnboardingChooseWipeDevicePage.is, OnboardingChooseWipeDevicePage);
