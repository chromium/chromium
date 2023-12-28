// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './critical_error_page.js';
import './hardware_error_page.js';
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
import './reboot_page.js';
import './reimaging_calibration_failed_page.js';
import './reimaging_calibration_run_page.js';
import './reimaging_calibration_setup_page.js';
import './reimaging_device_information_page.js';
import './reimaging_firmware_update_page.js';
import './reimaging_provisioning_page.js';
import './shimless_rma_shared.css.js';
import './splash_screen.js';
import './wrapup_finalize_page.js';
import './wrapup_repair_complete_page.js';
import './wrapup_restock_page.js';
import './wrapup_wait_for_manual_wp_enable_page.js';
import 'chrome://resources/cr_elements/cr_button/cr_button.js';

import {assert} from 'chrome://resources/ash/common/assert.js';
import {I18nBehavior, I18nBehaviorInterface} from 'chrome://resources/ash/common/i18n_behavior.js';
import {FilePath} from 'chrome://resources/mojo/mojo/public/mojom/base/file_path.mojom-webui.js';
import {mixinBehaviors, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getShimlessRmaService} from './mojo_interface_provider.js';
import {Shimless3pDiagnostics} from './shimless_3p_diagnostics.js';
import {getTemplate} from './shimless_rma.html.js';
import {ErrorObserverInterface, ErrorObserverReceiver, ExternalDiskStateObserverInterface, ExternalDiskStateObserverReceiver, RmadErrorCode, ShimlessRmaServiceInterface, State, StateResult} from './shimless_rma.mojom-webui.js';

/**
 * @typedef {{savePath: FilePath, error: RmadErrorCode}}
 */
export let SaveLogResponse;

/**
 * Enum for the state of USB used for saving logs. The states are transitioned
 * through as the user plugs in a USB then attempts to save the log.
 * @enum {number}
 */
const USBLogState = {
  USB_UNPLUGGED: 0,
  USB_READY: 1,
  SAVING_LOGS: 2,
  LOG_SAVE_SUCCESS: 3,
  LOG_SAVE_FAIL: 4,
};

/**
 * The starting USB state for the logs dialog.
 * @type {!USBLogState}
 */
const DEFAULT_USB_LOG_STATE = USBLogState.USB_READY;

/**
 * Enum for button states.
 * @enum {string}
 */
