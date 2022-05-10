// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './critical_error_page.js';
import './onboarding_choose_destination_page.js';
import './onboarding_choose_wipe_device_page.js';
import './onboarding_choose_wp_disable_method_page.js';
import './onboarding_enter_rsu_wp_disable_code_page.js';
import './onboarding_landing_page.js';
import './onboarding_network_page.js';
import './onboarding_select_components_page.js';
import './onboarding_update_page.js';
import './onboarding_wait_for_manual_wp_disable_page.js';
import './onboarding_wp_disable_complete_page.js';
import './reimaging_calibration_failed_page.js';
import './reimaging_calibration_run_page.js';
import './reimaging_calibration_setup_page.js';
import './reimaging_device_information_page.js';
import './reimaging_firmware_update_page.js';
import './reimaging_provisioning_page.js';
import './shimless_rma_shared_css.js';
import './splash_screen.js';
import './wrapup_finalize_page.js';
import './wrapup_repair_complete_page.js';
import './wrapup_restock_page.js';
import './wrapup_wait_for_manual_wp_enable_page.js';
import 'chrome://resources/cr_elements/cr_button/cr_button.m.js';

import {assert} from 'chrome://resources/js/assert.m.js';
import {I18nBehavior, I18nBehaviorInterface} from 'chrome://resources/js/i18n_behavior.m.js';
import {html, mixinBehaviors, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getShimlessRmaService} from './mojo_interface_provider.js';
import {ErrorObserverInterface, ErrorObserverReceiver, RmadErrorCode, ShimlessRmaServiceInterface, State, StateResult} from './shimless_rma_types.js';

/**
 * Enum for button states.
 * @enum {string}
 */
export const ButtonState = {
  VISIBLE: 'visible',
  DISABLED: 'disabled',
  HIDDEN: 'hidden'
};

/** @type {number} */
const HEADER_FOOTER_HEIGHT_PX = 80;

/** @type {number} */
const OOBE_LARGE_SCREEN_WIDTH_PX = 80;

/**
 * @typedef {{
 *  componentIs: string,
 *  requiresReloadWhenShown: boolean,
 *  buttonNext: !ButtonState,
 *  buttonNextLabelKey: ?string,
 *  buttonCancelLabelKey: ?string,
 *  buttonCancel: !ButtonState,
 *  buttonBack: !ButtonState,
 * }}
 */
let PageInfo;

/**
 * @type {!Object<!State, !PageInfo>}
 */
