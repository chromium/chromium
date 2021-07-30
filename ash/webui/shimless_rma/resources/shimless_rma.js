// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './onboarding_choose_destination_page.js';
import './onboarding_choose_wp_disable_method_page.js';
import './onboarding_enter_rsu_wp_disable_code_page.js';
import './onboarding_landing_page.js';
import './onboarding_network_page.js';
import './onboarding_select_components_page.js';
import './onboarding_update_page.js';
import './onboarding_wait_for_manual_wp_disable_page.js';
import './onboarding_wp_disable_complete_page.js';
import './reimaging_calibration_page.js';
import './reimaging_device_information_page.js';
import './reimaging_firmware_update_page.js';
import './reimaging_provisioning_page.js';
import './shimless_rma_shared_css.js';
import './wrapup_repair_complete_page.js';
import 'chrome://resources/cr_elements/cr_button/cr_button.m.js';

import {assert} from 'chrome://resources/js/assert.m.js';
import {html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getShimlessRmaService, rmadErrorString} from './mojo_interface_provider.js';
import {RmadErrorCode, RmaState, ShimlessRmaServiceInterface, StateResult} from './shimless_rma_types.js'

/**
 * Enum for button states.
 * @enum {string}
 */
export const ButtonState = {
  VISIBLE: 'visible',
  DISABLED: 'disable',
  HIDDEN: 'hidden'
};

/**
 * @typedef {{
 *  componentIs: string,
 *  buttonNext: !ButtonState,
 *  buttonNextLabel: string,
 *  buttonCancel: !ButtonState,
 *  buttonBack: !ButtonState,
 * }}
 */
let PageInfo;

/**
 * @type {!Object<!RmaState, !PageInfo>}
 */
const StateComponentMapping = {
  [RmaState.kUnknown]: {
    componentIs: 'badcomponent',
    buttonNext: ButtonState.HIDDEN,
    buttonCancel: ButtonState.VISIBLE,
    buttonBack: ButtonState.HIDDEN,
  },
  [RmaState.kWelcomeScreen]: {
    componentIs: 'onboarding-landing-page',
    buttonNext: ButtonState.VISIBLE,
    buttonCancel: ButtonState.VISIBLE,
    buttonBack: ButtonState.HIDDEN,
  },
  [RmaState.kConfigureNetwork]: {
    componentIs: 'onboarding-network-page',
    buttonNext: ButtonState.VISIBLE,
    buttonCancel: ButtonState.VISIBLE,
    buttonBack: ButtonState.VISIBLE,
  },
  [RmaState.kChooseDestination]: {
    componentIs: 'onboarding-choose-destination-page',
    buttonNext: ButtonState.VISIBLE,
    buttonCancel: ButtonState.VISIBLE,
    buttonBack: ButtonState.VISIBLE,
  },
  [RmaState.kChooseWriteProtectDisableMethod]: {
    componentIs: 'onboarding-choose-wp-disable-method-page',
    buttonNext: ButtonState.VISIBLE,
    buttonCancel: ButtonState.VISIBLE,
    buttonBack: ButtonState.VISIBLE,
  },
  [RmaState.kWaitForManualWPDisable]: {
    componentIs: 'onboarding-wait-for-manual-wp-disable-page',
    buttonNext: ButtonState.VISIBLE,
    buttonCancel: ButtonState.HIDDEN,
    buttonBack: ButtonState.VISIBLE,
  },
  [RmaState.kUpdateOs]: {
    componentIs: 'onboarding-update-page',
    buttonNext: ButtonState.VISIBLE,
    buttonCancel: ButtonState.VISIBLE,
    buttonBack: ButtonState.VISIBLE,
  },
  [RmaState.kSelectComponents]: {
    componentIs: 'onboarding-select-components-page',
    buttonNext: ButtonState.VISIBLE,
    buttonCancel: ButtonState.VISIBLE,
    buttonBack: ButtonState.VISIBLE,
  },
  [RmaState.kEnterRSUWPDisableCode]: {
    componentIs: 'onboarding-enter-rsu-wp-disable-code-page',
    buttonNext: ButtonState.VISIBLE,
    buttonCancel: ButtonState.HIDDEN,
    buttonBack: ButtonState.VISIBLE,
  },
  [RmaState.kWPDisableComplete]: {
    componentIs: 'onboarding-wp-disable-complete-page',
    buttonNext: ButtonState.VISIBLE,
    buttonCancel: ButtonState.HIDDEN,
    buttonBack: ButtonState.VISIBLE,
  },
  [RmaState.kChooseFirmwareReimageMethod]: {
    componentIs: 'reimaging-firmware-update-page',
    buttonNext: ButtonState.VISIBLE,
    buttonCancel: ButtonState.HIDDEN,
    buttonBack: ButtonState.VISIBLE,
  },
  [RmaState.kUpdateDeviceInformation]: {
    componentIs: 'reimaging-device-information-page',
    btnNext: ButtonState.VISIBLE,
    btnCancel: ButtonState.HIDDEN,
    btnBack: ButtonState.VISIBLE,
  },
  [RmaState.kCheckCalibration]: {
    componentIs: 'reimaging-calibration-page',
    buttonNext: ButtonState.VISIBLE,
    buttonCancel: ButtonState.HIDDEN,
    buttonBack: ButtonState.VISIBLE,
  },
  [RmaState.kProvisionDevice]: {
    componentIs: 'reimaging-provisioning-page',
    btnNext: ButtonState.VISIBLE,
    btnCancel: ButtonState.HIDDEN,
    btnBack: ButtonState.VISIBLE,
  },
  [RmaState.kRepairComplete]: {
    componentIs: 'wrapup-repair-complete-page',
    buttonNext: ButtonState.VISIBLE,
    buttonCancel: ButtonState.HIDDEN,
    buttonBack: ButtonState.VISIBLE,
  },
};