export const ButtonState = {
  VISIBLE: 'visible',
  DISABLED: 'disabled',
  HIDDEN: 'hidden',
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
 *  buttonExitLabelKey: ?string,
 *  buttonExit: !ButtonState,
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
    buttonExit: ButtonState.HIDDEN,
    buttonBack: ButtonState.HIDDEN,
  },
  [State.kWelcomeScreen]: {
    componentIs: 'onboarding-landing-page',
    requiresReloadWhenShown: false,
    buttonNext: ButtonState.HIDDEN,
    buttonNextLabelKey: 'getStartedButtonLabel',
    buttonExit: ButtonState.HIDDEN,
    buttonBack: ButtonState.HIDDEN,
  },
  [State.kConfigureNetwork]: {
    componentIs: 'onboarding-network-page',
    requiresReloadWhenShown: false,
    buttonNext: ButtonState.DISABLED,
    buttonNextLabelKey: 'skipButtonLabel',
    buttonExit: ButtonState.VISIBLE,
    buttonBack: ButtonState.VISIBLE,
  },
  [State.kUpdateOs]: {
    componentIs: 'onboarding-update-page',
    requiresReloadWhenShown: false,
    buttonNext: ButtonState.DISABLED,
    buttonNextLabelKey: 'skipButtonLabel',
    buttonExit: ButtonState.VISIBLE,
    buttonBack: ButtonState.VISIBLE,
  },
  [State.kSelectComponents]: {
    componentIs: 'onboarding-select-components-page',
    requiresReloadWhenShown: true,
    buttonNext: ButtonState.DISABLED,
    buttonExit: ButtonState.VISIBLE,
    buttonBack: ButtonState.VISIBLE,
  },
  [State.kChooseDestination]: {
    componentIs: 'onboarding-choose-destination-page',
    requiresReloadWhenShown: false,
    buttonNext: ButtonState.DISABLED,
    buttonExit: ButtonState.VISIBLE,
    buttonBack: ButtonState.VISIBLE,
  },
  [State.kChooseWipeDevice]: {
    componentIs: 'onboarding-choose-wipe-device-page',
    requiresReloadWhenShown: false,
    buttonNext: ButtonState.DISABLED,
    buttonExit: ButtonState.VISIBLE,
    buttonBack: ButtonState.VISIBLE,
  },
  [State.kChooseWriteProtectDisableMethod]: {
    componentIs: 'onboarding-choose-wp-disable-method-page',
    requiresReloadWhenShown: false,
    buttonNext: ButtonState.DISABLED,
    buttonExit: ButtonState.VISIBLE,
    buttonBack: ButtonState.VISIBLE,
  },
  [State.kEnterRSUWPDisableCode]: {
    componentIs: 'onboarding-enter-rsu-wp-disable-code-page',
    requiresReloadWhenShown: true,
    buttonNext: ButtonState.DISABLED,
    buttonExit: ButtonState.VISIBLE,
    buttonBack: ButtonState.VISIBLE,
  },
  [State.kWaitForManualWPDisable]: {
    componentIs: 'onboarding-wait-for-manual-wp-disable-page',
    requiresReloadWhenShown: true,
    buttonNext: ButtonState.HIDDEN,
    buttonExit: ButtonState.VISIBLE,
    buttonBack: ButtonState.VISIBLE,
  },
  [State.kWPDisableComplete]: {
    componentIs: 'onboarding-wp-disable-complete-page',
    requiresReloadWhenShown: false,
    buttonNext: ButtonState.DISABLED,
    buttonExit: ButtonState.VISIBLE,
    buttonBack: ButtonState.HIDDEN,
  },
  [State.kUpdateRoFirmware]: {
    componentIs: 'reimaging-firmware-update-page',
    requiresReloadWhenShown: false,
    buttonNext: ButtonState.HIDDEN,
    buttonExit: ButtonState.HIDDEN,
    buttonBack: ButtonState.HIDDEN,
  },
  [State.kUpdateDeviceInformation]: {
    componentIs: 'reimaging-device-information-page',
    requiresReloadWhenShown: false,
    buttonNext: ButtonState.DISABLED,
    buttonExit: ButtonState.HIDDEN,
    buttonBack: ButtonState.HIDDEN,
  },
  [State.kCheckCalibration]: {
    componentIs: 'reimaging-calibration-failed-page',
    requiresReloadWhenShown: true,
    buttonNext: ButtonState.DISABLED,
    buttonExitLabelKey: 'calibrationFailedSkipCalibrationButtonLabel',
    buttonExit: ButtonState.VISIBLE,
    buttonBack: ButtonState.VISIBLE,
  },
  [State.kRunCalibration]: {
    componentIs: 'reimaging-calibration-run-page',
    requiresReloadWhenShown: true,
    buttonNext: ButtonState.DISABLED,
    buttonExit: ButtonState.HIDDEN,
    buttonBack: ButtonState.HIDDEN,
  },
  [State.kSetupCalibration]: {
    componentIs: 'reimaging-calibration-setup-page',
    requiresReloadWhenShown: true,
    buttonNext: ButtonState.DISABLED,
    buttonExit: ButtonState.HIDDEN,
    buttonBack: ButtonState.VISIBLE,
  },
  [State.kProvisionDevice]: {
    componentIs: 'reimaging-provisioning-page',
    requiresReloadWhenShown: true,
    buttonNext: ButtonState.HIDDEN,
    buttonExit: ButtonState.HIDDEN,
    buttonBack: ButtonState.HIDDEN,
  },
  [State.kWaitForManualWPEnable]: {
    componentIs: 'wrapup-wait-for-manual-wp-enable-page',
    requiresReloadWhenShown: true,
    buttonNext: ButtonState.HIDDEN,
    buttonExit: ButtonState.HIDDEN,
    buttonBack: ButtonState.HIDDEN,
  },
  [State.kRestock]: {
    componentIs: 'wrapup-restock-page',
    requiresReloadWhenShown: false,
    buttonNext: ButtonState.HIDDEN,
    buttonExit: ButtonState.HIDDEN,
    buttonBack: ButtonState.VISIBLE,
  },
  [State.kFinalize]: {
    componentIs: 'wrapup-finalize-page',
    buttonNext: ButtonState.HIDDEN,
    buttonExit: ButtonState.HIDDEN,
    buttonBack: ButtonState.HIDDEN,
  },
  [State.kRepairComplete]: {
    componentIs: 'wrapup-repair-complete-page',
    requiresReloadWhenShown: false,
    buttonNext: ButtonState.HIDDEN,
    buttonExit: ButtonState.HIDDEN,
    buttonBack: ButtonState.HIDDEN,
  },
  [State.kHardwareError]: {
    componentIs: 'hardware-error-page',
    requiresReloadWhenShown: false,
    buttonNext: ButtonState.HIDDEN,
    buttonExit: ButtonState.HIDDEN,
    buttonBack: ButtonState.HIDDEN,
  },
  [State.kReboot]: {
    componentIs: 'reboot-page',
    requiresReloadWhenShown: false,
    buttonNext: ButtonState.HIDDEN,
    buttonExit: ButtonState.HIDDEN,
    buttonBack: ButtonState.HIDDEN,
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
    return getTemplate();
  }

  static get properties() {
    return {
      /**
       * Current PageInfo based on current state
       * @protected
       * @type {PageInfo}
       */
      currentPage: {
        reflectToAttribute: true,
        type: Object,
        value: {
          componentIs: 'splash-screen',
          requiresReloadWhenShown: false,
          buttonNext: ButtonState.HIDDEN,
          buttonExit: ButtonState.HIDDEN,
          buttonBack: ButtonState.HIDDEN,
        },
      },

      /** @private {ShimlessRmaServiceInterface} */
      shimlessRmaService: {
        type: Object,
        value: {},
      },

      /**
       * Used to disable all buttons while waiting for long running mojo API
       * calls to complete. Also controls the busy state overlay.
       * TODO(gavindodd): Handle disabling per page buttons.
       * @protected
       */
      allButtonsDisabled: {
        type: Boolean,
        value: true,
        reflectToAttribute: true,
      },

      /**
       * Show busy state overlay while waiting for the service response.
       * @protected
       */
      showBusyStateOverlay: {
        type: Boolean,
        value: false,
        reflectToAttribute: true,
      },

      /**
       * After the next button is clicked, true until the next state is
       * processed.
       * @protected
       */
      nextButtonClicked: {
        type: Boolean,
        value: false,
      },

      /**
       * After the back button is clicked, true until the next state is
       * processed.
       * @protected
       */
      backButtonClicked: {
        type: Boolean,
        value: false,
      },

      /**
       * After the exit button is clicked, true until the next state is
       * processed.
       * @protected
       */
      confirmExitButtonClicked: {
        type: Boolean,
        value: false,
      },

      /** @protected */
      log: {
        type: String,
        value: '',
      },

      /**
       * Tracks the current status of the USB and log saving.
       * @protected {!USBLogState}
       */
      usbLogState: {
        type: Number,
        value: DEFAULT_USB_LOG_STATE,
      },

      /** @protected */
      logSavedStatusText: {
        type: String,
        value: '',
      },
    };
  }

  /** @override */
  constructor() {
    super();
    this.shimlessRmaService = getShimlessRmaService();

    /** @protected {?ErrorObserverReceiver} */
    this.errorObserverReceiver = new ErrorObserverReceiver(
        /**
         * @type {!ErrorObserverInterface}
         */
        (this));

    this.shimlessRmaService.observeError(
        this.errorObserverReceiver.$.bindNewPipeAndPassRemote());

    /** @private {!ExternalDiskStateObserverReceiver} */
    this.externalDiskStateReceiver = new ExternalDiskStateObserverReceiver(
        /** @type {!ExternalDiskStateObserverInterface} */ (this));

    this.shimlessRmaService.observeExternalDiskState(
        this.externalDiskStateReceiver.$.bindNewPipeAndPassRemote());

    /**
     * transitionState is used by page elements to trigger state transition
     * functions and switching to the next page without using the 'Next' button.
     * @private {?Function}
     */
    this.transitionState = (e) => {
      this.setAllButtonsState(
          /* shouldDisableButtons= */ true, /* showBusyStateOverlay= */ true);
      e.detail().then((stateResult) => this.processStateResult(stateResult));
    };

    /**
     * The disableNextButton callback is used by page elements to control the
     * disabled state of the 'Next' button.
     * @private {?Function}
     */
    this.disableNextButtonCallback = (e) => {
      this.currentPage.buttonNext =
          e.detail ? ButtonState.DISABLED : ButtonState.VISIBLE;
      // Allow polymer to observe the changed state.
      this.notifyPath('currentPage.buttonNext');
    };

    /**
     * The enableAllButtons callback is used by page elements to enable all
     * buttons.
     * @private {?Function}
     */
    this.enableAllButtonsCallback = () => {
      this.setAllButtonsState(
          /* shouldDisableButtons= */ false, /* showBusyStateOverlay= */ false);
    };

    /**
     * The disableAllButtons callback is used by page elements to disable all
     * buttons and optionally show a busy overlay.
     * @private {?Function}
     */
    this.disableAllButtonsCallback = (e) => {
      const customEvent =
          /**
             @type {!CustomEvent<{showBusyStateOverlay: boolean}>}
           */
          (e);
      this.setAllButtonsState(
          /* shouldDisableButtons= */ true,
          customEvent.detail.showBusyStateOverlay);
    };

    /**
     * The exitButtonCallback callback is used by the landing page to create
     * its own Exit button in the left pane.
     * @private {?Function}
     */
    this.exitButtonCallback = (e) => {
      this.onExitButtonClicked();
    };

    /**
     * The nextButtonCallback callback is used by the landing page to simulate
     * the next button being clicked.
     * @private {?Function}
     */
    this.nextButtonCallback = (e) => {
      this.onNextButtonClicked();
    };

    /**
     * The setNextButtonLabelCallback callback is used by page elements to set
     * the text label for the 'Next' button.
     * @private {?Function}
     */
    this.setNextButtonLabelCallback = (e) => {
      this.currentPage.buttonNextLabelKey = e.detail;
      this.notifyPath('currentPage.buttonNextLabelKey');
    };

    /**
     * The fatalHardwareErrorCallback callback is used by the finalization
     * page and the provisioning page to tell the app that there is a fatal
     * hardware error.
     * @private {?Function}
     */
    this.fatalHardwareErrorCallback = (event) => {
      const errorState = {
        stateResult: {
          state: State.kHardwareError,
          canExit: false,
          canGoBack: false,
          error: event.detail.fatalErrorCode,
        },
      };
      this.showState(errorState);
    };

    /**
     * Opens the logs dialog.
     * @private {?Function}
     */
    this.openLogsDialogCallback = () => {
      this.openLogsDialog();
    };

    /** @private {?Function} */
    this.onKeyDownCallback = (event) => {
      this.handleKeyboardShortcut(event);
    };
  }

  /** @override */
  connectedCallback() {
    super.connectedCallback();
    window.addEventListener('transition-state', this.transitionState);
    window.addEventListener(
        'disable-next-button', this.disableNextButtonCallback);
    window.addEventListener(
        'set-next-button-label', this.setNextButtonLabelCallback);
    window.addEventListener(
        'disable-all-buttons', this.disableAllButtonsCallback);
    window.addEventListener(
        'enable-all-buttons', this.enableAllButtonsCallback);
    window.addEventListener('click-exit-button', this.exitButtonCallback);
    window.addEventListener('click-next-button', this.nextButtonCallback);
    window.addEventListener(
        'fatal-hardware-error', this.fatalHardwareErrorCallback);
    window.addEventListener('open-logs-dialog', this.openLogsDialogCallback);

    window.addEventListener('keydown', this.onKeyDownCallback);
  }

  /** @override */
  disconnectedCallback() {
    super.disconnectedCallback();
    window.removeEventListener('transition-state', this.transitionState);
    window.removeEventListener(
        'disable-next-button', this.disableNextButtonCallback);
    window.removeEventListener(
        'set-next-button-label', this.setNextButtonLabelCallback);
    window.removeEventListener(
        'disable-all-buttons', this.disableAllButtonsCallback);
    window.removeEventListener(
        'enable-all-buttons', this.enableAllButtonsCallback);
    window.removeEventListener('click-exit-button', this.exitButtonCallback);
    window.removeEventListener('click-next-button', this.nextButtonCallback);
    window.removeEventListener(
        'fatal-hardware-error', this.fatalHardwareErrorCallback);
    window.removeEventListener('open-logs-dialog', this.openLogsDialogCallback);

    window.removeEventListener('keydown', this.onKeyDownCallback);
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

    const splashComponent = this.loadComponent(this.currentPage.componentIs);
    splashComponent.hidden = false;

    // Get the initial state.
    this.shimlessRmaService.getCurrentState().then((stateResult) => {
      this.processStateResult(stateResult);
    });
  }

  /**
   * @param {{stateResult: !StateResult}} stateResult
   * @private
   */
  processStateResult(stateResult) {
    // Do not show the state screen if the critical error screen was shown.
    if (this.handleStandardAndCriticalError(stateResult.stateResult.error)) {
      return;
    }

    // This is a special case for showing the reboot page when the platform
    // sends the error code for expecting a reboot or a shut down.
    if (stateResult.stateResult.error === RmadErrorCode.kExpectReboot ||
        stateResult.stateResult.error === RmadErrorCode.kExpectShutdown) {
      const rebootState = {
        stateResult: {
          state: State.kReboot,
          canExit: false,
          canGoBack: false,
          error: stateResult.stateResult.error,
        },
      };
      this.showState(rebootState);
      return;
    }

    this.showState(stateResult);
  }

  /** @param {!RmadErrorCode} error */
  onError(error) {
    this.handleStandardAndCriticalError(error);
  }

  /**
   * @param {!RmadErrorCode} error
   * @return {boolean}
   * @private
   * Returns true if the critical error screen was displayed.
   */
  handleStandardAndCriticalError(error) {
    // Critical error - expected to be in RMA.
    if (error === RmadErrorCode.kRmaNotRequired) {
      const errorState = {
        stateResult: {
          state: State.kUnknown,
          canExit: false,
          canGoBack: false,
          error: RmadErrorCode.kRmaNotRequired,
        },
      };
      this.showState(errorState);
      return true;
    }

    return false;
  }

  /**
   * @param {{stateResult: !StateResult}} stateResult
   * @private
   */
  showState({stateResult}) {
    // Reset clicked variables to hide the spinners.
    this.nextButtonClicked = false;
    this.backButtonClicked = false;
    this.confirmExitButtonClicked = false;

    const nextStatePageInfo = StateComponentMapping[stateResult.state];
    assert(nextStatePageInfo);

    if (this.currentPage.requiresReloadWhenShown) {
      this.removeComponent(this.currentPage.componentIs);
    }

    // Only perform the below actions if the page needs to change or reload.
    const shouldLoadNextPage = this.currentPage !== nextStatePageInfo ||
        this.currentPage.requiresReloadWhenShown;
    if (shouldLoadNextPage) {
      this.hideAllComponents();

      // Set the next page as the current page.
      this.currentPage = nextStatePageInfo;
      if (!stateResult.canExit) {
        // The calibration failed page is a special case because the Exit button
        // is used as the Skip Calibration button. So we don't want to
        // acknowledge `canExit` here.
        if (this.currentPage.componentIs !==
            'reimaging-calibration-failed-page') {
          this.currentPage.buttonExit = ButtonState.HIDDEN;
        }
      }
      if (!stateResult.canGoBack) {
        this.currentPage.buttonBack = ButtonState.HIDDEN;
      }

      // Load the next page so it's visible.
      const currentPageComponent =
          this.loadComponent(this.currentPage.componentIs);
      currentPageComponent.hidden = false;
      currentPageComponent.errorCode = stateResult.error;
      this.notifyPath('currentPage.buttonNext');
      this.notifyPath('currentPage.buttonExit');
      this.notifyPath('currentPage.buttonBack');

      // A special case for the landing page, which has its own navigation
      // buttons.
      currentPageComponent.getStartedButtonClicked = false;
      currentPageComponent.confirmExitButtonClicked = false;
    }

    this.setAllButtonsState(
        /* shouldDisableButtons= */ false, /* showBusyStateOverlay= */ false);
  }

  /**
   * Utility method to bulk hide all contents.
   */
  hideAllComponents() {
    const components = this.shadowRoot.querySelectorAll('.shimless-content');
    Array.from(components).map((c) => c.hidden = true);
  }

  /**
   * @param {string} componentIs
   * @private
   */
  removeComponent(componentIs) {
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
  loadComponent(componentIs) {
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
  isButtonHidden(button) {
    return button === ButtonState.HIDDEN;
  }

  /**
   * @param {ButtonState} button
   * @protected
   */
  isButtonDisabled(button) {
    return (button === ButtonState.DISABLED) || this.allButtonsDisabled;
  }

  /**
   * @param {boolean} shouldDisableButtons
   * @param {boolean} showBusyStateOverlay
   * @protected
   */
  setAllButtonsState(shouldDisableButtons, showBusyStateOverlay) {
    // `showBusyStateOverlay` should only be true when disabling all buttons.
    assert(!showBusyStateOverlay || shouldDisableButtons);

    this.allButtonsDisabled = shouldDisableButtons;
    this.showBusyStateOverlay = showBusyStateOverlay;
    const component =
        this.shadowRoot.querySelector(`#${this.currentPage.componentIs}`);
    if (!component) {
      return;
    }

    component.allButtonsDisabled = this.allButtonsDisabled;
  }

  /**
   * @param {string} buttonName
   * @param {!ButtonState} buttonState
   */
  updateButtonState(buttonName, buttonState) {
    assert(this.currentPage.hasOwnProperty(buttonName));
    this.set(`currentPage.${buttonName}`, buttonState);
  }

  /** @protected */
  onBackButtonClicked() {
    this.backButtonClicked = true;
    this.setAllButtonsState(
        /* shouldDisableButtons= */ true, /* showBusyStateOverlay= */ true);
    this.shimlessRmaService.transitionPreviousState().then(
        (stateResult) => this.processStateResult(stateResult));
  }

  /** @protected */
  onNextButtonClicked() {
    const page = this.shadowRoot.querySelector(this.currentPage.componentIs);
    assert(page, 'Could not find page ' + this.currentPage.componentIs);
    assert(
        page.onNextButtonClick,
        'No onNextButtonClick for ' + this.currentPage.componentIs);
    assert(
        typeof page.onNextButtonClick === 'function',
        'onNextButtonClick not a function for ' + this.currentPage.componentIs);
    this.nextButtonClicked = true;
    this.setAllButtonsState(
        /* shouldDisableButtons= */ true, /* showBusyStateOverlay= */ true);
    page.onNextButtonClick()
        .then((stateResult) => {
          this.processStateResult(stateResult);
        })
        // TODO(gavindodd): Better error handling.
        .catch((err) => {
          this.nextButtonClicked = false;
          this.setAllButtonsState(
              /* shouldDisableButtons= */ false,
              /* showBusyStateOverlay= */ false);
        });
  }

  /** @protected */
  onExitButtonClicked() {
    const page = this.shadowRoot.querySelector(this.currentPage.componentIs);

    // Don't show the exit dialog if it's on calibration failed page.
    if (page.onExitButtonClick) {
      // A special case for the calibration failed page, where the skip button
      // replaces the exit button.
      // TODO(swifton): find a more straightforward solution for this case.
      page.onExitButtonClick()
          .then((stateResult) => {
            this.processStateResult(stateResult);
          })
          .catch((err) => {
            this.confirmExitButtonClicked = false;
            this.setAllButtonsState(
                /* shouldDisableButtons= */ false,
                /* showBusyStateOverlay= */ false);
          });
    } else {
      this.shadowRoot.querySelector('#exitDialog').showModal();
    }
  }

  /** @protected */
  onConfirmExitButtonClicked() {
    this.confirmExitButtonClicked = true;
    this.shadowRoot.querySelector('#exitDialog').close();

    // Show exit button spinner on the landing page
    const currentPageComponent =
        this.shadowRoot.querySelector(this.currentPage.componentIs);
    currentPageComponent.confirmExitButtonClicked = true;

    this.setAllButtonsState(
        /* shouldDisableButtons= */ true, /* showBusyStateOverlay= */ true);

    this.shimlessRmaService.abortRma().then((result) => {
      this.confirmExitButtonClicked = false;
      this.handleStandardAndCriticalError(result.error);
    });
  }

  /** @protected */
  closeDialog() {
    this.shadowRoot.querySelector('#exitDialog').close();
  }

  /**
   * @return {string}
   * @private
   */
  getNextButtonLabel() {
    return this.i18n(
        this.currentPage.buttonNextLabelKey ?
            this.currentPage.buttonNextLabelKey :
            'nextButtonLabel');
  }

  /**
   * @return {string}
   * @protected
   */
  getExitButtonLabel() {
    return this.i18n(
        this.currentPage.buttonExitLabelKey ?
            this.currentPage.buttonExitLabelKey :
            'exitButtonLabel');
  }

  /** @protected */
  openLogsDialog() {
    this.shimlessRmaService.getLog().then((res) => this.log = res.log);
    const dialog = /** @type {!CrDialogElement} */ (
        this.shadowRoot.querySelector('#logsDialog'));
    if (!dialog.open) {
      dialog.showModal();
    }
  }

  /** @protected */
  launch3pDiagnostics() {
    if (this.allButtonsDisabled) {
      return;
    }

    const diagnostics = /** @type {!Shimless3pDiagnostics} */ (
        this.shadowRoot.querySelector('#shimless3pDiagnostics'));
    diagnostics.launch3pDiagnostics();
  }

  /** @private */
  saveLog() {
    this.shimlessRmaService.saveLog().then(
        /*@type {!SaveLogResponse}*/ (result) => {
          if (result.error === RmadErrorCode.kOk) {
            this.logSavedStatusText =
                this.i18n('rmaLogsSaveSuccessText', result.savePath.path);
            this.usbLogState = USBLogState.LOG_SAVE_SUCCESS;
          } else if (result.error === RmadErrorCode.kUsbNotFound) {
            this.logSavedStatusText = this.i18n('rmaLogsSaveUsbNotFound');
            this.usbLogState = USBLogState.LOG_SAVE_FAIL;
          } else {
            this.logSavedStatusText = this.i18n('rmaLogsSaveFailText');
            this.usbLogState = USBLogState.LOG_SAVE_FAIL;
          }
        });
  }

  /** @protected */
  onSaveLogClick() {
    this.saveLog();
  }

  /** @protected */
  retrySaveLogs() {
    this.saveLog();
  }

  /** @protected */
  closeLogsDialog() {
    this.shadowRoot.querySelector('#logsDialog').close();

    // Reset the USB state back to the default.
    this.usbLogState = DEFAULT_USB_LOG_STATE;
  }

  /**
   * Implements ExternalDiskStateObserver.onExternalDiskStateChanged()
   * @param {boolean} detected
   */
  onExternalDiskStateChanged(detected) {
    if (!detected) {
      this.usbLogState = USBLogState.USB_UNPLUGGED;
      return;
    }

    if (this.usbLogState === USBLogState.USB_UNPLUGGED) {
      this.usbLogState = USBLogState.USB_READY;
    }
  }

  /**
   * @return {boolean}
   * @protected
   */
  shouldShowSaveToUsbButton() {
    return this.usbLogState === USBLogState.USB_READY;
  }

  /**
   * @return {boolean}
   * @protected
   */
  shouldShowLogSaveAttemptContainer() {
    return this.usbLogState === USBLogState.LOG_SAVE_SUCCESS ||
        this.usbLogState === USBLogState.LOG_SAVE_FAIL;
  }

  /**
   * @return {boolean}
   * @protected
   */
  shouldShowRetryButton() {
    return this.usbLogState === USBLogState.LOG_SAVE_FAIL;
  }

  /**
   * @return {boolean}
   * @protected
   */
  shouldShowLogUsbMessageContainer() {
    return this.usbLogState === USBLogState.USB_UNPLUGGED;
  }

  /**
   * @return {string}
   * @protected
   */
  getSaveLogResultIcon() {
    switch (this.usbLogState) {
      case USBLogState.LOG_SAVE_SUCCESS:
        return 'shimless-icon:check';
      case USBLogState.LOG_SAVE_FAIL:
        return 'shimless-icon:warning';
      default:
        return '';
    }
  }

  /**
   * @param {Event} event
   * @private
   */
  handleKeyboardShortcut(event) {
    // Handle `Alt + Shift + {key}` shortcuts.
    if (event.altKey && event.shiftKey) {
      switch (event.key.toLowerCase()) {
        case 'l':
          this.openLogsDialog();
          break;
        case 'd':
          this.launch3pDiagnostics();
          break;
      }
    }
  }
}

customElements.define(ShimlessRma.is, ShimlessRma);