export const StateComponentMapping = {
  // It is assumed that if state is kUnknown the error is kRmaNotRequired.
  [State.kUnknown]: {
    componentIs: 'critical-error-page',
    requiresReloadWhenShown: false,
    buttonNext: ButtonState.HIDDEN,
    buttonCancel: ButtonState.HIDDEN,
    buttonBack: ButtonState.HIDDEN,
  },
  [State.kWelcomeScreen]: {
    componentIs: 'onboarding-landing-page',
    requiresReloadWhenShown: false,
    buttonNext: ButtonState.HIDDEN,
    buttonNextLabelKey: 'getStartedButtonLabel',
    buttonCancel: ButtonState.HIDDEN,
    buttonBack: ButtonState.VISIBLE,
  },
  [State.kConfigureNetwork]: {
    componentIs: 'onboarding-network-page',
    requiresReloadWhenShown: false,
    buttonNext: ButtonState.DISABLED,
    buttonNextLabelKey: 'skipButtonLabel',
    buttonCancel: ButtonState.VISIBLE,
    buttonBack: ButtonState.VISIBLE,
  },
  [State.kUpdateOs]: {
    componentIs: 'onboarding-update-page',
    requiresReloadWhenShown: false,
    buttonNext: ButtonState.DISABLED,
    buttonNextLabelKey: 'skipButtonLabel',
    buttonCancel: ButtonState.VISIBLE,
    buttonBack: ButtonState.VISIBLE,
  },
  [State.kSelectComponents]: {
    componentIs: 'onboarding-select-components-page',
    requiresReloadWhenShown: false,
    buttonNext: ButtonState.DISABLED,
    buttonCancel: ButtonState.VISIBLE,
    buttonBack: ButtonState.VISIBLE,
  },
  [State.kChooseDestination]: {
    componentIs: 'onboarding-choose-destination-page',
    requiresReloadWhenShown: false,
    buttonNext: ButtonState.DISABLED,
    buttonCancel: ButtonState.VISIBLE,
    buttonBack: ButtonState.VISIBLE,
  },
  [State.kChooseWipeDevice]: {
    componentIs: 'onboarding-choose-wipe-device-page',
    requiresReloadWhenShown: false,
    buttonNext: ButtonState.DISABLED,
    buttonCancel: ButtonState.VISIBLE,
    buttonBack: ButtonState.VISIBLE,
  },
  [State.kChooseWriteProtectDisableMethod]: {
    componentIs: 'onboarding-choose-wp-disable-method-page',
    requiresReloadWhenShown: false,
    buttonNext: ButtonState.DISABLED,
    buttonCancel: ButtonState.VISIBLE,
    buttonBack: ButtonState.VISIBLE,
  },
  [State.kEnterRSUWPDisableCode]: {
    componentIs: 'onboarding-enter-rsu-wp-disable-code-page',
    requiresReloadWhenShown: true,
    buttonNext: ButtonState.DISABLED,
    buttonCancel: ButtonState.VISIBLE,
    buttonBack: ButtonState.VISIBLE,
  },
  [State.kWaitForManualWPDisable]: {
    componentIs: 'onboarding-wait-for-manual-wp-disable-page',
    requiresReloadWhenShown: true,
    buttonNext: ButtonState.HIDDEN,
    buttonCancel: ButtonState.VISIBLE,
    buttonBack: ButtonState.VISIBLE,
  },
  [State.kWPDisableComplete]: {
    componentIs: 'onboarding-wp-disable-complete-page',
    requiresReloadWhenShown: false,
    buttonNext: ButtonState.DISABLED,
    buttonCancel: ButtonState.VISIBLE,
    buttonBack: ButtonState.VISIBLE,
  },
  [State.kUpdateRoFirmware]: {
    componentIs: 'reimaging-firmware-update-page',
    requiresReloadWhenShown: false,
    buttonNext: ButtonState.HIDDEN,
    buttonCancel: ButtonState.HIDDEN,
    buttonBack: ButtonState.VISIBLE,
  },
  [State.kUpdateDeviceInformation]: {
    componentIs: 'reimaging-device-information-page',
    requiresReloadWhenShown: false,
    buttonNext: ButtonState.DISABLED,
    buttonCancel: ButtonState.HIDDEN,
    buttonBack: ButtonState.VISIBLE,
  },
  [State.kCheckCalibration]: {
    componentIs: 'reimaging-calibration-failed-page',
    requiresReloadWhenShown: true,
    buttonNext: ButtonState.DISABLED,
    buttonCancelLabelKey: 'calibrationFailedSkipCalibrationButtonLabel',
    buttonCancel: ButtonState.VISIBLE,
    buttonBack: ButtonState.VISIBLE,
  },
  [State.kRunCalibration]: {
    componentIs: 'reimaging-calibration-run-page',
    requiresReloadWhenShown: true,
    buttonNext: ButtonState.DISABLED,
    buttonCancel: ButtonState.HIDDEN,
    buttonBack: ButtonState.VISIBLE,
  },
  [State.kSetupCalibration]: {
    componentIs: 'reimaging-calibration-setup-page',
    requiresReloadWhenShown: true,
    buttonNext: ButtonState.DISABLED,
    buttonCancel: ButtonState.HIDDEN,
    buttonBack: ButtonState.VISIBLE,
  },
  [State.kProvisionDevice]: {
    componentIs: 'reimaging-provisioning-page',
    requiresReloadWhenShown: true,
    buttonNext: ButtonState.HIDDEN,
    buttonCancel: ButtonState.HIDDEN,
    buttonBack: ButtonState.VISIBLE,
  },
  [State.kWaitForManualWPEnable]: {
    componentIs: 'wrapup-wait-for-manual-wp-enable-page',
    requiresReloadWhenShown: true,
    buttonNext: ButtonState.HIDDEN,
    buttonCancel: ButtonState.HIDDEN,
    buttonBack: ButtonState.VISIBLE,
  },
  [State.kRestock]: {
    componentIs: 'wrapup-restock-page',
    requiresReloadWhenShown: false,
    buttonNext: ButtonState.HIDDEN,
    buttonCancel: ButtonState.HIDDEN,
    buttonBack: ButtonState.VISIBLE,
  },
  [State.kFinalize]: {
    componentIs: 'wrapup-finalize-page',
    buttonNext: ButtonState.HIDDEN,
    buttonCancel: ButtonState.HIDDEN,
    buttonBack: ButtonState.VISIBLE,
  },
  [State.kRepairComplete]: {
    componentIs: 'wrapup-repair-complete-page',
    requiresReloadWhenShown: false,
    buttonNext: ButtonState.HIDDEN,
    buttonCancel: ButtonState.HIDDEN,
    buttonBack: ButtonState.VISIBLE,
  },
};

