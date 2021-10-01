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
import './onboarding_verify_rsu_page.js';
import './onboarding_wp_disable_complete_page.js';
import './reimaging_calibration_page.js';
import './reimaging_calibration_run_page.js';
import './reimaging_calibration_setup_page.js';
import './reimaging_device_information_page.js';
import './reimaging_firmware_update_page.js';
import './reimaging_provisioning_page.js';
import './shimless_rma_shared_css.js';
import './wrapup_finalize_page.js';
import './wrapup_repair_complete_page.js';
import './wrapup_restock_page.js';
import './wrapup_wait_for_manual_wp_enable_page.js';
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
  DISABLED: 'disabled',
  HIDDEN: 'hidden'
};

/**
 * @typedef {{
 *  componentIs: string,
 *  requiresReloadWhenShown: boolean,
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
    requiresReloadWhenShown: false,
    buttonNext: ButtonState.HIDDEN,
    buttonCancel: ButtonState.HIDDEN,
    buttonBack: ButtonState.HIDDEN,
  },
  [RmaState.kWelcomeScreen]: {
    componentIs: 'onboarding-landing-page',
    requiresReloadWhenShown: false,
    buttonNext: ButtonState.DISABLED,
    buttonCancel: ButtonState.HIDDEN,
    buttonBack: ButtonState.HIDDEN,
  },
  [RmaState.kConfigureNetwork]: {
    componentIs: 'onboarding-network-page',
    requiresReloadWhenShown: false,
    buttonNext: ButtonState.DISABLED,
    buttonCancel: ButtonState.HIDDEN,
    buttonBack: ButtonState.HIDDEN,
  },
  [RmaState.kUpdateOs]: {
    componentIs: 'onboarding-update-page',
    requiresReloadWhenShown: false,
    buttonNext: ButtonState.DISABLED,
    buttonCancel: ButtonState.HIDDEN,
    buttonBack: ButtonState.HIDDEN,
  },
  [RmaState.kSelectComponents]: {
    componentIs: 'onboarding-select-components-page',
    requiresReloadWhenShown: false,
    buttonNext: ButtonState.DISABLED,
    buttonCancel: ButtonState.HIDDEN,
    buttonBack: ButtonState.HIDDEN,
  },
  [RmaState.kChooseDestination]: {
    componentIs: 'onboarding-choose-destination-page',
    requiresReloadWhenShown: false,
    buttonNext: ButtonState.DISABLED,
    buttonCancel: ButtonState.HIDDEN,
    buttonBack: ButtonState.HIDDEN,
  },
  [RmaState.kChooseWriteProtectDisableMethod]: {
    componentIs: 'onboarding-choose-wp-disable-method-page',
    requiresReloadWhenShown: false,
    buttonNext: ButtonState.DISABLED,
    buttonCancel: ButtonState.HIDDEN,
    buttonBack: ButtonState.HIDDEN,
  },
  [RmaState.kEnterRSUWPDisableCode]: {
    componentIs: 'onboarding-enter-rsu-wp-disable-code-page',
    requiresReloadWhenShown: true,
    buttonNext: ButtonState.DISABLED,
    buttonCancel: ButtonState.HIDDEN,
    buttonBack: ButtonState.HIDDEN,
  },
  [RmaState.kVerifyRsu]: {
    componentIs: 'onboarding-verify-rsu-page',
    requiresReloadWhenShown: false,
    buttonNext: ButtonState.DISABLED,
    buttonCancel: ButtonState.HIDDEN,
    buttonBack: ButtonState.HIDDEN,
  },
  [RmaState.kWaitForManualWPDisable]: {
    componentIs: 'onboarding-wait-for-manual-wp-disable-page',
    requiresReloadWhenShown: true,
    buttonNext: ButtonState.DISABLED,
    buttonCancel: ButtonState.HIDDEN,
    buttonBack: ButtonState.HIDDEN,
  },
  [RmaState.kWPDisableComplete]: {
    componentIs: 'onboarding-wp-disable-complete-page',
    requiresReloadWhenShown: false,
    buttonNext: ButtonState.DISABLED,
    buttonCancel: ButtonState.HIDDEN,
    buttonBack: ButtonState.HIDDEN,
  },
  [RmaState.kChooseFirmwareReimageMethod]: {
    componentIs: 'reimaging-firmware-update-page',
    requiresReloadWhenShown: false,
    buttonNext: ButtonState.DISABLED,
    buttonCancel: ButtonState.HIDDEN,
    buttonBack: ButtonState.HIDDEN,
  },
  [RmaState.kUpdateDeviceInformation]: {
    componentIs: 'reimaging-device-information-page',
    requiresReloadWhenShown: false,
    buttonNext: ButtonState.DISABLED,
    buttonCancel: ButtonState.HIDDEN,
    buttonBack: ButtonState.HIDDEN,
  },
  [RmaState.kCheckCalibration]: {
    componentIs: 'reimaging-calibration-page',
    requiresReloadWhenShown: true,
    buttonNext: ButtonState.DISABLED,
    buttonCancel: ButtonState.HIDDEN,
    buttonBack: ButtonState.HIDDEN,
  },
  [RmaState.kRunCalibration]: {
    componentIs: 'reimaging-calibration-run-page',
    requiresReloadWhenShown: true,
    buttonNext: ButtonState.DISABLED,
    buttonCancel: ButtonState.HIDDEN,
    buttonBack: ButtonState.HIDDEN,
  },
  [RmaState.kSetupCalibration]: {
    componentIs: 'reimaging-calibration-setup-page',
    requiresReloadWhenShown: true,
    buttonNext: ButtonState.DISABLED,
    buttonCancel: ButtonState.HIDDEN,
    buttonBack: ButtonState.HIDDEN,
  },
  [RmaState.kProvisionDevice]: {
    componentIs: 'reimaging-provisioning-page',
    requiresReloadWhenShown: true,
    buttonNext: ButtonState.DISABLED,
    buttonCancel: ButtonState.HIDDEN,
    buttonBack: ButtonState.HIDDEN,
  },
  [RmaState.kWaitForManualWPEnable]: {
    componentIs: 'wrapup-wait-for-manual-wp-enable-page',
    requiresReloadWhenShown: true,
    buttonNext: ButtonState.DISABLED,
    buttonCancel: ButtonState.HIDDEN,
    buttonBack: ButtonState.HIDDEN,
  },
  [RmaState.kRestock]: {
    componentIs: 'wrapup-restock-page',
    requiresReloadWhenShown: false,
    buttonNext: ButtonState.DISABLED,
    buttonCancel: ButtonState.HIDDEN,
    buttonBack: ButtonState.HIDDEN,
  },
  [RmaState.kFinalize]: {
    componentIs: 'wrapup-finalize-page',
    buttonNext: ButtonState.VISIBLE,
    buttonCancel: ButtonState.HIDDEN,
    buttonBack: ButtonState.HIDDEN,
  },
  [RmaState.kRepairComplete]: {
    componentIs: 'wrapup-repair-complete-page',
    requiresReloadWhenShown: false,
    buttonNext: ButtonState.HIDDEN,
    buttonCancel: ButtonState.HIDDEN,
    buttonBack: ButtonState.HIDDEN,
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
  constructor() {
    super();

    /**
     * The loadNextState callback is used by page elements to trigger loading
     * the next page without using the 'Next' button.
     * @private {?Function}
     */
    this.loadNextStateCallback_ = (e) => {
      this.processStateResult_(e.detail)
    };

    /**
     * The disableNextButton callback is used by page elements to control the
     * disabled state of the 'Next' button.
     * @private {?Function}
     */
    this.disableNextButtonCallback_ = (e) => {
      this.currentPage_.buttonNext =
          e.detail ? ButtonState.DISABLED : ButtonState.VISIBLE;
      // Allow polymer to observe the changed state.
      this.notifyPath('currentPage_.buttonNext');
    };
  }

  /** @override */
  connectedCallback() {
    super.connectedCallback();

    window.addEventListener('load-next-state', this.loadNextStateCallback_);
    window.addEventListener(
        'disable-next-button', this.disableNextButtonCallback_);
  }

  /** @override */
  disconnectedCallback() {
    super.disconnectedCallback();

    window.removeEventListener('load-next-state', this.loadNextStateCallback_);
    window.removeEventListener(
        'disable-next-button', this.disableNextButtonCallback_);
  }

  /** @override */
  ready() {
    super.ready();
    this.shimlessRmaService_ = getShimlessRmaService();

    // Get the initial state.
    this.fetchState_().then((stateResult) => {
      this.initialState_ = stateResult.state;
      this.processStateResult_(stateResult);
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
   * @param {!StateResult} stateResult
   */
  processStateResult_(stateResult) {
    this.handleError_(stateResult.error);
    this.showState_(
        stateResult.state, stateResult.canCancel, stateResult.canGoBack);
  }

  /**
   * @private
   * @param {!RmadErrorCode} error
   */
  handleError_(error) {
    // TODO(gavindodd): Handle error appropriately
    this.errorMessage_ = rmadErrorString(error);
  }

  /**
   * @private
   * @param {!RmaState} state
   * @param {boolean} canCancel
   * @param {boolean} canGoBack
   */
  showState_(state, canCancel, canGoBack) {
    const pageInfo = StateComponentMapping[state];
    assert(pageInfo);

    pageInfo.buttonCancel =
        canCancel ? ButtonState.VISIBLE : ButtonState.HIDDEN;
    pageInfo.buttonBack = canGoBack ? ButtonState.VISIBLE : ButtonState.HIDDEN;

    // TODO(gavindodd): Replace this with 'dom-if' in html.
    if (!!this.currentPage_ && this.currentPage_.requiresReloadWhenShown) {
      let component =
          this.shadowRoot.querySelector(`#${this.currentPage_.componentIs}`);
      if (component !== null) {
        component.remove();
        component = null;
      }
    } else if (pageInfo == this.currentPage_) {
      // Make sure all button states are correct.
      this.notifyPath('currentPage_.buttonNext');
      this.notifyPath('currentPage_.buttonBack');
      this.notifyPath('currentPage_.buttonCancel');
      return;
    }
    this.currentPage_ = pageInfo;
    let component =
        this.shadowRoot.querySelector(`#${this.currentPage_.componentIs}`);
    if (component === null) {
      component = this.loadComponent_(this.currentPage_.componentIs);
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
    return button === ButtonState.HIDDEN;
  }

  /** @protected */
  isButtonDisabled_(button) {
    return button === ButtonState.DISABLED;
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
    this.fetchPrevState_().then(
        (stateResult) => this.processStateResult_(stateResult));
  }

  /** @protected */
  onNextButtonClicked_() {
    const page = this.shadowRoot.querySelector(this.currentPage_.componentIs);
    assert(page);

    // Acquire promise to check whether current page is ready for next page.
    const prepPageAdvance =
        page.onNextButtonClick || (() => Promise.resolve(undefined));
    assert(typeof prepPageAdvance === 'function');

    prepPageAdvance.call(page)
        .then(
            (stateResult) => !!stateResult ? Promise.resolve(stateResult) :
                                             this.fetchNextState_())
        .then((stateResult) => this.processStateResult_(stateResult))
        .catch((err) => void 0);
  }

  /** @protected */
  onCancelButtonClicked_() {
    this.shimlessRmaService_.abortRma().then(
        (result) => this.handleError_(result.error));
  }
};

customElements.define(ShimlessRmaElement.is, ShimlessRmaElement);
