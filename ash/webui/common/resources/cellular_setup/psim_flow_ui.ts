// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './setup_loading_page.js';
import './provisioning_page.js';
import './final_page.js';
import '//resources/polymer/v3_0/iron-pages/iron-pages.js';

import {I18nMixin} from '//resources/ash/common/cr_elements/i18n_mixin.js';
import {assert, assertNotReached} from '//resources/js/assert.js';
import {ActivationDelegateReceiver, ActivationResult, CarrierPortalHandlerRemote, CarrierPortalStatus, CellularMetadata, CellularSetupInterface} from '//resources/mojo/chromeos/ash/services/cellular_setup/public/mojom/cellular_setup.mojom-webui.js';
import {PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {CellularSetupDelegate} from './cellular_setup_delegate.js';
import {ButtonState} from './cellular_types.js';
import {FinalPageElement} from './final_page.js';
import {getCellularSetupRemote} from './mojo_interface_provider.js';
import {ProvisioningPageElement} from './provisioning_page.js';
import {getTemplate} from './psim_flow_ui.html.js';
import {SetupLoadingPageElement} from './setup_loading_page.js';
import {SubflowMixin} from './subflow_mixin.js';

export enum PsimPageName {
  SIM_DETECT = 'simDetectPage',
  PROVISIONING = 'provisioningPage',
  FINAL = 'finalPage',
}

export enum PsimUiState {
  IDLE = 'idle',
  STARTING_ACTIVATION = 'starting-activation',
  WAITING_FOR_ACTIVATION_TO_START = 'waiting-for-activation-to-start',
  TIMEOUT_START_ACTIVATION = 'timeout-start-activation',
  FINAL_TIMEOUT_START_ACTIVATION = 'final-timeout-start-activation',
  WAITING_FOR_PORTAL_TO_LOAD = 'waiting-for-portal-to-load',
  TIMEOUT_PORTAL_LOAD = 'timeout-portal-load',
  WAITING_FOR_USER_PAYMENT = 'waiting-for-user-payment',
  WAITING_FOR_ACTIVATION_TO_FINISH = 'waiting-for-activation-to-finish',
  TIMEOUT_FINISH_ACTIVATION = 'timeout-finish-activation',
  ACTIVATION_SUCCESS = 'activation-success',
  ALREADY_ACTIVATED = 'already-activated',
  ACTIVATION_FAILURE = 'activation-failure',
}

// The reason that caused the user to exit the PSim Setup flow.
// These values are persisted to logs. Entries should not be renumbered
// and numeric values should never be reused.
export enum PsimSetupFlowResult {
  SUCCESS = 0,
  CANCELLED = 1,
  CANCELLED_NO_SIM = 2,
  CANCELLED_COLD_SIM_DEFER = 3,
  CANCELLED_CARRIER_PORTAL = 4,
  CANCELLED_PORTAL_ERROR = 5,
  CARRIER_PORTAL_TIMEOUT = 6,
  NETWORK_ERROR = 7,
}

/**
 * The time delta, in ms, for the timeout corresponding to |state|. If no
 * timeout is applicable for this state, null is returned.
 */
function getTimeoutMsForPsimUiState(state: PsimUiState): number|null {
  // In some cases, starting activation may require power-cycling the device's
  // modem, a process that can take several seconds.
  if (state === PsimUiState.STARTING_ACTIVATION) {
    return 10000;  // 10 seconds.
  }

  // The portal is a website served by the mobile carrier.
  if (state === PsimUiState.WAITING_FOR_PORTAL_TO_LOAD) {
    return 10000;  // 10 seconds.
  }

  // Finishing activation only requires sending a D-Bus message to Shill.
  if (state === PsimUiState.WAITING_FOR_ACTIVATION_TO_FINISH) {
    return 1000;  // 1 second.
  }

  // No other states require timeouts.
  return null;
}

/**
 * The maximum tries allowed to detect the SIM.
 */
const MAX_START_ACTIVATION_ATTEMPTS = 3;

export const PSIM_SETUP_RESULT_METRIC_NAME =
    'Network.Cellular.PSim.SetupFlowResult';

export const SUCCESSFUL_PSIM_SETUP_DURATION_METRIC_NAME =
    'Network.Cellular.PSim.CellularSetup.Success.Duration';

export const FAILED_PSIM_SETUP_DURATION_METRIC_NAME =
    'Network.Cellular.PSim.CellularSetup.Failure.Duration';

/**
 * Root element for the pSIM cellular setup flow. This element interacts with
 * the CellularSetup service to carry out the psim activation flow. It
 * contains navigation buttons and sub-pages corresponding to each step of the
 * flow.
 */
const PsimFlowUiElementBase = SubflowMixin(I18nMixin(PolymerElement));

export class PsimFlowUiElement extends PsimFlowUiElementBase {
  static get is() {
    return 'psim-flow-ui' as const;
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      delegate: Object,

      /**
       * Carrier name; used in dialog title to show the current carrier
       * name being setup
       */
      nameOfCarrierPendingSetup: {
        type: String,
        notify: true,
        computed: 'getCarrierText(' +
            'selectedPsimPageName_, cellularMetadata_.*)',
      },

      forwardButtonLabel: {
        type: String,
        notify: true,
      },

      state_: {
        type: String,
        value: PsimUiState.IDLE,
        observer: 'handlePsimUiStateChange_',
      },

      /**
       * Element name of the current selected sub-page.
       */
      selectedPsimPageName_: {
        type: String,
        value: PsimPageName.SIM_DETECT,
        notify: true,
      },

      /**
       * DOM Element for the current selected sub-page.
       */
      selectedPage_: Object,

      /**
       * Whether error state should be shown for the current page.
       */
      showError_: {type: Boolean, value: false},

      /**
       * Cellular metadata received via the onActivationStarted() callback. If
       * that callback has not occurred, this field is null.
       */
      cellularMetadata_: {
        type: Object,
        value: null,
      },

      /**
       * The current number of tries to detect the SIM.
       */
      startActivationAttempts_: {
        type: Number,
        value: 0,
      },
    };
  }

  delegate: CellularSetupDelegate;
  nameOfCarrierPendingSetup: string;
  forwardButtonLabel: string;
  private state_: PsimUiState;
  private selectedPsimPageName_: PsimPageName;
  private selectedPage_:
      SetupLoadingPageElement|ProvisioningPageElement|FinalPageElement;
  private showError_: boolean;
  private cellularMetadata_: CellularMetadata|null;
  private startActivationAttempts_: number;

  /**
   * Provides an interface to the CellularSetup Mojo service.
   */
  private cellularSetupRemote_: CellularSetupInterface|null = null;

  /**
   * Delegate responsible for routing activation started/finished events.
   */
  private activationDelegateReceiver_: ActivationDelegateReceiver|null = null;

  /**
   * The timeout ID corresponding to a timeout for the current state. If no
   * timeout is active, this value is null.
   */
  private currentTimeoutId_: number|null = null;

  /**
   * Handler used to communicate state updates back to the CellularSetup
   * service.
   */
  private carrierPortalHandler_: CarrierPortalHandlerRemote|null = null;

  /**
   * Whether there was a carrier portal error.
   */
  private didCarrierPortalResultFail_: boolean = false;

  /**
   * The function used to initiate a timer. Can be overwritten in tests.
   */
  private setTimeoutFunction_: Function = setTimeout.bind(window);

  /**
   * The time at which the PSim flow is attached.
   */
  private timeOnAttached_: Date|null = null;

  constructor() {
    super();

    this.cellularSetupRemote_ = getCellularSetupRemote();
  }

  override connectedCallback() {
    super.connectedCallback();

    this.timeOnAttached_ = new Date();
  }

  override disconnectedCallback() {
    super.disconnectedCallback();

    let resultCode = null;
    switch (this.state_) {
      case PsimUiState.IDLE:
      case PsimUiState.STARTING_ACTIVATION:
        resultCode = PsimSetupFlowResult.CANCELLED;
        break;
      case PsimUiState.WAITING_FOR_ACTIVATION_TO_START:
        resultCode = PsimSetupFlowResult.CANCELLED_COLD_SIM_DEFER;
        break;
      case PsimUiState.TIMEOUT_START_ACTIVATION:
      case PsimUiState.FINAL_TIMEOUT_START_ACTIVATION:
        resultCode = PsimSetupFlowResult.CANCELLED_NO_SIM;
        break;
      case PsimUiState.WAITING_FOR_PORTAL_TO_LOAD:
        resultCode = PsimSetupFlowResult.CANCELLED;
        break;
      case PsimUiState.TIMEOUT_PORTAL_LOAD:
        resultCode = PsimSetupFlowResult.CARRIER_PORTAL_TIMEOUT;
        break;
      case PsimUiState.WAITING_FOR_USER_PAYMENT:
        resultCode = PsimSetupFlowResult.CANCELLED_CARRIER_PORTAL;
        break;
      case PsimUiState.ACTIVATION_SUCCESS:
      case PsimUiState.WAITING_FOR_ACTIVATION_TO_FINISH:
      case PsimUiState.TIMEOUT_FINISH_ACTIVATION:
      case PsimUiState.ALREADY_ACTIVATED:
        resultCode = PsimSetupFlowResult.SUCCESS;
        break;
      case PsimUiState.ACTIVATION_FAILURE:
        resultCode = this.didCarrierPortalResultFail_ ?
            PsimSetupFlowResult.CANCELLED_PORTAL_ERROR :
            PsimSetupFlowResult.NETWORK_ERROR;
        break;
      default:
        assertNotReached();
    }

    assert(resultCode !== null);
    chrome.metricsPrivate.recordEnumerationValue(
        PSIM_SETUP_RESULT_METRIC_NAME, resultCode,
        Object.keys(PsimSetupFlowResult).length);

    const elapsedTimeMs = Date.now() - this.timeOnAttached_!.getTime();
    if (resultCode === PsimSetupFlowResult.SUCCESS) {
      chrome.metricsPrivate.recordLongTime(
          SUCCESSFUL_PSIM_SETUP_DURATION_METRIC_NAME, elapsedTimeMs);
      return;
    }

    chrome.metricsPrivate.recordLongTime(
        FAILED_PSIM_SETUP_DURATION_METRIC_NAME, elapsedTimeMs);
  }

  /**
   * Overrides ActivationDelegateInterface.
   */
  onActivationStarted(metadata: CellularMetadata): void {
    this.clearTimer_();
    this.cellularMetadata_ = metadata;
    this.state_ = PsimUiState.WAITING_FOR_PORTAL_TO_LOAD;
  }

  override initSubflow(): void {
    this.state_ = PsimUiState.STARTING_ACTIVATION;
    this.startActivationAttempts_ = 0;
    this.updateButtonBarState_();
    this.dispatchEvent(new CustomEvent(
      'focus-default-button', {bubbles: true, composed: true}));
  }

  override navigateForward(): void {
    switch (this.state_) {
      case PsimUiState.WAITING_FOR_PORTAL_TO_LOAD:
      case PsimUiState.TIMEOUT_PORTAL_LOAD:
      case PsimUiState.WAITING_FOR_USER_PAYMENT:
      case PsimUiState.ACTIVATION_SUCCESS:
        this.state_ = PsimUiState.WAITING_FOR_ACTIVATION_TO_FINISH;
        break;
      case PsimUiState.WAITING_FOR_ACTIVATION_TO_FINISH:
      case PsimUiState.TIMEOUT_FINISH_ACTIVATION:
      case PsimUiState.FINAL_TIMEOUT_START_ACTIVATION:
      case PsimUiState.ALREADY_ACTIVATED:
      case PsimUiState.ACTIVATION_FAILURE:
        this.dispatchEvent(new CustomEvent(
            'exit-cellular-setup', {bubbles: true, composed: true}));
        break;
      case PsimUiState.TIMEOUT_START_ACTIVATION:
        this.state_ = PsimUiState.STARTING_ACTIVATION;
        break;
      default:
        assertNotReached();
    }
  }

  /**
   * Sets the function used to initiate a timer.
   */
  setTimerFunctionForTest(timerFunction: Function): void {
    this.setTimeoutFunction_ = timerFunction;
  }

  getSelectedPsimPageNameForTest(): PsimPageName {
    return this.selectedPsimPageName_;
  }

  getCurrentTimeoutIdForTest(): number|null {
    return this.currentTimeoutId_;
  }

  setCurrentPsimUiStateForTest(state: PsimUiState): void {
    this.state_ = state;
  }

  getCurrentPsimUiStateForTest(): PsimUiState {
    return this.state_;
  }

  private updateButtonBarState_(): void {
    let buttonState;
    switch (this.state_) {
      case PsimUiState.IDLE:
      case PsimUiState.STARTING_ACTIVATION:
      case PsimUiState.WAITING_FOR_ACTIVATION_TO_START:
      case PsimUiState.WAITING_FOR_PORTAL_TO_LOAD:
      case PsimUiState.TIMEOUT_PORTAL_LOAD:
      case PsimUiState.WAITING_FOR_USER_PAYMENT:
        this.forwardButtonLabel = this.i18n('next');
        buttonState = {
          cancel: ButtonState.ENABLED,
          forward: ButtonState.DISABLED,
        };
        break;
      case PsimUiState.TIMEOUT_START_ACTIVATION:
        this.forwardButtonLabel = this.i18n('tryAgain');
        buttonState = {
          cancel: ButtonState.ENABLED,
          forward: ButtonState.ENABLED,
        };
        break;
      case PsimUiState.ACTIVATION_SUCCESS:
        this.forwardButtonLabel = this.i18n('next');
        buttonState = {
          cancel: ButtonState.ENABLED,
          forward: ButtonState.ENABLED,
        };
        break;
      case PsimUiState.ALREADY_ACTIVATED:
      case PsimUiState.ACTIVATION_FAILURE:
      case PsimUiState.FINAL_TIMEOUT_START_ACTIVATION:
        this.forwardButtonLabel = this.i18n('done');
        buttonState = {
          cancel: ButtonState.ENABLED,
          forward: ButtonState.ENABLED,
        };
        break;
      case PsimUiState.WAITING_FOR_ACTIVATION_TO_FINISH:
      case PsimUiState.TIMEOUT_FINISH_ACTIVATION:
        this.forwardButtonLabel = this.i18n('done');
        buttonState = {
          cancel: ButtonState.HIDDEN,
          forward: ButtonState.ENABLED,
        };
        break;
      default:
        assertNotReached();
    }
    this.set('buttonState', buttonState);
  }

  /**
   * Overrides ActivationDelegateInterface.
   */
  onActivationFinished(result: ActivationResult): void {
    this.closeActivationConnection_();

    switch (result) {
      case ActivationResult.kSuccessfullyStartedActivation:
        this.state_ = PsimUiState.ACTIVATION_SUCCESS;
        break;
      case ActivationResult.kAlreadyActivated:
        this.state_ = PsimUiState.ALREADY_ACTIVATED;
        break;
      case ActivationResult.kFailedToActivate:
        this.state_ = PsimUiState.ACTIVATION_FAILURE;
        break;
      default:
        assertNotReached();
    }
  }

  private getCarrierText(): string {
    if (this.selectedPsimPageName_ === PsimPageName.PROVISIONING &&
        this.cellularMetadata_) {
      return this.cellularMetadata_.carrier;
    }
    return '';
  }

  private updateShowError_(): void {
    switch (this.state_) {
      case PsimUiState.TIMEOUT_PORTAL_LOAD:
      case PsimUiState.TIMEOUT_FINISH_ACTIVATION:
      case PsimUiState.ACTIVATION_FAILURE:
        this.showError_ = true;
        return;
      default:
        this.showError_ = false;
        return;
    }
  }

  private updateSelectedPage_(): void {
    switch (this.state_) {
      case PsimUiState.IDLE:
      case PsimUiState.STARTING_ACTIVATION:
      case PsimUiState.WAITING_FOR_ACTIVATION_TO_START:
      case PsimUiState.TIMEOUT_START_ACTIVATION:
      case PsimUiState.FINAL_TIMEOUT_START_ACTIVATION:
        this.selectedPsimPageName_ = PsimPageName.SIM_DETECT;
        return;
      case PsimUiState.WAITING_FOR_PORTAL_TO_LOAD:
      case PsimUiState.TIMEOUT_PORTAL_LOAD:
      case PsimUiState.WAITING_FOR_USER_PAYMENT:
      case PsimUiState.ACTIVATION_SUCCESS:
        this.selectedPsimPageName_ = PsimPageName.PROVISIONING;
        return;
      case PsimUiState.WAITING_FOR_ACTIVATION_TO_FINISH:
      case PsimUiState.TIMEOUT_FINISH_ACTIVATION:
      case PsimUiState.ALREADY_ACTIVATED:
      case PsimUiState.ACTIVATION_FAILURE:
        this.selectedPsimPageName_ = PsimPageName.FINAL;
        return;
      default:
        assertNotReached();
    }
  }

  private handlePsimUiStateChange_(): void {
    this.updateShowError_();
    this.updateSelectedPage_();

    // Since the state has changed, the previous state did not time out, so
    // clear any active timeout.
    this.clearTimer_();

    // If the new state has an associated timeout, set it.
    const timeoutMs = getTimeoutMsForPsimUiState(this.state_);
    if (timeoutMs !== null) {
      this.currentTimeoutId_ =
          this.setTimeoutFunction_(this.onTimeout_.bind(this), timeoutMs);
    }

    if (this.state_ === PsimUiState.STARTING_ACTIVATION) {
      this.startActivation_();
    }

    this.updateButtonBarState_();
  }

  private onTimeout_(): void {
    // The activation attempt failed, so close the connection to the service.
    this.closeActivationConnection_();

    switch (this.state_) {
      case PsimUiState.STARTING_ACTIVATION:
        this.startActivationAttempts_++;
        if (this.startActivationAttempts_ < MAX_START_ACTIVATION_ATTEMPTS) {
          this.state_ = PsimUiState.TIMEOUT_START_ACTIVATION;
        } else {
          this.state_ = PsimUiState.FINAL_TIMEOUT_START_ACTIVATION;
        }
        return;
      case PsimUiState.WAITING_FOR_PORTAL_TO_LOAD:
        this.state_ = PsimUiState.TIMEOUT_PORTAL_LOAD;
        return;
      case PsimUiState.WAITING_FOR_ACTIVATION_TO_FINISH:
        this.state_ = PsimUiState.TIMEOUT_FINISH_ACTIVATION;
        return;
      default:
        // Only the above states are expected to time out.
        assertNotReached();
    }
  }

  private startActivation_() {
    assert(!this.activationDelegateReceiver_);
    this.activationDelegateReceiver_ = new ActivationDelegateReceiver(
        (this));

    this.cellularSetupRemote_!
        .startActivation(
            this.activationDelegateReceiver_.$.bindNewPipeAndPassRemote())
        .then(
            (params) => {
              this.carrierPortalHandler_ = params.observer;
            });
  }

  private closeActivationConnection_(): void {
    assert(!!this.activationDelegateReceiver_);
    this.activationDelegateReceiver_.$.close();
    this.activationDelegateReceiver_ = null;
    this.carrierPortalHandler_ = null;
    this.cellularMetadata_ = null;
  }

  private clearTimer_(): void {
    if (this.currentTimeoutId_) {
      clearTimeout(this.currentTimeoutId_);
    }
    this.currentTimeoutId_ = null;
  }

  private onCarrierPortalLoaded_(): void {
    this.state_ = PsimUiState.WAITING_FOR_USER_PAYMENT;
    this.carrierPortalHandler_!.onCarrierPortalStatusChange(
        CarrierPortalStatus.kPortalLoadedWithoutPaidUser);
  }

  private onCarrierPortalResult_(event: CustomEvent<boolean>): void {
    const success = event.detail;
    this.didCarrierPortalResultFail_ = !success;
    this.state_ = success ? PsimUiState.ACTIVATION_SUCCESS :
                            PsimUiState.ACTIVATION_FAILURE;
  }

  private getLoadingMessage_(): string {
    if (this.state_ === PsimUiState.TIMEOUT_START_ACTIVATION) {
      return this.i18n('simDetectPageErrorMessage');
    } else if (this.state_ === PsimUiState.FINAL_TIMEOUT_START_ACTIVATION) {
      return this.i18n('simDetectPageFinalErrorMessage');
    }
    return this.i18n('establishNetworkConnectionMessage');
  }

  private isSimDetectError_(): boolean {
    return this.state_ === PsimUiState.TIMEOUT_START_ACTIVATION ||
        this.state_ === PsimUiState.FINAL_TIMEOUT_START_ACTIVATION;
  }

  private getLoadingTitle_(): string {
    if (this.delegate.shouldShowPageTitle() && this.isSimDetectError_()) {
      return this.i18n('simDetectPageErrorTitle');
    }
    return '';
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [PsimFlowUiElement.is]: PsimFlowUiElement;
  }
}

customElements.define(PsimFlowUiElement.is, PsimFlowUiElement);