/**
 * @fileoverview
 * 'shimless-rma' is the main page for the shimless rma process modal dialog.
 */
export class ShimlessRmaElement extends PolymerElement {
  static get is() {
    return 'shimless-rma';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      /**
       * Current PageInfo based on current state
       * @protected
       * @type {PageInfo}
       */
      currentPage_: {
        reflectToAttribute: true,
        type: Object,
        value: {},
      },

      /** @private {ShimlessRmaServiceInterface} */
      shimlessRmaService_: {
        type: Object,
        value: {},
      },

      /**
       * Initial state to cancel to
       * @private {?RmaState}
       */
      initialState_: {
        type: Object,
        value: null,
      },

      /** @protected {string} */
      errorMessage_: {
        type: String,
        value: '',
      }
    };
  }

  /** @override */
  ready() {
    super.ready();
    this.shimlessRmaService_ = getShimlessRmaService();

    // Get the initial state.
    this.fetchState_().then((stateResult) => {
      // TODO(gavindodd): Handle stateResult.error
      this.errorMessage_ = rmadErrorString(stateResult.error);
      this.initialState_ = stateResult.state;
      this.loadState_(stateResult.state);
    });
  }

  /** @private */
  fetchState_() {
    return this.shimlessRmaService_.getCurrentState();
  }

  /** @private */
  fetchNextState_() {
    return this.shimlessRmaService_.transitionNextState();
  }

  /** @private */
  fetchPrevState_() {
    return this.shimlessRmaService_.transitionPreviousState();
  }

  /**
   * @private
   * @param {!RmaState} state
   */
  loadState_(state) {
    const pageInfo = StateComponentMapping[state];
    assert(pageInfo);

    this.currentPage_ = pageInfo;
    this.showComponent_(pageInfo.componentIs);
  }

  /**
   * @param {string} componentIs
   * @private
   */
  showComponent_(componentIs) {
    let component = this.shadowRoot.querySelector(`#${componentIs}`);
    if (component === null) {
      component = this.loadComponent_(componentIs);
    }

    this.hideAllComponents_();
    component.hidden = false;
  }

  /**
   * Utility method to bulk hide all contents.
   */
  hideAllComponents_() {
    const components = this.shadowRoot.querySelectorAll('.shimless-content');
    Array.from(components).map((c) => c.hidden = true);
  }

  /**
   * @param {string} componentIs
   * @private
   */
  loadComponent_(componentIs) {
    const shimlessBody = this.shadowRoot.querySelector('#contentContainer');

    let component = document.createElement(componentIs);
    component.setAttribute('id', componentIs);
    component.setAttribute('class', 'shimless-content');
    component.hidden = true;

    if (!component) {
      console.error('Failed to create ' + componentIs);
    }

    shimlessBody.appendChild(component);
    return component;
  }

  /** @protected */
  isButtonHidden_(button) {
    return button === 'hidden';
  }

  /** @protected */
  isButtonDisabled_(button) {
    return button === 'disabled';
  }

  /**
   * @param {string} buttonName
   * @param {!ButtonState} buttonState
   */
  updateButtonState(buttonName, buttonState) {
    assert(this.currentPage_.hasOwnProperty(buttonName));
    this.set(`currentPage_.${buttonName}`, buttonState);
  }

  /** @protected */
  onBackButtonClicked_() {
    this.fetchPrevState_().then((stateResult) => {
      this.errorMessage_ = rmadErrorString(stateResult.error);
      this.loadState_(stateResult.state);
    });
  }

  /** @protected */
  onNextButtonClicked_() {
    const page = this.shadowRoot.querySelector(this.currentPage_.componentIs);
    assert(page);

    // Acquire promise to check whether current page is ready for next page.
    const prepPageAdvance =
        page.onNextButtonClick || (() => Promise.resolve(undefined));
    assert(typeof prepPageAdvance === 'function');

    // TODO(gavindodd): Handle stateResult.error
    prepPageAdvance.call(page)
        .then(
            (stateResult) => !!stateResult ? Promise.resolve(stateResult) :
                                             this.fetchNextState_())
        .then((stateResult) => {
          this.errorMessage_ = rmadErrorString(stateResult.error);
          this.loadState_(stateResult.state);
        })
        .catch((err) => void 0);
  }

  /** @protected */
  onCancelButtonClicked_() {
    // TODO(gavindodd): This should call abortRma
    this.errorMessage_ = '';
    this.loadState_(assert(this.initialState_));
  }
};

customElements.define(ShimlessRmaElement.is, ShimlessRmaElement);
