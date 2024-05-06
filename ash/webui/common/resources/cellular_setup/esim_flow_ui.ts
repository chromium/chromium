// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/polymer/v3_0/iron-pages/iron-pages.js';
import './setup_loading_page.js';
import './activation_code_page.js';
import './activation_verification_page.js';
import './final_page.js';
import './profile_discovery_consent_page.js';
import './profile_discovery_list_page.js';
import './confirmation_code_page.js';

import {I18nMixin} from '//resources/ash/common/cr_elements/i18n_mixin.js';
import {hasActiveCellularNetwork} from '//resources/ash/common/network/cellular_utils.js';
import {MojoInterfaceProviderImpl} from '//resources/ash/common/network/mojo_interface_provider.js';
import {NetworkListenerBehavior} from '//resources/ash/common/network/network_listener_behavior.js';
import {assert, assertNotReached} from '//resources/js/assert.js';
import {ESimManagerInterface, ESimOperationResult, ESimProfileProperties, EuiccRemote, ProfileInstallMethod, ProfileInstallResult, ProfileState} from '//resources/mojo/chromeos/ash/services/cellular_setup/public/mojom/esim_manager.mojom-webui.js';
import {FilterType, NetworkStateProperties, NO_LIMIT} from '//resources/mojo/chromeos/services/network_config/public/mojom/cros_network_config.mojom-webui.js';
import {ConnectionStateType, NetworkType} from '//resources/mojo/chromeos/services/network_config/public/mojom/network_types.mojom-webui.js';
import {mixinBehaviors, PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {ActivationCodePageElement} from './activation_code_page.js';
import {CellularSetupDelegate} from './cellular_setup_delegate.js';
import {ButtonBarState, ButtonState} from './cellular_types.js';
import {getTemplate} from './esim_flow_ui.html.js';
import {getEuicc} from './esim_manager_utils.js';
import {getESimManagerRemote} from './mojo_interface_provider.js';
import {ProfileDiscoveryListPageElement} from './profile_discovery_list_page.js';
import {SubflowMixin} from './subflow_mixin.js';

export enum EsimPageName {
  PROFILE_LOADING = 'profileLoadingPage',
  PROFILE_DISCOVERY_CONSENT = 'profileDiscoveryConsentPage',
  PROFILE_DISCOVERY = 'profileDiscoveryPage',
  ACTIVATION_CODE = 'activationCodePage',
  CONFIRMATION_CODE = 'confirmationCodePage',
  PROFILE_INSTALLING = 'profileInstallingPage',
  FINAL = 'finalPage',
}

export enum EsimUiState {
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
export enum EsimSetupFlowResult {
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
        computed: 'computeHeader_(selectedEsimPageName_, showError_)',
      },

      forwardButtonLabel: {
        type: String,
        notify: true,
      },

      state_: {
        type: String,
        value: EsimUiState.PROFILE_SEARCH_CONSENT,
        observer: 'onStateChanged_',
      },

      /**
       * Element name of the current selected sub-page.
       * This is set in updateSelectedPage_ on initialization.
       */
      selectedEsimPageName_: String,

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
    };
  }

  delegate: CellularSetupDelegate;
  header: string;
  forwardButtonLabel: string;
  private state_: string;
  private selectedEsimPageName_: string;
  private hasConsentedForDiscovery_: boolean;
  private shouldSkipDiscovery_: boolean;
  private showError_: boolean;
  private pendingProfileProperties_: ESimProfileProperties[];
  private selectedProfileProperties_: ESimProfileProperties|null;
  private activationCode_: string;
  private confirmationCode_: string;
  private hasHadActiveCellularNetwork_: boolean;
  private isActivationCodeFromQrCode_: boolean;

  /**
   * Provides an interface to the ESimManager Mojo service.
   */
  private eSimManagerRemote_: ESimManagerInterface;
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
          resultCode = EsimSetupFlowResult.ERROR_FETCHING_PROFILES;
        } else if (this.noProfilesFound_()) {
          resultCode = EsimSetupFlowResult.CANCELLED_NO_PROFILES;
        } else {
          resultCode = EsimSetupFlowResult.CANCELLED_WITHOUT_ERROR;
        }
        break;
      case ProfileInstallResult.kSuccess:
        resultCode = EsimSetupFlowResult.SUCCESS;
        break;
      case ProfileInstallResult.kFailure:
        resultCode = EsimSetupFlowResult.INSTALL_FAIL;
        break;
      case ProfileInstallResult.kErrorNeedsConfirmationCode:
        resultCode = EsimSetupFlowResult.CANCELLED_NEEDS_CONFIRMATION_CODE;
        break;
      case ProfileInstallResult.kErrorInvalidActivationCode:
        resultCode = EsimSetupFlowResult.CANCELLED_INVALID_ACTIVATION_CODE;
        break;
      default:
        break;
    }

    if (this.isOffline_ && resultCode !== ProfileInstallResult.kSuccess) {
      resultCode = EsimSetupFlowResult.NO_NETWORK;
    }

    assert(resultCode !== null);
    chrome.metricsPrivate.recordEnumerationValue(
        ESIM_SETUP_RESULT_METRIC_NAME, resultCode,
        Object.keys(EsimSetupFlowResult).length);

    const elapsedTimeMs = new Date().getTime() - this.timeOnAttached_!.getTime();
    if (resultCode === EsimSetupFlowResult.SUCCESS) {
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
    // Installing an eSIM profile may result in Hermes restarting and losing
    // its state. When this happens the cache of profiles used for the UI may
    // become corrupted and will not include all installed profiles.
    // Explicitly refresh the cache when this dialog is opened to ensure the
    // cache is regenerated and valid.
    this.refreshInstalledProfiles_();
    this.onNetworkStateListChanged();
  }

  private async fetchProfiles_(): Promise<void> {
    await this.getEuicc_();
    if (!this.euicc_) {
      return;
    }

    await this.getAvailableProfileProperties_();
    if (this.noProfilesFound_()) {
      this.state_ = EsimUiState.ACTIVATION_CODE_ENTRY;
    } else {
      this.state_ = EsimUiState.PROFILE_SELECTION;
    }
  }

  private async getEuicc_(): Promise<void> {
    const euicc = await getEuicc();
    if (!euicc) {
      this.hasFailedFetchingProfiles_ = true;
      this.showError_ = true;
      this.state_ = EsimUiState.SETUP_FINISH;
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

  private async refreshInstalledProfiles_(): Promise<void> {
    await this.getEuicc_();
    if (!this.euicc_) {
      return;
    }
    await this.euicc_.refreshInstalledProfiles();
  }

  private handleProfileInstallResponse_(
      response: {result: ProfileInstallResult}): void {
    this.lastProfileInstallResult_ = response.result;
    if (response.result === ProfileInstallResult.kErrorNeedsConfirmationCode) {
      this.state_ = EsimUiState.CONFIRMATION_CODE_ENTRY;
      return;
    }
    this.showError_ = response.result !== ProfileInstallResult.kSuccess;
    if (response.result === ProfileInstallResult.kFailure &&
        this.state_ === EsimUiState.CONFIRMATION_CODE_ENTRY_INSTALLING) {
      this.state_ = EsimUiState.CONFIRMATION_CODE_ENTRY_READY;
      return;
    }
    if (response.result === ProfileInstallResult.kErrorInvalidActivationCode &&
        this.state_ !== EsimUiState.PROFILE_SELECTION_INSTALLING) {
      this.state_ = EsimUiState.ACTIVATION_CODE_ENTRY_READY;
      return;
    }
    if (response.result === ProfileInstallResult.kSuccess ||
        response.result === ProfileInstallResult.kFailure) {
      this.state_ = EsimUiState.SETUP_FINISH;
    }
  }

  private onStateChanged_(newState: EsimUiState, oldState: EsimUiState): void {
    this.updateButtonBarState_();
    this.updateSelectedPage_();
    if (this.hasConsentedForDiscovery_ &&
        newState === EsimUiState.PROFILE_SEARCH) {
      this.fetchProfiles_();
    }
    this.initializePageState_(newState, oldState);
  }

  private updateSelectedPage_(): void {
    const oldSelectedEsimPageName = this.selectedEsimPageName_;
    switch (this.state_) {
      case EsimUiState.PROFILE_SEARCH:
        this.selectedEsimPageName_ = EsimPageName.PROFILE_LOADING;
        break;
      case EsimUiState.PROFILE_SEARCH_CONSENT:
        this.selectedEsimPageName_ = EsimPageName.PROFILE_DISCOVERY_CONSENT;
        break;
      case EsimUiState.ACTIVATION_CODE_ENTRY:
      case EsimUiState.ACTIVATION_CODE_ENTRY_READY:
        this.selectedEsimPageName_ = EsimPageName.ACTIVATION_CODE;
        break;
      case EsimUiState.ACTIVATION_CODE_ENTRY_INSTALLING:
        this.selectedEsimPageName_ = EsimPageName.PROFILE_INSTALLING;
        break;
      case EsimUiState.CONFIRMATION_CODE_ENTRY:
      case EsimUiState.CONFIRMATION_CODE_ENTRY_READY:
          this.selectedEsimPageName_ = EsimPageName.CONFIRMATION_CODE;
        break;
      case EsimUiState.CONFIRMATION_CODE_ENTRY_INSTALLING:
          this.selectedEsimPageName_ = EsimPageName.PROFILE_INSTALLING;
        break;
      case EsimUiState.PROFILE_SELECTION:
          this.selectedEsimPageName_ = EsimPageName.PROFILE_DISCOVERY;
        break;
      case EsimUiState.PROFILE_SELECTION_INSTALLING:
          this.selectedEsimPageName_ = EsimPageName.PROFILE_INSTALLING;
        break;
      case EsimUiState.SETUP_FINISH:
        this.selectedEsimPageName_ = EsimPageName.FINAL;
        break;
      default:
        assertNotReached();
    }
    // If there is a page change, fire focus event.
    if (oldSelectedEsimPageName !== this.selectedEsimPageName_) {
      this.dispatchEvent(new CustomEvent('focus-default-button', {
        bubbles: true, composed: true,
      }));
    }
  }

  private generateButtonStateForActivationPage_(
      enableForwardBtn: boolean,
      cancelButtonStateIfEnabled: ButtonState): ButtonBarState {
    this.forwardButtonLabel = this.i18n('next');
    return {
      cancel: cancelButtonStateIfEnabled,
      forward: enableForwardBtn ? ButtonState.ENABLED : ButtonState.DISABLED,
    };
  }

  private generateButtonStateForConfirmationPage_(
      enableForwardBtn: boolean,
      cancelButtonStateIfEnabled: ButtonState): ButtonBarState {
    this.forwardButtonLabel = this.i18n('confirm');
    return {
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
      case EsimUiState.PROFILE_SEARCH:
        this.forwardButtonLabel = this.i18n('next');
        buttonState = {
          cancel: cancelButtonStateIfEnabled,
          forward: ButtonState.DISABLED,
        };
        break;
      case EsimUiState.PROFILE_SEARCH_CONSENT:
        this.forwardButtonLabel = this.i18n('profileDiscoveryConsentScan');
        buttonState = {
          cancel: ButtonState.ENABLED,
          forward: ButtonState.ENABLED,
        };
        break;
      case EsimUiState.ACTIVATION_CODE_ENTRY:
        buttonState = this.generateButtonStateForActivationPage_(
            /*enableForwardBtn*/ false, cancelButtonStateIfEnabled);
        break;
      case EsimUiState.ACTIVATION_CODE_ENTRY_READY:
        buttonState = this.generateButtonStateForActivationPage_(
            /*enableForwardBtn*/ true, cancelButtonStateIfEnabled);
        break;
      case EsimUiState.ACTIVATION_CODE_ENTRY_INSTALLING:
        buttonState = this.generateButtonStateForActivationPage_(
            /*enableForwardBtn*/ false, cancelButtonStateIfDisabled);
        break;
      case EsimUiState.CONFIRMATION_CODE_ENTRY:
        buttonState = this.generateButtonStateForConfirmationPage_(
            /*enableForwardBtn*/ false, cancelButtonStateIfEnabled);
        break;
      case EsimUiState.CONFIRMATION_CODE_ENTRY_READY:
        buttonState = this.generateButtonStateForConfirmationPage_(
            /*enableForwardBtn*/ true, cancelButtonStateIfEnabled);
        break;
      case EsimUiState.CONFIRMATION_CODE_ENTRY_INSTALLING:
        buttonState = this.generateButtonStateForConfirmationPage_(
            /*enableForwardBtn*/ false, cancelButtonStateIfDisabled);
        break;
      case EsimUiState.PROFILE_SELECTION:
        this.updateForwardButtonLabel_();
        buttonState = {
          cancel: cancelButtonStateIfEnabled,
          forward: ButtonState.ENABLED,
        };
        break;
      case EsimUiState.PROFILE_SELECTION_INSTALLING:
        buttonState = {
          cancel: cancelButtonStateIfDisabled,
          forward: ButtonState.DISABLED,
        };
        break;
      case EsimUiState.SETUP_FINISH:
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

  private updateForwardButtonLabel_(): void {
    this.forwardButtonLabel = this.selectedProfileProperties_ ?
        this.i18n('next') :
        this.i18n('skipDiscovery');
  }

  private initializePageState_(newState: EsimUiState, oldState: EsimUiState):
      void {
    if (newState === EsimUiState.CONFIRMATION_CODE_ENTRY &&
        oldState !== EsimUiState.CONFIRMATION_CODE_ENTRY_READY) {
      this.confirmationCode_ = '';
    }
    if (newState === EsimUiState.ACTIVATION_CODE_ENTRY &&
        oldState !== EsimUiState.ACTIVATION_CODE_ENTRY_READY) {
      this.activationCode_ = '';
    }
  }

  private onActivationCodeUpdated_(event: CustomEvent<{activationCode: string|null}>): void {
    // initializePageState_() may cause this observer to fire and update the
    // buttonState when we're not on the activation code page. Check we're on
    // the activation code page before proceeding.
    if (this.state_ !== EsimUiState.ACTIVATION_CODE_ENTRY &&
        this.state_ !== EsimUiState.ACTIVATION_CODE_ENTRY_READY) {
      return;
    }
    this.state_ = event.detail.activationCode ?
        EsimUiState.ACTIVATION_CODE_ENTRY_READY :
        EsimUiState.ACTIVATION_CODE_ENTRY;
  }

  private onSelectedProfilePropertiesChanged_(): void {
    // initializePageState_() may cause this observer to fire and update the
    // buttonState when we're not on the profile selection page. Check we're
    // on the profile selection page before proceeding.
    if (this.state_ !== EsimUiState.PROFILE_SELECTION) {
      return;
    }

    this.updateForwardButtonLabel_();
  }

  private onConfirmationCodeUpdated_(): void {
    // initializePageState_() may cause this observer to fire and update the
    // buttonState when we're not on the confirmation code page. Check we're
    // on the confirmation code page before proceeding.
    if (this.state_ !== EsimUiState.CONFIRMATION_CODE_ENTRY &&
        this.state_ !== EsimUiState.CONFIRMATION_CODE_ENTRY_READY) {
      return;
    }
    this.state_ = this.confirmationCode_ ?
        EsimUiState.CONFIRMATION_CODE_ENTRY_READY :
        EsimUiState.CONFIRMATION_CODE_ENTRY;
  }

  /** SubflowMixin override */
  override navigateForward(): void {
    this.showError_ = false;
    switch (this.state_) {
      case EsimUiState.PROFILE_SEARCH_CONSENT:
        if (this.shouldSkipDiscovery_) {
          this.state_ = EsimUiState.ACTIVATION_CODE_ENTRY;
          break;
        }
        // Set |this.hasConsentedForDiscovery_| to |true| since navigating
        // forward and not setting up manually is explicitly giving consent
        // to perform SM-DS scans.
        this.hasConsentedForDiscovery_= true;
        this.state_ = EsimUiState.PROFILE_SEARCH;
        break;
      case EsimUiState.ACTIVATION_CODE_ENTRY_READY:
        assert(this.euicc_);
        // Assume installing the profile doesn't require a confirmation
        // code.
        const confirmationCode = '';
        this.state_ = EsimUiState.ACTIVATION_CODE_ENTRY_INSTALLING;
        this.euicc_
            .installProfileFromActivationCode(
                this.activationCode_, confirmationCode,
                this.computeProfileInstallMethod_())
            .then(this.handleProfileInstallResponse_.bind(this));
        break;
      case EsimUiState.PROFILE_SELECTION:
          if (this.selectedProfileProperties_) {
            assert(this.euicc_);
            this.state_ = EsimUiState.PROFILE_SELECTION_INSTALLING;
            // Assume installing the profile doesn't require a confirmation
            // code.
            const confirmationCode = '';
            this.euicc_
                .installProfileFromActivationCode(
                    this.selectedProfileProperties_.activationCode,
                    confirmationCode, ProfileInstallMethod.kViaSmds)
                .then(this.handleProfileInstallResponse_.bind(this));
          } else {
            this.state_ = EsimUiState.ACTIVATION_CODE_ENTRY;
          }
        break;
      case EsimUiState.CONFIRMATION_CODE_ENTRY_READY:
        this.state_ = EsimUiState.CONFIRMATION_CODE_ENTRY_INSTALLING;
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
        break;
      case EsimUiState.SETUP_FINISH:
        this.dispatchEvent(new CustomEvent('exit-cellular-setup', {
          bubbles: true, composed: true,
        }));
        break;
      default:
        assertNotReached();
    }
  }

  /** SubflowMixin override */
  override maybeFocusPageElement(): boolean {
    switch (this.state_) {
      case EsimUiState.ACTIVATION_CODE_ENTRY:
      case EsimUiState.ACTIVATION_CODE_ENTRY_READY:
        const activationCodePage =
            this.shadowRoot!.querySelector<ActivationCodePageElement>(
                '#activationCodePage');

        if (!activationCodePage) {
          return false;
        }
        return activationCodePage!.attemptToFocusOnPageContent();
      case EsimUiState.PROFILE_SELECTION:
        const profileDiscoveryPage =
            this.shadowRoot!.querySelector<ProfileDiscoveryListPageElement>(
                '#profileDiscoveryPage');

        if (!profileDiscoveryPage) {
          return false;
        }
        return profileDiscoveryPage.attemptToFocusOnFirstProfile();
      default:
        return false;
    }
  }

  private onForwardNavigationRequested_(): void {
    if (this.state_ === EsimUiState.ACTIVATION_CODE_ENTRY_READY ||
        this.state_ === EsimUiState.CONFIRMATION_CODE_ENTRY_READY ||
        this.state_ === EsimUiState.PROFILE_SEARCH_CONSENT ||
        this.state_ === EsimUiState.PROFILE_SELECTION) {
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

  private computeHeader_(): string {
    if (this.selectedEsimPageName_ === EsimPageName.FINAL && !this.showError_) {
      return this.i18n('eSimFinalPageSuccessHeader');
    }

    if (this.selectedEsimPageName_ === EsimPageName.PROFILE_DISCOVERY_CONSENT) {
      return this.i18n('profileDiscoveryConsentTitle');
    }

    if (this.selectedEsimPageName_ === EsimPageName.PROFILE_DISCOVERY) {
      return this.i18n('profileDiscoveryPageTitle');
    }

    if (this.selectedEsimPageName_ == EsimPageName.CONFIRMATION_CODE) {
      return this.i18n('confimationCodePageTitle');
    }
    if (this.selectedEsimPageName_ == EsimPageName.PROFILE_LOADING) {
      return this.i18n('profileLoadingPageTitle');
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
    return this.hasConsentedForDiscovery_ && !!this.pendingProfileProperties_ &&
        this.pendingProfileProperties_.length === 0;
  }

  private profilesFound_(): boolean {
    return this.hasConsentedForDiscovery_ && !!this.pendingProfileProperties_ &&
        this.pendingProfileProperties_.length > 0;
  }

  getSelectedEsimPageNameForTest(): string {
    return this.selectedEsimPageName_;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [EsimFlowUiElement.is]: EsimFlowUiElement;
  }
}

customElements.define(EsimFlowUiElement.is, EsimFlowUiElement);
