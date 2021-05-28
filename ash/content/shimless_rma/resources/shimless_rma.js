// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_button/cr_button.m.js';
import './onboarding_choose_destination_page.js';
import './onboarding_choose_wp_disable_method_page.js';
import './onboarding_enter_rsu_wp_disable_code_page.js';
import './onboarding_landing_page.js';
import './onboarding_select_components_page.js';
import './onboarding_update_page.js';
import './onboarding_wait_for_manual_wp_disable_page.js';
import './shimless_rma_shared_css.js';

import {assert} from 'chrome://resources/js/assert.m.js';
import {html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getShimlessRmaService} from './mojo_interface_provider.js';
import {CurrentState, NextState, PrevState, RmadErrorCode, RmaState, ShimlessRmaServiceInterface} from './shimless_rma_types.js'

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
  // TODO(joonbug): update to correct RmaState
  [RmaState.kSelectComponents]: {
    componentIs: 'onboarding-update-page',
    btnNext: ButtonState.VISIBLE,
    btnCancel: ButtonState.VISIBLE,
    btnBack: ButtonState.VISIBLE,
  },
  [RmaState.kChooseDestination]: {
    componentIs: 'onboarding-choose-destination-page',
    btnNext: ButtonState.HIDDEN,
    btnCancel: ButtonState.VISIBLE,
    btnBack: ButtonState.VISIBLE,
  },
  [RmaState.kChooseWriteProtectDisableMethod]: {
    componentIs: 'onboarding-choose-wp-disable-method-page',
    btnNext: ButtonState.VISIBLE,
    btnCancel: ButtonState.VISIBLE,
    btnBack: ButtonState.VISIBLE,
  },
  [RmaState.kWaitForManualWPDisable]: {
    componentIs: 'onboarding-wait-for-manual-wp-disable-page',
    btnNext: ButtonState.VISIBLE,
    btnCancel: ButtonState.HIDDEN,
    btnBack: ButtonState.VISIBLE,
  },
  [RmaState.kUpdateChrome]: {
    componentIs: 'onboarding-update-page',
    btnNext: ButtonState.VISIBLE,
    btnCancel: ButtonState.VISIBLE,
    btnBack: ButtonState.VISIBLE,
  },
  [RmaState.kSelectComponents]: {
    componentIs: 'onboarding-select-components-page',
    btnNext: ButtonState.HIDDEN,
    btnCancel: ButtonState.VISIBLE,
    btnBack: ButtonState.VISIBLE,
  },
  [RmaState.kEnterRSUWPDisableCode]: {
    componentIs: 'onboarding-enter-rsu-wp-disable-code-page',
    btnNext: ButtonState.HIDDEN,
    btnCancel: ButtonState.HIDDEN,
    btnBack: ButtonState.VISIBLE,
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
      }
    };
  }

  /** @override */
  ready() {
    super.ready();
    this.shimlessRmaService_ = getShimlessRmaService();

    // Get the initial state.
    this.fetchState_().then((state) => {
      this.initialState_ = state.currentState;
      this.loadState_(state.currentState);
    });
  }

  /** @private */
  fetchState_() {
    return this.shimlessRmaService_.getCurrentState();
  }

  /** @private */
  fetchNextState_() {
    return this.shimlessRmaService_.getNextState();
  }

  /** @private */
  fetchPrevState_() {
    return this.shimlessRmaService_.getPrevState();
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
    // TODO(joonbug): error handling based on state.error
    this.fetchPrevState_().then((state) => this.loadState_(state.prevState));
  }

  /** @protected */
  onNextButtonClicked_() {
    const page = this.shadowRoot.querySelector(this.currentPage_.componentIs);
    assert(page);

    // Acquire promise to check whether current page is ready for next page.
    const prepPageAdvance =
        page.onNextButtonClick || (() => Promise.resolve(RmaState.kUnknown));
    assert(typeof prepPageAdvance === 'function');

    prepPageAdvance()
        .then(
            (rmaState) => !!rmaState ? Promise.resolve({nextState: rmaState}) :
                                       this.fetchNextState_())
        .then((state) => this.loadState_(state.nextState))
        .catch((err) => void 0);
  }

  /** @protected */
  onCancelButtonClicked_() {
    this.loadState_(assert(this.initialState_));
  }
};

customElements.define(ShimlessRmaElement.is, ShimlessRmaElement);
