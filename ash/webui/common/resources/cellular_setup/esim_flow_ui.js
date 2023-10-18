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

import {assert, assertNotReached} from '//resources/ash/common/assert.js';
import {I18nBehavior} from '//resources/ash/common/i18n_behavior.js';
import {hasActiveCellularNetwork} from '//resources/ash/common/network/cellular_utils.js';
import {MojoInterfaceProvider, MojoInterfaceProviderImpl} from '//resources/ash/common/network/mojo_interface_provider.js';
import {NetworkListenerBehavior} from '//resources/ash/common/network/network_listener_behavior.js';
import {Polymer} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {loadTimeData} from 'chrome://resources/ash/common/load_time_data.m.js';
import {ESimManagerRemote, ESimOperationResult, ESimProfileProperties, ESimProfileRemote, EuiccRemote, ProfileInstallMethod, ProfileInstallResult, ProfileState} from 'chrome://resources/mojo/chromeos/ash/services/cellular_setup/public/mojom/esim_manager.mojom-webui.js';
import {FilterType, NetworkStateProperties, NO_LIMIT} from 'chrome://resources/mojo/chromeos/services/network_config/public/mojom/cros_network_config.mojom-webui.js';
import {ConnectionStateType, NetworkType} from 'chrome://resources/mojo/chromeos/services/network_config/public/mojom/network_types.mojom-webui.js';

import {CellularSetupDelegate} from './cellular_setup_delegate.js';
import {ButtonBarState, ButtonState} from './cellular_types.js';
import {getTemplate} from './esim_flow_ui.html.js';
import {getEuicc, getPendingESimProfiles} from './esim_manager_utils.js';
import {getESimManagerRemote} from './mojo_interface_provider.js';
import {SubflowBehavior} from './subflow_behavior.js';

/** @enum {string} */
export const ESimPageName = {
  PROFILE_LOADING: 'profileLoadingPage',
  PROFILE_DISCOVERY_CONSENT: 'profileDiscoveryConsentPage',
  PROFILE_DISCOVERY: 'profileDiscoveryPage',
  PROFILE_DISCOVERY_LEGACY: 'profileDiscoveryPageLegacy',
  ACTIVATION_CODE: 'activationCodePage',
  CONFIRMATION_CODE: 'confirmationCodePage',
  CONFIRMATION_CODE_LEGACY: 'confirmationCodePageLegacy',
  PROFILE_INSTALLING: 'profileInstallingPage',
  FINAL: 'finalPage',
};

/** @enum {string} */
export const ESimUiState = {
  PROFILE_SEARCH: 'profile-search',
  PROFILE_SEARCH_CONSENT: 'profile-search-consent',
  ACTIVATION_CODE_ENTRY: 'activation-code-entry',
  ACTIVATION_CODE_ENTRY_READY: 'activation-code-entry-ready',
  ACTIVATION_CODE_ENTRY_INSTALLING: 'activation-code-entry-installing',
  CONFIRMATION_CODE_ENTRY: 'confirmation-code-entry',
  CONFIRMATION_CODE_ENTRY_READY: 'confirmation-code-entry-ready',
  CONFIRMATION_CODE_ENTRY_INSTALLING: 'confirmation-code-entry-installing',
  PROFILE_SELECTION: 'profile-selection',
  PROFILE_SELECTION_INSTALLING: 'profile-selection-installing',
  SETUP_FINISH: 'setup-finish',
};

/**
 * The reason that caused the user to exit the ESim Setup flow.
 * These values are persisted to logs. Entries should not be renumbered
 * and numeric values should never be reused.
 * @enum {number}
 */
export const ESimSetupFlowResult = {
  SUCCESS: 0,
  INSTALL_FAIL: 1,
  CANCELLED_NEEDS_CONFIRMATION_CODE: 2,
  CANCELLED_INVALID_ACTIVATION_CODE: 3,
  ERROR_FETCHING_PROFILES: 4,
  CANCELLED_WITHOUT_ERROR: 5,
  CANCELLED_NO_PROFILES: 6,
  NO_NETWORK: 7,
};

export const ESIM_SETUP_RESULT_METRIC_NAME =
    'Network.Cellular.ESim.SetupFlowResult';

export const SUCCESSFUL_ESIM_SETUP_DURATION_METRIC_NAME =
    'Network.Cellular.ESim.CellularSetup.Success.Duration';

export const FAILED_ESIM_SETUP_DURATION_METRIC_NAME =
    'Network.Cellular.ESim.CellularSetup.Failure.Duration';