/**
 * @fileoverview
 * 'shimless-rma' is the main page for the shimless rma process modal dialog.
 */

/**
 * @constructor
 * @extends {PolymerElement}
 * @implements {I18nBehaviorInterface}
 */
const ShimlessRmaBase = mixinBehaviors([I18nBehavior], PolymerElement);

/** @polymer */
export class ShimlessRma extends ShimlessRmaBase {
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
        value: {
          componentIs: 'splash-screen',
          requiresReloadWhenShown: false,
          buttonNext: ButtonState.HIDDEN,
          buttonCancel: ButtonState.HIDDEN,
          buttonBack: ButtonState.HIDDEN,
        },
      },

      /** @private {ShimlessRmaServiceInterface} */
      shimlessRmaService_: {
        type: Object,
        value: {},
      },

      /**
       * Used to disable all buttons while waiting for long running mojo API
       * calls to complete. Also controls the busy state overlay.
       * TODO(gavindodd): Handle disabling per page buttons.
       * @protected
       */
      allButtonsDisabled_: {
        type: Boolean,
        value: true,
        reflectToAttribute: true,
      },

      /**
       * Show busy state overlay while waiting for the service response.
       * @protected
       */
      showBusyStateOverlay_: {
        type: Boolean,
        value: false,
        reflectToAttribute: true,
      },

      /**
       * After the next button is clicked, true until the next state is
       * processed.
       * @protected
       */
      nextButtonClicked_: {
        type: Boolean,
        value: false,
      },

      /**
       * After the back button is clicked, true until the next state is
       * processed.
       * @protected
       */
      backButtonClicked_: {
        type: Boolean,
        value: false,
      },

      /**
       * After the cancel button is clicked, true until the next state is
       * processed.
       * @protected
       */
      cancelButtonClicked_: {
        type: Boolean,
        value: false,
      },
    };
  }

  /** @override */
  constructor() {
    super();
    this.shimlessRmaService_ = getShimlessRmaService();

    /** @protected {?ErrorObserverReceiver} */
    this.errorObserverReceiver_ = new ErrorObserverReceiver(
        /**
         * @type {!ErrorObserverInterface}
         */
        (this));

    this.shimlessRmaService_.observeError(
        this.errorObserverReceiver_.$.bindNewPipeAndPassRemote());

    /**
     * transitionState_ is used by page elements to trigger state transition
     * functions and switching to the next page without using the 'Next' button.
     * @private {?Function}
     */
    this.transitionState_ = (e) => {
      this.setAllButtonsState_(
          /* shouldDisableButtons= */ true, /* showBusyStateOverlay= */ true);
      e.detail().then((stateResult) => this.processStateResult_(stateResult));
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

    /**
     * The enableAllButtons callback is used by page elements to enable all
     * buttons.
     * @private {?Function}
     */
    this.enableAllButtonsCallback_ = () => {
      this.setAllButtonsState_(
          /* shouldDisableButtons= */ false, /* showBusyStateOverlay= */ false);
    };

    /**
     * The disableAllButtons callback is used by page elements to disable all
     * buttons and optionally show a busy overlay.
     * @private {?Function}
     */
    this.disableAllButtonsCallback_ = (e) => {
      const customEvent =
          /**
             @type {!CustomEvent<{showBusyStateOverlay: boolean}>}
           */
          (e);
      this.setAllButtonsState_(
          /* shouldDisableButtons= */ true,
          customEvent.detail.showBusyStateOverlay);
    };

    /**
     * The cancelButtonCallback_ callback is used by the landing page to create
     * its own Cancel button in the left pane.
     * @private {?Function}
     */
    this.cancelButtonCallback_ = (e) => {
      this.onCancelButtonClicked_();
    };

    /**
     * The nextButtonCallback_ callback is used by the landing page to simulate
     * the next button being clicked.
     * @private {?Function}
     */
    this.nextButtonCallback_ = (e) => {
      this.onNextButtonClicked_();
    };

    /**
     * The setNextButtonLabelCallback callback is used by page elements to set
     * the text label for the 'Next' button.
     * @private {?Function}
     */
    this.setNextButtonLabelCallback_ = (e) => {
      this.currentPage_.buttonNextLabelKey = e.detail;
      this.notifyPath('currentPage_.buttonNextLabelKey');
    };
  }

  /** @override */
  connectedCallback() {
    super.connectedCallback();
    window.addEventListener('transition-state', this.transitionState_);
    window.addEventListener(
        'disable-next-button', this.disableNextButtonCallback_);
    window.addEventListener(
        'set-next-button-label', this.setNextButtonLabelCallback_);
    window.addEventListener(
        'disable-all-buttons', this.disableAllButtonsCallback_);
    window.addEventListener(
        'enable-all-buttons', this.enableAllButtonsCallback_);
    window.addEventListener('click-cancel-button', this.cancelButtonCallback_);
    window.addEventListener('click-next-button', this.nextButtonCallback_);
  }

  /** @override */
  disconnectedCallback() {
    super.disconnectedCallback();
    window.removeEventListener('transition-state', this.transitionState_);
    window.removeEventListener(
        'disable-next-button', this.disableNextButtonCallback_);
    window.removeEventListener(
        'set-next-button-label', this.setNextButtonLabelCallback_);
    window.removeEventListener(
        'disable-all-buttons', this.disableAllButtonsCallback_);
    window.removeEventListener(
        'enable-all-buttons', this.enableAllButtonsCallback_);
    window.removeEventListener(
        'click-cancel-button', this.cancelButtonCallback_);
    window.removeEventListener('click-next-button', this.nextButtonCallback_);
  }

  /** @override */
  ready() {
    super.ready();

    this.style.setProperty(
        '--header-footer-height', `${HEADER_FOOTER_HEIGHT_PX}px`);

    const screenWidth = window.innerWidth;
    const containerHorizontalPadding =
        screenWidth > OOBE_LARGE_SCREEN_WIDTH_PX ? ((screenWidth - 1040) / 2) :
                                                   (screenWidth * .08);
    this.style.setProperty(
        '--container-horizontal-padding', `${containerHorizontalPadding}px`);

    const contentContainerWidth =
        screenWidth - (containerHorizontalPadding * 2);
    this.style.setProperty(
        '--content-container-width', `${contentContainerWidth}px`);

    const screenHeight = window.innerHeight;
    const containerVerticalPadding = screenHeight * .06;
    this.style.setProperty(
        '--container-vertical-padding', `${containerVerticalPadding}px`);

    const contentContainerHeight = screenHeight -
        (containerVerticalPadding * 2) - (HEADER_FOOTER_HEIGHT_PX * 2);
    this.style.setProperty(
        '--content-container-height', `${contentContainerHeight}px`);

    const splashComponent = this.loadComponent_(this.currentPage_.componentIs);
    splashComponent.hidden = false;

    // Get the initial state.
    this.shimlessRmaService_.getCurrentState().then((stateResult) => {
      this.processStateResult_(stateResult);
    });
  }

  /**
   * @param {!StateResult} stateResult
   * @private
   */
  processStateResult_(stateResult) {
    // Do not show the state screen if the critical error screen was shown.
    if (this.handleStandardAndCriticalError_(stateResult.error)) {
      return;
    }
    this.showState_(stateResult);
  }

  /** @param {!RmadErrorCode} error */
  onError(error) {
    this.handleStandardAndCriticalError_(error);
  }

  /**
   * @param {!RmadErrorCode} error
   * @return {boolean}
   * @private
   * Returns true if the critical error screen was displayed.
   */
  handleStandardAndCriticalError_(error) {
    // Critical error - expected to be in RMA.
    if (error === RmadErrorCode.kRmaNotRequired) {
      const errorState = {
        state: State.kUnknown,
        canCancel: false,
        canGoBack: false,
        error: RmadErrorCode.kRmaNotRequired
      };
      this.showState_(errorState);
      return true;
    }

    return false;
  }

  /**
   * @param {!StateResult} stateResult
   * @private
   */
  showState_(stateResult) {
    // Reset clicked variables to hide the spinners.
    this.nextButtonClicked_ = false;
    this.backButtonClicked_ = false;
    this.cancelButtonClicked_ = false;

    const nextStatePageInfo = StateComponentMapping[stateResult.state];
    assert(nextStatePageInfo);

    if (this.currentPage_.requiresReloadWhenShown) {
      this.removeComponent_(this.currentPage_.componentIs);
    }

    // Only perform the below actions if the page needs to change or reload.
    const shouldLoadNextPage = this.currentPage_ !== nextStatePageInfo ||
        this.currentPage_.requiresReloadWhenShown;
    if (shouldLoadNextPage) {
      this.hideAllComponents_();

      // Set the next page as the current page.
      this.currentPage_ = nextStatePageInfo;
      if (!stateResult.canCancel) {
        this.currentPage_.buttonCancel = ButtonState.HIDDEN;
      }
      if (!stateResult.canGoBack) {
        this.currentPage_.buttonBack = ButtonState.HIDDEN;
      }

      // Load the next page so it's visible.
      const currentPageComponent =
          this.loadComponent_(this.currentPage_.componentIs);
      currentPageComponent.hidden = false;
      currentPageComponent.errorCode = stateResult.error;

      // A special case for the landing page, which has its own navigation
      // buttons.
      currentPageComponent.getStartedButtonClicked = false;
      currentPageComponent.landingCancelButtonClicked = false;
    }

    this.setAllButtonsState_(
        /* shouldDisableButtons= */ false, /* showBusyStateOverlay= */ false);
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
  removeComponent_(componentIs) {
    const currentPageComponent =
        this.shadowRoot.querySelector(`#${componentIs}`);
    assert(!!currentPageComponent);
    currentPageComponent.remove();
  }

  /**
   * @param {string} componentIs
   * @return {!Element}
   * @private
   */
  loadComponent_(componentIs) {
    const alreadyLoadedComponent =
        this.shadowRoot.querySelector(`#${componentIs}`);
    if (alreadyLoadedComponent) {
      return alreadyLoadedComponent;
    }

    const shimlessBody = this.shadowRoot.querySelector('#contentContainer');

    /** @type {!Element} */
    const component = document.createElement(componentIs);
    component.setAttribute('id', componentIs);
    component.setAttribute('class', 'shimless-content');
    component.hidden = true;

    shimlessBody.appendChild(component);
    return component;
  }

  /** @protected */
  isButtonHidden_(button) {
    return button === ButtonState.HIDDEN;
  }

  /**
   * @param {ButtonState} button
   * @protected
   */
  isButtonDisabled_(button) {
    return (button === ButtonState.DISABLED) || this.allButtonsDisabled_;
  }

  /**
   * @param {boolean} shouldDisableButtons
   * @param {boolean} showBusyStateOverlay
   * @protected
   */
  setAllButtonsState_(shouldDisableButtons, showBusyStateOverlay) {
    // `showBusyStateOverlay` should only be true when disabling all buttons.
    assert(!showBusyStateOverlay || shouldDisableButtons);

    this.allButtonsDisabled_ = shouldDisableButtons;
    this.showBusyStateOverlay_ = showBusyStateOverlay;
    const component =
        this.shadowRoot.querySelector(`#${this.currentPage_.componentIs}`);
    if (!component) {
      return;
    }

    component.allButtonsDisabled = this.allButtonsDisabled_;
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
    this.backButtonClicked_ = true;
    this.setAllButtonsState_(
        /* shouldDisableButtons= */ true, /* showBusyStateOverlay= */ true);
    this.shimlessRmaService_.transitionPreviousState().then(
        (stateResult) => this.processStateResult_(stateResult));
  }

  /** @protected */
  onNextButtonClicked_() {
    const page = this.shadowRoot.querySelector(this.currentPage_.componentIs);
    assert(page, 'Could not find page ' + this.currentPage_.componentIs);
    assert(
        page.onNextButtonClick,
        'No onNextButtonClick for ' + this.currentPage_.componentIs);
    assert(
        typeof page.onNextButtonClick === 'function',
        'onNextButtonClick not a function for ' +
            this.currentPage_.componentIs);
    this.nextButtonClicked_ = true;
    this.setAllButtonsState_(
        /* shouldDisableButtons= */ true, /* showBusyStateOverlay= */ true);
    page.onNextButtonClick()
        .then((stateResult) => {
          this.processStateResult_(stateResult);
        })
        // TODO(gavindodd): Better error handling.
        .catch((err) => {
          this.nextButtonClicked_ = false;
          this.setAllButtonsState_(
              /* shouldDisableButtons= */ false,
              /* showBusyStateOverlay= */ false);
        });
  }

  /** @protected */
  onCancelButtonClicked_() {
    this.cancelButtonClicked_ = true;
    this.setAllButtonsState_(
        /* shouldDisableButtons= */ true, /* showBusyStateOverlay= */ true);
    const page = this.shadowRoot.querySelector(this.currentPage_.componentIs);
    if (page.onCancelButtonClick) {
      // A special case for the calibration failed page, where the skip button
      // replaces the cancel button.
      // TODO(swifton): find a more straightforward solution for this case.
      page.onCancelButtonClick()
          .then((stateResult) => {
            this.processStateResult_(stateResult);
          })
          .catch((err) => {
            this.cancelButtonClicked_ = false;
            this.setAllButtonsState_(
                /* shouldDisableButtons= */ false,
                /* showBusyStateOverlay= */ false);
          });
    } else {
      this.shimlessRmaService_.abortRma().then((result) => {
        this.cancelButtonClicked_ = false;
        this.handleStandardAndCriticalError_(result.error);
      });
    }
  }

  /**
   * @return {string}
   * @private
   */
  getNextButtonLabel_() {
    return this.i18n(
        this.currentPage_.buttonNextLabelKey ?
            this.currentPage_.buttonNextLabelKey :
            'nextButtonLabel');
  }

  /**
   * @return {string}
   * @protected
   */
  getCancelButtonLabel_() {
    return this.i18n(
        this.currentPage_.buttonCancelLabelKey ?
            this.currentPage_.buttonCancelLabelKey :
            'cancelButtonLabel');
  }
}

customElements.define(ShimlessRma.is, ShimlessRma);
