// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/polymer/v3_0/iron-pages/iron-pages.js';
import './setup_loading_page.js';
import './activation_code_page.js';
import './activation_verification_page.js';
import './final_page.js';
import './profile_discovery_consent_page.js';
import './profile_discovery_list_page_legacy.js';
import './profile_discovery_list_page.js';
import './confirmation_code_page_legacy.js';
import './confirmation_code_page.js';

import {assert, assertNotReached} from 'chrome://resources/js/assert.js';
import {hasActiveCellularNetwork} from 'chrome://resources/ash/common/network/cellular_utils.js';
import {MojoInterfaceProviderImpl} from 'chrome://resources/ash/common/network/mojo_interface_provider.js';
import {NetworkListenerBehavior} from 'chrome://resources/ash/common/network/network_listener_behavior.js';
import {I18nMixin} from 'chrome://resources/cr_elements/i18n_mixin.js';
import {mixinBehaviors, PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {ESimManagerRemote, ESimOperationResult, ESimProfileProperties, ESimProfileRemote, EuiccRemote, ProfileInstallMethod, ProfileInstallResult, ProfileState} from 'chrome://resources/mojo/chromeos/ash/services/cellular_setup/public/mojom/esim_manager.mojom-webui.js';
import {FilterType, NetworkStateProperties, NO_LIMIT} from 'chrome://resources/mojo/chromeos/services/network_config/public/mojom/cros_network_config.mojom-webui.js';
import {ConnectionStateType, NetworkType} from 'chrome://resources/mojo/chromeos/services/network_config/public/mojom/network_types.mojom-webui.js';

import {CellularSetupDelegate} from './cellular_setup_delegate.js';
import {ButtonBarState, ButtonState} from './cellular_types.js';
import {getTemplate} from './esim_flow_ui.html.js';
import {getEuicc, getPendingESimProfiles} from './esim_manager_utils.js';
import {getESimManagerRemote} from './mojo_interface_provider.js';
import {SubflowMixin} from './subflow_mixin.js';

// eslint-disable-next-line @typescript-eslint/naming-convention
export enum ESimPageName {
  PROFILE_LOADING = 'profileLoadingPage',
  PROFILE_DISCOVERY_CONSENT = 'profileDiscoveryConsentPage',
  PROFILE_DISCOVERY = 'profileDiscoveryPage',
  PROFILE_DISCOVERY_LEGACY = 'profileDiscoveryPageLegacy',
  ACTIVATION_CODE = 'activationCodePage',
  CONFIRMATION_CODE = 'confirmationCodePage',
  CONFIRMATION_CODE_LEGACY = 'confirmationCodePageLegacy',
  PROFILE_INSTALLING = 'profileInstallingPage',
  FINAL = 'finalPage',
}

// eslint-disable-next-line @typescript-eslint/naming-convention
export enum ESimUiState {
  PROFILE_SEARCH = 'profile-search',
  PROFILE_SEARCH_CONSENT = 'profile-search-consent',
  ACTIVATION_CODE_ENTRY = 'activation-code-entry',
  ACTIVATION_CODE_ENTRY_READY = 'activation-code-entry-ready',
  ACTIVATION_CODE_ENTRY_INSTALLING = 'activation-code-entry-installing',
  CONFIRMATION_CODE_ENTRY = 'confirmation-code-entry',
  CONFIRMATION_CODE_ENTRY_READY = 'confirmation-code-entry-ready',
  CONFIRMATION_CODE_ENTRY_INSTALLING = 'confirmation-code-entry-installing',
  PROFILE_SELECTION = 'profile-selection',
  PROFILE_SELECTION_INSTALLING = 'profile-selection-installing',
  SETUP_FINISH = 'setup-finish',
}

// The reason that caused the user to exit the ESim Setup flow.
// These values are persisted to logs. Entries should not be renumbered
// and numeric values should never be reused.
// eslint-disable-next-line @typescript-eslint/naming-convention
export enum ESimSetupFlowResult {
  SUCCESS = 0,
  INSTALL_FAIL = 1,
  CANCELLED_NEEDS_CONFIRMATION_CODE = 2,
  CANCELLED_INVALID_ACTIVATION_CODE = 3,
  ERROR_FETCHING_PROFILES = 4,
  CANCELLED_WITHOUT_ERROR = 5,
  CANCELLED_NO_PROFILES = 6,
  NO_NETWORK = 7,
}

export const ESIM_SETUP_RESULT_METRIC_NAME =
    'Network.Cellular.ESim.SetupFlowResult';

export const SUCCESSFUL_ESIM_SETUP_DURATION_METRIC_NAME =
    'Network.Cellular.ESim.CellularSetup.Success.Duration';

export const FAILED_ESIM_SETUP_DURATION_METRIC_NAME =
    'Network.Cellular.ESim.CellularSetup.Failure.Duration';

declare global {
  interface HTMLElementEventMap {
    'activation-code-updated':
        CustomEvent<{activationCode: string|null}>;
  }
}

/**
 * Root element for the eSIM cellular setup flow. This element interacts with
 * the CellularSetup service to carry out the esim activation flow.
 */
const EsimFlowUiElementBase =
    mixinBehaviors([NetworkListenerBehavior],
        SubflowMixin(I18nMixin(PolymerElement)));

export class EsimFlowUiElement extends EsimFlowUiElementBase {
  static get is() {
    return 'esim-flow-ui' as const;
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      delegate: Object,

      /**
       * Header shown at the top of the flow. No header shown if the string is
       * empty.
       */
      header: {
        type: String,
        notify: true,
        computed: 'computeHeader_(selectedESimPageName_, showError_)',
      },

      forwardButtonLabel: {
        type: String,
        notify: true,
      },

      state_: {
        type: String,
        value: function() {
          if (loadTimeData.valueExists('isSmdsSupportEnabled') &&
              loadTimeData.getBoolean('isSmdsSupportEnabled')) {
            return ESimUiState.PROFILE_SEARCH_CONSENT;
          }
          return ESimUiState.PROFILE_SEARCH;
        },
        observer: 'onStateChanged_',
      },

      /**
       * Element name of the current selected sub-page.
       * This is set in updateSelectedPage_ on initialization.
       */
      selectedESimPageName_: String,

      /**
       * Whether the user has consented to a scan for profiles.
       */
      hasConsentedForDiscovery_: {
        type: Boolean,
        value: false,
      },

      /**
       * Whether the user is setting up the eSIM profile manually.
       */
      shouldSkipDiscovery_: {
        type: Boolean,
        value: false,
      },

      /**
       * Whether error state should be shown for the current page.
       */
      showError_: {
        type: Boolean,
        value: false,
      },

      /**
       * Profiles fetched that have status kPending.
       */
      pendingProfiles_: Array,

      /**
       * Profile selected to be installed.
       */
      selectedProfile_: {
        type: Object,
        observer: 'onSelectedProfileChanged_',
      },

      /**
       * Profile properties fetched from the latest SM-DS scan.
       */
      pendingProfileProperties_: Array,

      /**
       * Profile properties selected to be installed.
       */
      selectedProfileProperties_: {
        type: Object,
        observer: 'onSelectedProfilePropertiesChanged_',
      },

      activationCode_: {
        type: String,
        value: '',
      },

      confirmationCode_: {
        type: String,
        value: '',
        observer: 'onConfirmationCodeUpdated_',
      },

      hasHadActiveCellularNetwork_: {
        type: Boolean,
        value: false,
      },

      isActivationCodeFromQrCode_: Boolean,

      /**
       * Return true if SmdsSupportEnabled feature flag is enabled.
       */
      smdsSupportEnabled_: {
        type: Boolean,
        value() {
          return loadTimeData.valueExists('isSmdsSupportEnabled') &&
              loadTimeData.getBoolean('isSmdsSupportEnabled');
        },
      },

    };
  }

  delegate: CellularSetupDelegate;
  header: string;
  forwardButtonLabel: string;
  private state_: string;
  private selectedESimPageName_: string;
  private hasConsentedForDiscovery_: boolean;
  private shouldSkipDiscovery_: boolean;
  private showError_: boolean;
  private pendingProfiles_: ESimProfileRemote[];
  private selectedProfile_: ESimProfileRemote|null;
  private pendingProfileProperties_: ESimProfileProperties[];
  private selectedProfileProperties_: ESimProfileProperties|null;
  private activationCode_: string;
  private confirmationCode_: string;
  private hasHadActiveCellularNetwork_: boolean;
  private isActivationCodeFromQrCode_: boolean;
  private smdsSupportEnabled_: boolean;

  /**
   * Provides an interface to the ESimManager Mojo service.
   */
  private eSimManagerRemote_: ESimManagerRemote;
  private euicc_: EuiccRemote|null = null;
  private lastProfileInstallResult_: ProfileInstallResult|null = null;
  private hasFailedFetchingProfiles_: boolean = false;

  /**
   * If there are no active network connections of any type.
   */
  private isOffline_: boolean = false;

  /**
   * The time at which the ESim flow is attached.
   */
  private timeOnAttached_: Date|null = null;

  constructor() {
    super();

    this.eSimManagerRemote_ = getESimManagerRemote();
    const networkConfig =
        MojoInterfaceProviderImpl.getInstance().getMojoServiceRemote();

    const filter = {
      filter: FilterType.kActive,
      limit: NO_LIMIT,
      networkType: NetworkType.kAll,
    };
    networkConfig.getNetworkStateList(filter).then((response) => {
      this.onActiveNetworksChanged(response.result);
    });
  }

  override connectedCallback() {
    super.connectedCallback();

    this.timeOnAttached_ = new Date();
  }

  override disconnectedCallback() {
    super.disconnectedCallback();

    let resultCode = null;

    switch (this.lastProfileInstallResult_) {
      case null:
        // Handles case when no profile installation was attempted.
        if (this.hasFailedFetchingProfiles_) {
          resultCode = ESimSetupFlowResult.ERROR_FETCHING_PROFILES;
        } else if (this.noProfilesFound_()) {
          resultCode = ESimSetupFlowResult.CANCELLED_NO_PROFILES;
        } else {
          resultCode = ESimSetupFlowResult.CANCELLED_WITHOUT_ERROR;
        }
        break;
      case ProfileInstallResult.kSuccess:
        resultCode = ESimSetupFlowResult.SUCCESS;
        break;
      case ProfileInstallResult.kFailure:
        resultCode = ESimSetupFlowResult.INSTALL_FAIL;
        break;
      case ProfileInstallResult.kErrorNeedsConfirmationCode:
        resultCode = ESimSetupFlowResult.CANCELLED_NEEDS_CONFIRMATION_CODE;
        break;
      case ProfileInstallResult.kErrorInvalidActivationCode:
        resultCode = ESimSetupFlowResult.CANCELLED_INVALID_ACTIVATION_CODE;
        break;
      default:
        break;
    }

    if (this.isOffline_ && resultCode !== ProfileInstallResult.kSuccess) {
      resultCode = ESimSetupFlowResult.NO_NETWORK;
    }

    assert(resultCode !== null);
    chrome.metricsPrivate.recordEnumerationValue(
        ESIM_SETUP_RESULT_METRIC_NAME, resultCode,
        Object.keys(ESimSetupFlowResult).length);

    const elapsedTimeMs = new Date().getTime() - this.timeOnAttached_!.getTime();
    if (resultCode === ESimSetupFlowResult.SUCCESS) {
      chrome.metricsPrivate.recordLongTime(
          SUCCESSFUL_ESIM_SETUP_DURATION_METRIC_NAME, elapsedTimeMs);
      return;
    }

    chrome.metricsPrivate.recordLongTime(
        FAILED_ESIM_SETUP_DURATION_METRIC_NAME, elapsedTimeMs);
  }

  override ready() {
    super.ready();

    this.addEventListener('activation-code-updated',
        (event: CustomEvent<{activationCode: string|null}>) => {
          this.onActivationCodeUpdated_(event);
        });
    this.addEventListener('forward-navigation-requested',
        this.onForwardNavigationRequested_);
  }

  /**
   * NetworkListenerBehavior override
   * Used to determine if there is an online network connection.
   */
  onActiveNetworksChanged(activeNetworks: NetworkStateProperties[]): void {
    this.isOffline_ = !activeNetworks.some(
        (network) => network.connectionState === ConnectionStateType.kOnline);
  }

  override initSubflow(): void {
    if (!this.smdsSupportEnabled_) {
      this.fetchProfiles_();
    } else {
      this.getEuicc_();
    }
    this.onNetworkStateListChanged();
  }

  private async fetchProfiles_(): Promise<void> {
    await this.getEuicc_();
    if (!this.euicc_) {
      return;
    }

    if (this.smdsSupportEnabled_) {
      await this.getAvailableProfileProperties_();
    } else {
      await this.getPendingProfiles_();
    }
    if (this.noProfilesFound_()) {
      this.state_ = ESimUiState.ACTIVATION_CODE_ENTRY;
    } else {
      this.state_ = ESimUiState.PROFILE_SELECTION;
    }
  }

  private async getEuicc_(): Promise<void> {
    const euicc = await getEuicc();
    if (!euicc) {
      this.hasFailedFetchingProfiles_ = true;
      this.showError_ = true;
      this.state_ = ESimUiState.SETUP_FINISH;
      console.warn('No Euiccs found');
      return;
    }
    this.euicc_ = euicc;
  }

  private async getAvailableProfileProperties_(): Promise<void> {
    assert(this.euicc_);
    const requestAvailableProfilesResponse =
        await this.euicc_.requestAvailableProfiles();
    if (requestAvailableProfilesResponse.result ===
        ESimOperationResult.kFailure) {
      this.hasFailedFetchingProfiles_ = true;
      console.warn(
          'Error requesting available profiles: ',
          requestAvailableProfilesResponse);
      this.pendingProfileProperties_ = [];
    }
    this.pendingProfileProperties_ =
        requestAvailableProfilesResponse.profiles.filter((properties) => {
          return properties.state === ProfileState.kPending &&
              properties.activationCode;
        });
  }

  private async getPendingProfiles_(): Promise<void> {
    assert(this.euicc_);
    const requestPendingProfilesResponse =
        await this.euicc_.requestPendingProfiles();
    if (requestPendingProfilesResponse.result ===
        ESimOperationResult.kFailure) {
      this.hasFailedFetchingProfiles_ = true;
      console.warn(
          'Error requesting pending profiles: ',
          requestPendingProfilesResponse);
      this.pendingProfiles_ = [];
    }
    this.pendingProfiles_ = await getPendingESimProfiles(this.euicc_);
  }

  private handleProfileInstallResponse_(
      response: {result: ProfileInstallResult}): void {
    this.lastProfileInstallResult_ = response.result;
    if (response.result === ProfileInstallResult.kErrorNeedsConfirmationCode) {
      this.state_ = ESimUiState.CONFIRMATION_CODE_ENTRY;
      return;
    }
    this.showError_ = response.result !== ProfileInstallResult.kSuccess;
    if (response.result === ProfileInstallResult.kFailure &&
        this.state_ === ESimUiState.CONFIRMATION_CODE_ENTRY_INSTALLING) {
      this.state_ = ESimUiState.CONFIRMATION_CODE_ENTRY_READY;
      return;
    }
    if (response.result === ProfileInstallResult.kErrorInvalidActivationCode &&
        (!this.smdsSupportEnabled_ ||
         this.state_ !== ESimUiState.PROFILE_SELECTION_INSTALLING)) {
      this.state_ = ESimUiState.ACTIVATION_CODE_ENTRY_READY;
      return;
    }
    if (response.result === ProfileInstallResult.kSuccess ||
        response.result === ProfileInstallResult.kFailure) {
      this.state_ = ESimUiState.SETUP_FINISH;
    }
  }

  private onStateChanged_(newState: ESimUiState, oldState: ESimUiState): void {
    this.updateButtonBarState_();
    this.updateSelectedPage_();
    if (this.hasConsentedForDiscovery_ &&
        newState === ESimUiState.PROFILE_SEARCH) {
      this.fetchProfiles_();
    }
    this.initializePageState_(newState, oldState);
  }

  private updateSelectedPage_(): void {
    const oldSelectedESimPageName = this.selectedESimPageName_;
    switch (this.state_) {
      case ESimUiState.PROFILE_SEARCH:
        this.selectedESimPageName_ = ESimPageName.PROFILE_LOADING;
        break;
      case ESimUiState.PROFILE_SEARCH_CONSENT:
        this.selectedESimPageName_= ESimPageName.PROFILE_DISCOVERY_CONSENT;
        break;
      case ESimUiState.ACTIVATION_CODE_ENTRY:
      case ESimUiState.ACTIVATION_CODE_ENTRY_READY:
        this.selectedESimPageName_ = ESimPageName.ACTIVATION_CODE;
        break;
      case ESimUiState.ACTIVATION_CODE_ENTRY_INSTALLING:
        this.selectedESimPageName_ = ESimPageName.PROFILE_INSTALLING;
        break;
      case ESimUiState.CONFIRMATION_CODE_ENTRY:
      case ESimUiState.CONFIRMATION_CODE_ENTRY_READY:
        if (this.smdsSupportEnabled_) {
          this.selectedESimPageName_ = ESimPageName.CONFIRMATION_CODE;
        } else {
          this.selectedESimPageName_ = ESimPageName.CONFIRMATION_CODE_LEGACY;
        }
        break;
      case ESimUiState.CONFIRMATION_CODE_ENTRY_INSTALLING:
        if (this.smdsSupportEnabled_) {
          this.selectedESimPageName_ = ESimPageName.PROFILE_INSTALLING;
        } else {
          this.selectedESimPageName_ = ESimPageName.CONFIRMATION_CODE_LEGACY;
        }
        break;
      case ESimUiState.PROFILE_SELECTION:
        if (this.smdsSupportEnabled_) {
          this.selectedESimPageName_ = ESimPageName.PROFILE_DISCOVERY;
        } else {
          this.selectedESimPageName_ = ESimPageName.PROFILE_DISCOVERY_LEGACY;
        }
        break;
      case ESimUiState.PROFILE_SELECTION_INSTALLING:
        if (this.smdsSupportEnabled_) {
          this.selectedESimPageName_ = ESimPageName.PROFILE_INSTALLING;
        } else {
          this.selectedESimPageName_ = ESimPageName.PROFILE_DISCOVERY_LEGACY;
        }
        break;
      case ESimUiState.SETUP_FINISH:
        this.selectedESimPageName_ = ESimPageName.FINAL;
        break;
      default:
        assertNotReached();
    }
    // If there is a page change, fire focus event.
    if (oldSelectedESimPageName !== this.selectedESimPageName_) {
      this.dispatchEvent(new CustomEvent('focus-default-button', {
        bubbles: true, composed: true,
      }));
    }
  }

  private generateButtonStateForActivationPage_(
      enableForwardBtn: boolean, cancelButtonStateIfEnabled: ButtonState,
      isInstalling: boolean): ButtonBarState {
    this.forwardButtonLabel = this.i18n('next');
    let backBtnState = ButtonState.HIDDEN;
    if (this.profilesFound_() && !this.smdsSupportEnabled_) {
      backBtnState = isInstalling ? ButtonState.DISABLED : ButtonState.ENABLED;
    }
    return {
      backward: backBtnState,
      cancel: cancelButtonStateIfEnabled,
      forward: enableForwardBtn ? ButtonState.ENABLED : ButtonState.DISABLED,
    };
  }

  private generateButtonStateForConfirmationPage_(
      enableForwardBtn: boolean, cancelButtonStateIfEnabled: ButtonState,
      isInstalling: boolean): ButtonBarState {
    this.forwardButtonLabel = this.i18n('confirm');
    let backBtnState = isInstalling ?
        ButtonState.DISABLED : ButtonState.ENABLED;
    if (this.smdsSupportEnabled_) {
      backBtnState = ButtonState.HIDDEN;
    }
    return {
      backward: backBtnState,
      cancel: cancelButtonStateIfEnabled,
      forward: enableForwardBtn ? ButtonState.ENABLED : ButtonState.DISABLED,
    };
  }

  private updateButtonBarState_(): void {
    let buttonState;
    const cancelButtonStateIfEnabled = this.delegate.shouldShowCancelButton() ?
        ButtonState.ENABLED :
        ButtonState.HIDDEN;
    const cancelButtonStateIfDisabled = this.delegate.shouldShowCancelButton() ?
        ButtonState.DISABLED :
        ButtonState.HIDDEN;
    switch (this.state_) {
      case ESimUiState.PROFILE_SEARCH:
        this.forwardButtonLabel = this.i18n('next');
        buttonState = {
          backward: ButtonState.HIDDEN,
          cancel: cancelButtonStateIfEnabled,
          forward: ButtonState.DISABLED,
        };
        break;
      case ESimUiState.PROFILE_SEARCH_CONSENT:
        this.forwardButtonLabel = this.i18n('profileDiscoveryConsentScan');
        buttonState = {
          backward: ButtonState.HIDDEN,
          cancel: ButtonState.ENABLED,
          forward: ButtonState.ENABLED,
        };
        break;
      case ESimUiState.ACTIVATION_CODE_ENTRY:
        buttonState = this.generateButtonStateForActivationPage_(
            /*enableForwardBtn*/ false, cancelButtonStateIfEnabled,
            /*isInstalling*/ false);
        break;
      case ESimUiState.ACTIVATION_CODE_ENTRY_READY:
        buttonState = this.generateButtonStateForActivationPage_(
            /*enableForwardBtn*/ true, cancelButtonStateIfEnabled,
            /*isInstalling*/ false);
        break;
      case ESimUiState.ACTIVATION_CODE_ENTRY_INSTALLING:
        buttonState = this.generateButtonStateForActivationPage_(
            /*enableForwardBtn*/ false, cancelButtonStateIfDisabled,
            /*isInstalling*/ true);
        break;
      case ESimUiState.CONFIRMATION_CODE_ENTRY:
        buttonState = this.generateButtonStateForConfirmationPage_(
            /*enableForwardBtn*/ false, cancelButtonStateIfEnabled,
            /*isInstalling*/ false);
        break;
      case ESimUiState.CONFIRMATION_CODE_ENTRY_READY:
        buttonState = this.generateButtonStateForConfirmationPage_(
            /*enableForwardBtn*/ true, cancelButtonStateIfEnabled,
            /*isInstalling*/ false);
        break;
      case ESimUiState.CONFIRMATION_CODE_ENTRY_INSTALLING:
        buttonState = this.generateButtonStateForConfirmationPage_(
            /*enableForwardBtn*/ false, cancelButtonStateIfDisabled,
            /*isInstalling*/ true);
        break;
      case ESimUiState.PROFILE_SELECTION:
        this.updateForwardButtonLabel_();
        buttonState = {
          backward: ButtonState.HIDDEN,
          cancel: cancelButtonStateIfEnabled,
          forward: ButtonState.ENABLED,
        };
        break;
      case ESimUiState.PROFILE_SELECTION_INSTALLING:
        buttonState = {
          backward: ButtonState.HIDDEN,
          cancel: cancelButtonStateIfDisabled,
          forward: ButtonState.DISABLED,
        };
        break;
      case ESimUiState.SETUP_FINISH:
        this.forwardButtonLabel = this.i18n('done');
        buttonState = {
          backward: ButtonState.HIDDEN,
          cancel: ButtonState.HIDDEN,
          forward: ButtonState.ENABLED,
        };
        break;
      default:
        assertNotReached();
    }
    this.set('buttonState', buttonState);
  }

  private updateForwardButtonLabel_(): void {
    if (this.smdsSupportEnabled_) {
      this.forwardButtonLabel = this.selectedProfileProperties_ ?
          this.i18n('next') :
          this.i18n('skipDiscovery');
    } else {
      this.forwardButtonLabel = this.selectedProfile_ ?
          this.i18n('next') :
          this.i18n('skipDiscovery');
    }
  }

  private initializePageState_(newState: ESimUiState,
      oldState: ESimUiState): void {
    if (newState === ESimUiState.CONFIRMATION_CODE_ENTRY &&
        oldState !== ESimUiState.CONFIRMATION_CODE_ENTRY_READY) {
      this.confirmationCode_ = '';
    }
    if (newState === ESimUiState.ACTIVATION_CODE_ENTRY &&
        oldState !== ESimUiState.ACTIVATION_CODE_ENTRY_READY) {
      this.activationCode_ = '';
    }
  }

  private onActivationCodeUpdated_(event: CustomEvent<{activationCode: string|null}>): void {
    // initializePageState_() may cause this observer to fire and update the
    // buttonState when we're not on the activation code page. Check we're on
    // the activation code page before proceeding.
    if (this.state_ !== ESimUiState.ACTIVATION_CODE_ENTRY &&
        this.state_ !== ESimUiState.ACTIVATION_CODE_ENTRY_READY) {
      return;
    }
    this.state_ = event.detail.activationCode ?
        ESimUiState.ACTIVATION_CODE_ENTRY_READY :
        ESimUiState.ACTIVATION_CODE_ENTRY;
  }

  private onSelectedProfileChanged_(): void {
    // initializePageState_() may cause this observer to fire and update the
    // buttonState when we're not on the profile selection page. Check we're
    // on the profile selection page before proceeding.
    if (this.state_ !== ESimUiState.PROFILE_SELECTION) {
      return;
    }
    if (this.smdsSupportEnabled_) {
      return;
    }
    this.updateForwardButtonLabel_();
  }

  private onSelectedProfilePropertiesChanged_(): void {
    // initializePageState_() may cause this observer to fire and update the
    // buttonState when we're not on the profile selection page. Check we're
    // on the profile selection page before proceeding.
    if (this.state_ !== ESimUiState.PROFILE_SELECTION) {
      return;
    }
    if (!this.smdsSupportEnabled_) {
      return;
    }
    this.updateForwardButtonLabel_();
  }

  private onConfirmationCodeUpdated_(): void {
    // initializePageState_() may cause this observer to fire and update the
    // buttonState when we're not on the confirmation code page. Check we're
    // on the confirmation code page before proceeding.
    if (this.state_ !== ESimUiState.CONFIRMATION_CODE_ENTRY &&
        this.state_ !== ESimUiState.CONFIRMATION_CODE_ENTRY_READY) {
      return;
    }
    this.state_ = this.confirmationCode_ ?
        ESimUiState.CONFIRMATION_CODE_ENTRY_READY :
        ESimUiState.CONFIRMATION_CODE_ENTRY;
  }

  /** SubflowMixin override */
  override navigateForward(): void {
    this.showError_ = false;
    switch (this.state_) {
      case ESimUiState.PROFILE_SEARCH_CONSENT:
        if (this.shouldSkipDiscovery_) {
          this.state_ = ESimUiState.ACTIVATION_CODE_ENTRY;
          break;
        }
        // Set |this.hasConsentedForDiscovery_| to |true| since navigating
        // forward and not setting up manually is explicitly giving consent
        // to perform SM-DS scans.
        this.hasConsentedForDiscovery_= true;
        this.state_ = ESimUiState.PROFILE_SEARCH;
        break;
      case ESimUiState.ACTIVATION_CODE_ENTRY_READY:
        assert(this.euicc_);
        // Assume installing the profile doesn't require a confirmation
        // code.
        const confirmationCode = '';
        this.state_ = ESimUiState.ACTIVATION_CODE_ENTRY_INSTALLING;
        this.euicc_
            .installProfileFromActivationCode(
                this.activationCode_, confirmationCode,
                this.computeProfileInstallMethod_())
            .then(this.handleProfileInstallResponse_.bind(this));
        break;
      case ESimUiState.PROFILE_SELECTION:
        if (this.smdsSupportEnabled_) {
          if (this.selectedProfileProperties_) {
            assert(this.euicc_);
            this.state_ = ESimUiState.PROFILE_SELECTION_INSTALLING;
            // Assume installing the profile doesn't require a confirmation
            // code.
            const confirmationCode = '';
            this.euicc_
                .installProfileFromActivationCode(
                    this.selectedProfileProperties_.activationCode,
                    confirmationCode, ProfileInstallMethod.kViaSmds)
                .then(this.handleProfileInstallResponse_.bind(this));
          } else {
            this.state_ = ESimUiState.ACTIVATION_CODE_ENTRY;
          }
        } else {
          if (this.selectedProfile_) {
            this.state_ = ESimUiState.PROFILE_SELECTION_INSTALLING;
            // Assume installing the profile doesn't require a confirmation
            // code, send an empty string.
            this.selectedProfile_.installProfile('').then(
                this.handleProfileInstallResponse_.bind(this));
          } else {
            this.state_ = ESimUiState.ACTIVATION_CODE_ENTRY;
          }
        }
        break;
      case ESimUiState.CONFIRMATION_CODE_ENTRY_READY:
        this.state_ = ESimUiState.CONFIRMATION_CODE_ENTRY_INSTALLING;
        if (this.smdsSupportEnabled_) {
          assert(this.euicc_);
          const fromQrCode = this.selectedProfileProperties_ ? true : false;
          const activationCode = fromQrCode ?
              this.selectedProfileProperties_!.activationCode :
              this.activationCode_;
          this.euicc_
              .installProfileFromActivationCode(
                  activationCode, this.confirmationCode_,
                  this.computeProfileInstallMethod_())
              .then(this.handleProfileInstallResponse_.bind(this));
        } else {
          if (this.selectedProfile_) {
            this.selectedProfile_.installProfile(this.confirmationCode_)
                .then(this.handleProfileInstallResponse_.bind(this));
          } else {
            assert(this.euicc_);
            this.euicc_
                .installProfileFromActivationCode(
                    this.activationCode_, this.confirmationCode_,
                    this.computeProfileInstallMethod_())
                .then(this.handleProfileInstallResponse_.bind(this));
          }
        }
        break;
      case ESimUiState.SETUP_FINISH:
        this.dispatchEvent(new CustomEvent('exit-cellular-setup', {
          bubbles: true, composed: true,
        }));
        break;
      default:
        assertNotReached();
    }
  }

  /** SubflowMixin override */
  override navigateBackward(): void {
    if (this.profilesFound_() &&
        (this.state_ === ESimUiState.ACTIVATION_CODE_ENTRY ||
         this.state_ === ESimUiState.ACTIVATION_CODE_ENTRY_READY)) {
      this.state_ = ESimUiState.PROFILE_SELECTION;
      return;
    }

    if (this.state_ === ESimUiState.CONFIRMATION_CODE_ENTRY ||
        this.state_ === ESimUiState.CONFIRMATION_CODE_ENTRY_READY) {
      if (this.activationCode_) {
        this.state_ = ESimUiState.ACTIVATION_CODE_ENTRY_READY;
        return;
      } else if (this.profilesFound_()) {
        this.state_ = ESimUiState.PROFILE_SELECTION;
        return;
      }
    }
    console.error(
        'Navigate backward faled for : ' + this.state_ +
        ' this state does not support backward navigation.');
    assertNotReached();
  }

  private onForwardNavigationRequested_(): void {
    if (this.state_ === ESimUiState.ACTIVATION_CODE_ENTRY_READY ||
        this.state_ === ESimUiState.CONFIRMATION_CODE_ENTRY_READY ||
        this.state_ === ESimUiState.PROFILE_SEARCH_CONSENT ||
        this.state_ === ESimUiState.PROFILE_SELECTION) {
      this.navigateForward();
    }
  }

  /** NetworkListenerBehavior override */
  async onNetworkStateListChanged(): Promise<void> {
    const hasActive = await hasActiveCellularNetwork();
    // If hasHadActiveCellularNetwork_ has been set to true, don't set to
    // false again as we should show the cellular disconnect warning for the
    // duration of the flow's lifecycle.
    if (hasActive) {
      this.hasHadActiveCellularNetwork_ = hasActive;
    }
  }

  private shouldShowSubpageBusy_(): boolean {
    return this.state_ === ESimUiState.ACTIVATION_CODE_ENTRY_INSTALLING ||
        this.state_ === ESimUiState.CONFIRMATION_CODE_ENTRY_INSTALLING ||
        this.state_ === ESimUiState.PROFILE_SELECTION_INSTALLING;
  }

  private getLoadingMessage_(): string {
    if (this.smdsSupportEnabled_) {
      return this.i18n('profileLoadingPageMessage');
    }

    return this.hasHadActiveCellularNetwork_ ?
        this.i18n('eSimProfileDetectDuringActiveCellularConnectionMessage') :
        this.i18n('eSimProfileDetectMessage');
  }

  private computeHeader_(): string {
    if (this.selectedESimPageName_ === ESimPageName.FINAL && !this.showError_) {
      return this.i18n('eSimFinalPageSuccessHeader');
    }

    if (this.selectedESimPageName_ === ESimPageName.PROFILE_DISCOVERY_CONSENT) {
      return this.i18n('profileDiscoveryConsentTitle');
    }

    if (this.smdsSupportEnabled_) {
      if (this.selectedESimPageName_ === ESimPageName.PROFILE_DISCOVERY) {
        return this.i18n('profileDiscoveryPageTitle');
      }

      if (this.selectedESimPageName_ == ESimPageName.CONFIRMATION_CODE) {
        return this.i18n('confimationCodePageTitle');
      }
      if (this.selectedESimPageName_ == ESimPageName.PROFILE_LOADING) {
        return this.i18n('profileLoadingPageTitle');
      }
    }

    return '';
  }

  private computeProfileInstallMethod_(): ProfileInstallMethod {
    if (this.isActivationCodeFromQrCode_) {
      return this.hasConsentedForDiscovery_ ?
          ProfileInstallMethod.kViaQrCodeAfterSmds :
          ProfileInstallMethod.kViaQrCodeSkippedSmds;
    }
    return this.hasConsentedForDiscovery_ ?
        ProfileInstallMethod.kViaActivationCodeAfterSmds :
        ProfileInstallMethod.kViaActivationCodeSkippedSmds;
  }

  /**
   * Returns true if profiles have been received and none were found.
   */
  private noProfilesFound_(): boolean {
    if (this.smdsSupportEnabled_) {
      return this.hasConsentedForDiscovery_ &&
          !!this.pendingProfileProperties_ &&
          this.pendingProfileProperties_.length === 0;
    } else {
      return (this.pendingProfiles_ && this.pendingProfiles_.length === 0);
    }
  }

  private profilesFound_(): boolean {
    if (this.smdsSupportEnabled_) {
      return this.hasConsentedForDiscovery_ &&
          !!this.pendingProfileProperties_ &&
          this.pendingProfileProperties_.length > 0;
    } else {
      return (this.pendingProfiles_ && this.pendingProfiles_.length > 0);
    }
  }
}

customElements.define(EsimFlowUiElement.is, EsimFlowUiElement);