/**
 * Root element for the eSIM cellular setup flow. This element interacts with
 * the CellularSetup service to carry out the esim activation flow.
 */
Polymer({
  _template: getTemplate(),
  is: 'esim-flow-ui',

  behaviors: [
    I18nBehavior,
    NetworkListenerBehavior,
    SubflowBehavior,
  ],

  properties: {
    /** @type {!CellularSetupDelegate} */
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

    /**
     * @type {!ESimUiState}
     * @private
     */
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
     * @type {?ESimPageName}
     * @private
     */
    selectedESimPageName_: String,

    /**
     * Whether the user has consented to a scan for profiles.
     * @type {boolean}
     */
    hasConsentedForDiscovery_: {
      type: Boolean,
      value: false,
    },

    /**
     * Whether the user is setting up the eSIM profile manually.
     * @type {boolean}
     */
    shouldSkipDiscovery_: {
      type: Boolean,
      value: false,
    },

    /**
     * Whether error state should be shown for the current page.
     * @private {boolean}
     */
    showError_: {
      type: Boolean,
      value: false,
    },

    /**
     * Profiles fetched that have status kPending.
     * @type {!Array<!ESimProfileRemote>}
     * @private
     */
    pendingProfiles_: {
      type: Array,
    },

    /**
     * Profile selected to be installed.
     * @type {?ESimProfileRemote}
     * @private
     */
    selectedProfile_: {
      type: Object,
    },

    /**
     * Profile properties fetched from the latest SM-DS scan.
     * @type {!Array<!ESimProfileProperties>}
     * @private
     */
    pendingProfileProperties_: {
      type: Array,
    },

    /**
     * Profile properties selected to be installed.
     * @type {?ESimProfileProperties}
     * @private
     */
    selectedProfileProperties_: {
      type: Object,
    },

    /** @private */
    activationCode_: {
      type: String,
      value: '',
    },

    /** @private */
    confirmationCode_: {
      type: String,
      value: '',
      observer: 'onConfirmationCodeUpdated_',
    },

    /** @private */
    hasHadActiveCellularNetwork_: {
      type: Boolean,
      value: false,
    },

    /** @private */
    isActivationCodeFromQrCode_: {
      type: Boolean,
    },

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
  },

  /**
   * Provides an interface to the ESimManager Mojo service.
   * @private {?ESimManagerRemote}
   */
  eSimManagerRemote_: null,

  /** @private {?EuiccRemote} */
  euicc_: null,

  /** @private {boolean} */
  hasFailedFetchingProfiles_: false,

  /** @private {?ProfileInstallResult} */
  lastProfileInstallResult_: null,

  /**
   * If there are no active network connections of any type.
   * @private {boolean}
   */
  isOffline_: false,

  /**
   * The time at which the ESim flow is attached.
   * @private {?Date}
   */
  timeOnAttached_: null,

  listeners: {
    'activation-code-updated': 'onActivationCodeUpdated_',
    'forward-navigation-requested': 'onForwardNavigationRequested_',
  },

  observers: [
    'onSelectedProfileChanged_(selectedProfile_)',
    'onSelectedProfilePropertiesChanged_(selectedProfileProperties_)',
  ],

  /** @override */
  created() {
    this.eSimManagerRemote_ = getESimManagerRemote();
    const networkConfig =
        MojoInterfaceProviderImpl.getInstance().getMojoServiceRemote();

    const filter = {
      filter: FilterType.kActive,
      limit: NO_LIMIT,
      networkType: NetworkType.kAll,
    };
    networkConfig.getNetworkStateList(filter).then(response => {
      this.onActiveNetworksChanged(response.result);
    });
  },

  /** @override */
  attached() {
    this.timeOnAttached_ = new Date();
  },

  /** @override */
  detached() {
    let resultCode = null;

    switch (this.lastProfileInstallResult_) {
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
        // Handles case when no profile installation was attempted.
        if (this.hasFailedFetchingProfiles_) {
          resultCode = ESimSetupFlowResult.ERROR_FETCHING_PROFILES;
        } else if (this.noProfilesFound_()) {
          resultCode = ESimSetupFlowResult.CANCELLED_NO_PROFILES;
        } else {
          resultCode = ESimSetupFlowResult.CANCELLED_WITHOUT_ERROR;
        }
        break;
    }

    if (this.isOffline_ && resultCode !== ProfileInstallResult.kSuccess) {
      resultCode = ESimSetupFlowResult.NO_NETWORK;
    }

    assert(resultCode !== null);
    chrome.metricsPrivate.recordEnumerationValue(
        ESIM_SETUP_RESULT_METRIC_NAME, resultCode,
        Object.keys(ESimSetupFlowResult).length);

    const elapsedTimeMs = new Date() - this.timeOnAttached_;
    if (resultCode === ESimSetupFlowResult.SUCCESS) {
      chrome.metricsPrivate.recordLongTime(
          SUCCESSFUL_ESIM_SETUP_DURATION_METRIC_NAME, elapsedTimeMs);
      return;
    }

    chrome.metricsPrivate.recordLongTime(
        FAILED_ESIM_SETUP_DURATION_METRIC_NAME, elapsedTimeMs);
  },

  /**
   * NetworkListenerBehavior override
   * Used to determine if there is an online network connection.
   * @param {!Array<NetworkStateProperties>}
   *     activeNetworks
   */
  onActiveNetworksChanged(activeNetworks) {
    this.isOffline_ = !activeNetworks.some(
        (network) => network.connectionState === ConnectionStateType.kOnline);
  },

  initSubflow() {
    if (!this.smdsSupportEnabled_) {
      this.fetchProfiles_();
    } else {
      this.getEuicc_();
    }
    this.onNetworkStateListChanged();
  },

  /** @private */
  async fetchProfiles_() {
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
  },

  /** @private */
  async getEuicc_() {
    const euicc = await getEuicc();
    if (!euicc) {
      this.hasFailedFetchingProfiles_ = true;
      this.showError_ = true;
      this.state_ = ESimUiState.SETUP_FINISH;
      console.warn('No Euiccs found');
      return;
    }
    this.euicc_ = euicc;
  },

  /**
   * @private
   */
  async getAvailableProfileProperties_() {
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
        requestAvailableProfilesResponse.profiles.filter(properties => {
          return properties.state === ProfileState.kPending &&
              properties.activationCode;
        });
  },

  /**
   * @private
   */
  async getPendingProfiles_() {
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
  },

  /**
   * @private
   * @param {{result: ProfileInstallResult}} response
   */
  handleProfileInstallResponse_(response) {
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
  },

  /** @private */
  onStateChanged_(newState, oldState) {
    this.updateButtonBarState_();
    this.updateSelectedPage_();
    if (this.hasConsentedForDiscovery_ &&
        newState === ESimUiState.PROFILE_SEARCH) {
      this.fetchProfiles_();
    }
    this.initializePageState_(newState, oldState);
  },

  /** @private */
  updateSelectedPage_() {
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
        break;
    }
    // If there is a page change, fire focus event.
    if (oldSelectedESimPageName !== this.selectedESimPageName_) {
      this.fire('focus-default-button');
    }
  },

  /**
   * @param {boolean} enableForwardBtn
   * @param {!ButtonState} cancelButtonStateIfEnabled
   * @param {boolean} isInstalling
   * @return {!ButtonBarState}
   * @private
   */
  generateButtonStateForActivationPage_(
      enableForwardBtn, cancelButtonStateIfEnabled, isInstalling) {
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
  },

  /**
   * @param {boolean} enableForwardBtn
   * @param {!ButtonState} cancelButtonStateIfEnabled
   * @param {boolean} isInstalling
   * @return {!ButtonBarState}
   * @private
   */
  generateButtonStateForConfirmationPage_(
      enableForwardBtn, cancelButtonStateIfEnabled, isInstalling) {
    this.forwardButtonLabel = this.i18n('confirm');
    let backBtnState = isInstalling ? ButtonState.DISABLED : ButtonState.ENABLED;
    if (this.smdsSupportEnabled_) {
      backBtnState = ButtonState.HIDDEN;
    }
    return {
      backward: backBtnState,
      cancel: cancelButtonStateIfEnabled,
      forward: enableForwardBtn ? ButtonState.ENABLED : ButtonState.DISABLED,
    };
  },

  /** @private */
  updateButtonBarState_() {
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
        break;
    }
    this.set('buttonState', buttonState);
  },

  /** @private */
  updateForwardButtonLabel_() {
    if (this.smdsSupportEnabled_) {
      this.forwardButtonLabel = this.selectedProfileProperties_ ?
          this.i18n('next') :
          this.i18n('skipDiscovery');
    } else {
      this.forwardButtonLabel = this.selectedProfile_ ?
          this.i18n('next') :
          this.i18n('skipDiscovery');
    }
  },

  /** @private */
  initializePageState_(newState, oldState) {
    if (newState === ESimUiState.CONFIRMATION_CODE_ENTRY &&
        oldState !== ESimUiState.CONFIRMATION_CODE_ENTRY_READY) {
      this.confirmationCode_ = '';
    }
    if (newState === ESimUiState.ACTIVATION_CODE_ENTRY &&
        oldState !== ESimUiState.ACTIVATION_CODE_ENTRY_READY) {
      this.activationCode_ = '';
    }
  },

  /** @private */
  onActivationCodeUpdated_(event) {
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
  },

  /** @private */
  onSelectedProfileChanged_() {
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
  },

  /** @private */
  onSelectedProfilePropertiesChanged_() {
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
  },

  /** @private */
  onConfirmationCodeUpdated_() {
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
  },

  /** SubflowBehavior override */
  navigateForward() {
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
          const fromQrCode = this.selectedProfileProperties_ ? true : false;
          const activationCode = fromQrCode ?
              this.selectedProfileProperties_.activationCode :
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
            this.euicc_
                .installProfileFromActivationCode(
                    this.activationCode_, this.confirmationCode_,
                    this.computeProfileInstallMethod_())
                .then(this.handleProfileInstallResponse_.bind(this));
          }
        }
        break;
      case ESimUiState.SETUP_FINISH:
        this.fire('exit-cellular-setup');
        break;
      default:
        assertNotReached();
        break;
    }
  },

  /** SubflowBehavior override */
  navigateBackward() {
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
  },

  /** @private */
  onForwardNavigationRequested_() {
    if (this.state_ === ESimUiState.ACTIVATION_CODE_ENTRY_READY ||
        this.state_ === ESimUiState.CONFIRMATION_CODE_ENTRY_READY ||
        this.state_ === ESimUiState.PROFILE_SEARCH_CONSENT ||
        this.state_ === ESimUiState.PROFILE_SELECTION) {
      this.navigateForward();
    }
  },

  /** NetworkListenerBehavior override */
  onNetworkStateListChanged() {
    hasActiveCellularNetwork().then((hasActive) => {
      // If hasHadActiveCellularNetwork_ has been set to true, don't set to
      // false again as we should show the cellular disconnect warning for the
      // duration of the flow's lifecycle.
      if (hasActive) {
        this.hasHadActiveCellularNetwork_ = hasActive;
      }
    });
  },

  /** @private */
  shouldShowSubpageBusy_() {
    return this.state_ === ESimUiState.ACTIVATION_CODE_ENTRY_INSTALLING ||
        this.state_ === ESimUiState.CONFIRMATION_CODE_ENTRY_INSTALLING ||
        this.state_ === ESimUiState.PROFILE_SELECTION_INSTALLING;
  },

  /** @private */
  getLoadingMessage_() {
    if (this.smdsSupportEnabled_) {
      return this.i18n('profileLoadingPageMessage');
    }

    return this.hasHadActiveCellularNetwork_ ?
        this.i18n('eSimProfileDetectDuringActiveCellularConnectionMessage') :
        this.i18n('eSimProfileDetectMessage');
  },

  /**
   * @return {string}
   * @private
   */
  computeHeader_() {
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
  },

  /**
   * @return {ProfileInstallMethod}
   * @private
   */
  computeProfileInstallMethod_() {
    if (this.isActivationCodeFromQrCode_) {
      return this.hasConsentedForDiscovery_ ?
          ProfileInstallMethod.kViaQrCodeAfterSmds :
          ProfileInstallMethod.kViaQrCodeSkippedSmds;
    }
    return this.hasConsentedForDiscovery_ ?
        ProfileInstallMethod.kViaActivationCodeAfterSmds :
        ProfileInstallMethod.kViaActivationCodeSkippedSmds;
  },

  /**
   * Returns true if profiles have been received and none were found.
   * @return {boolean}
   * @private
   */
  noProfilesFound_() {
    if (this.smdsSupportEnabled_) {
      return this.hasConsentedForDiscovery_ &&
          !!this.pendingProfileProperties_ &&
          this.pendingProfileProperties_.length === 0;
    } else {
      return (this.pendingProfiles_ && this.pendingProfiles_.length === 0);
    }
  },

  /** @private*/
  profilesFound_() {
    if (this.smdsSupportEnabled_) {
      return this.hasConsentedForDiscovery_ &&
          !!this.pendingProfileProperties_ &&
          this.pendingProfileProperties_.length > 0;
    } else {
      return (this.pendingProfiles_ && this.pendingProfiles_.length > 0);
    }
  },
});
