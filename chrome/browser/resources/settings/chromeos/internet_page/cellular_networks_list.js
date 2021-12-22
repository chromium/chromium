// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Polymer element for displaying a summary of Cellular network
 * states
 */

import '//resources/cr_components/chromeos/cellular_setup/cellular_eid_dialog.m.js';
import '//resources/cr_elements/cr_icon_button/cr_icon_button.m.js';
import '//resources/cr_elements/cr_icons_css.m.js';
import '//resources/polymer/v3_0/iron-flex-layout/iron-flex-layout-classes.js';
import '//resources/cr_elements/shared_style_css.m.js';
import '//resources/cr_elements/shared_vars_css.m.js';
import '../os_settings_icons_css.m.js';
import './esim_install_error_dialog.js';

import {Button, ButtonBarState, ButtonState, CellularSetupPageName} from '//resources/cr_components/chromeos/cellular_setup/cellular_types.m.js';
import {ESimManagerListenerBehavior} from '//resources/cr_components/chromeos/cellular_setup/esim_manager_listener_behavior.m.js';
import {getESimProfile, getESimProfileProperties, getEuicc, getNonPendingESimProfiles, getNumESimProfiles, getPendingESimProfiles} from '//resources/cr_components/chromeos/cellular_setup/esim_manager_utils.m.js';
import {getCellularSetupRemote, getESimManagerRemote, observeESimManager, setCellularSetupRemoteForTesting, setESimManagerRemoteForTesting} from '//resources/cr_components/chromeos/cellular_setup/mojo_interface_provider.m.js';
import {getSimSlotCount, hasActiveCellularNetwork, isActiveSim, isConnectedToNonCellularNetwork} from '//resources/cr_components/chromeos/network/cellular_utils.m.js';
import {MojoInterfaceProvider, MojoInterfaceProviderImpl} from '//resources/cr_components/chromeos/network/mojo_interface_provider.m.js';
import {NetworkList} from '//resources/cr_components/chromeos/network/network_list_types.m.js';
import {OncMojo} from '//resources/cr_components/chromeos/network/onc_mojo.m.js';
import {CrActionMenuElement} from '//resources/cr_elements/cr_action_menu/cr_action_menu.js';
import {assert, assertNotReached} from '//resources/js/assert.m.js';
import {I18nBehavior} from '//resources/js/i18n_behavior.m.js';
import {WebUIListenerBehavior} from '//resources/js/web_ui_listener_behavior.m.js';
import {afterNextRender, flush, html, Polymer, TemplateInstanceBase, Templatizer} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {MultiDeviceBrowserProxy, MultiDeviceBrowserProxyImpl} from '../multidevice_page/multidevice_browser_proxy.m.js';
import {MultiDeviceFeature, MultiDeviceFeatureState, MultiDevicePageContentData, MultiDeviceSettingsMode, PhoneHubNotificationAccessStatus, SmartLockSignInEnabledState} from '../multidevice_page/multidevice_constants.m.js';

Polymer({
  _template: html`{__html_template__}`,
  is: 'cellular-networks-list',

  behaviors: [
    ESimManagerListenerBehavior,
    I18nBehavior,
    WebUIListenerBehavior,
  ],

  properties: {
    /**
     * The list of network state properties for the items to display.
     * @type {!Array<!OncMojo.NetworkStateProperties>}
     */
    networks: {
      type: Array,
      value() {
        return [];
      },
      observer: 'onNetworksListChanged_',
    },

    /**
     * Whether to show technology badge on mobile network icons.
     */
    showTechnologyBadge: Boolean,

    /**
     * Device state for the cellular network type.
     * @type {!OncMojo.DeviceStateProperties|undefined}
     */
    cellularDeviceState: Object,

    isConnectedToNonCellularNetwork: {
      type: Boolean,
    },

    /**
     * If true, inhibited spinner can be shown, it will be shown
     * if true and cellular is inhibited.
     * @type {boolean}
     */
    canShowSpinner: {
      type: Boolean,
    },

    /**
     * Device state for the tether network type. This device state should be
     * used for instant tether networks.
     * @type {!OncMojo.DeviceStateProperties|undefined}
     */
    tetherDeviceState: Object,

    /** @type {!chromeos.networkConfig.mojom.GlobalPolicy|undefined} */
    globalPolicy: Object,

    /**
     * The list of eSIM network state properties for display.
     * @type {!Array<!OncMojo.NetworkStateProperties>}
     * @private
     */
    eSimNetworks_: {
      type: Array,
      value() {
        return [];
      },
    },

    /**
     * Dictionary mapping pending eSIM profile iccids to pending eSIM profiles.
     * @type {!Map<string, chromeos.cellularSetup.mojom.ESimProfileRemote>}
     * @private
     */
    profilesMap_: {
      type: Object,
      value() {
        return new Map();
      },
    },

    /**
     * The list of pending eSIM profiles to display after the list of eSIM
     * networks.
     * @type {!Array<NetworkList.CustomItemState>}
     * @private
     */
    eSimPendingProfileItems_: {
      type: Array,
      value() {
        return [];
      },
    },

    /**
     * The list of pSIM network state properties for display.
     * @type {!Array<!OncMojo.NetworkStateProperties>}
     * @private
     */
    pSimNetworks_: {
      type: Array,
      value() {
        return [];
      },
    },

    /**
     * The list of tether network state properties for display.
     * @type {!Array<!OncMojo.NetworkStateProperties>}
     * @private
     */
    tetherNetworks_: {
      type: Array,
      value() {
        return [];
      },
    },

    /**@private */
    shouldShowEidDialog_: {
      type: Boolean,
      value: false,
    },

    /** @private {boolean} */
    shouldShowInstallErrorDialog_: {
      type: Boolean,
      value: false,
    },

    /**
     * Euicc object representing the active euicc_ module on the device
     * @private {?chromeos.cellularSetup.mojom.EuiccRemote}
     */
    euicc_: {
      type: Object,
      value: null,
    },

    /**
     * The current eSIM profile being installed.
     * @type {?chromeos.cellularSetup.mojom.ESimProfileRemote}
     * @private
     */
    installingESimProfile_: {
      type: Object,
      value: null,
    },

    /**
     * The error code returned when eSIM profile install attempt was made.
     * @type {?chromeos.cellularSetup.mojom.ProfileInstallResult}
     * @private
     */
    eSimProfileInstallError_: {
      type: Object,
      value: null,
    },

    /**
     * Multi-device page data used to determine if the tether section should be
     * shown or not.
     * @type {?MultiDevicePageContentData}
     * @private
     */
    multiDevicePageContentData_: {
      type: Object,
      value: null,
    },

    /** @private {boolean} */
    isDeviceInhibited_: {
      type: Boolean,
      computed: 'computeIsDeviceInhibited_(cellularDeviceState,' +
          'cellularDeviceState.inhibitReason)',
    },

    /** @private {boolean} */
    isESimPolicyEnabled_: {
      type: Boolean,
      value() {
        return loadTimeData.valueExists('esimPolicyEnabled') &&
            loadTimeData.getBoolean('esimPolicyEnabled');
      }
    },
  },

  listeners: {
    'install-profile': 'installProfile_',
  },

  /** @private {?chromeos.networkConfig.mojom.CrosNetworkConfigRemote} */
  networkConfig_: null,

  /** @override */
  created() {
    this.networkConfig_ =
        MojoInterfaceProviderImpl.getInstance().getMojoServiceRemote();
    this.fetchESimPendingProfileList_();
  },

  /** @override */
  ready() {
    this.addWebUIListener(
        'settings.updateMultidevicePageContentData',
        this.onMultiDevicePageContentDataChanged_.bind(this));

    const browserProxy = MultiDeviceBrowserProxyImpl.getInstance();
    browserProxy.getPageContentData().then(
        this.onMultiDevicePageContentDataChanged_.bind(this));
  },

  /**
   * @param {!chromeos.cellularSetup.mojom.EuiccRemote} euicc
   * ESimManagerListenerBehavior override
   */
  onProfileListChanged(euicc) {
    this.fetchESimPendingProfileListForEuicc_(euicc);
  },

  /**
   * ESimManagerListenerBehavior override
   */
  onAvailableEuiccListChanged() {
    this.fetchESimPendingProfileList_();
  },

  /**
   * @param {!chromeos.cellularSetup.mojom.ESimProfileRemote} profile
   * ESimManagerListenerBehavior override
   */
  onProfileChanged(profile) {
    profile.getProperties().then(response => {
      const eSimPendingProfileItem =
          this.eSimPendingProfileItems_.find(item => {
            return item.customData.iccid === response.properties.iccid;
          });
      if (!eSimPendingProfileItem) {
        return;
      }
      eSimPendingProfileItem.customItemType = response.properties.state ===
              chromeos.cellularSetup.mojom.ProfileState.kInstalling ?
          NetworkList.CustomItemType.ESIM_INSTALLING_PROFILE :
          NetworkList.CustomItemType.ESIM_PENDING_PROFILE;
    });
  },

  /** @private */
  fetchESimPendingProfileList_() {
    getEuicc().then(euicc => {
      if (!euicc) {
        return;
      }
      this.euicc_ = euicc;
      this.fetchESimPendingProfileListForEuicc_(euicc);
    });
  },

  /**
   * Return true if esim section should be shown.
   * @return {boolean}
   * @private
   */
  shouldShowEsimSection_() {
    if (!this.cellularDeviceState) {
      return false;
    }
    const {eSimSlots} = getSimSlotCount(this.cellularDeviceState);
    // Check both the SIM slot infos and the number of EUICCs because the former
    // comes from Shill and the latter from Hermes, so there may be instances
    // where one may be true while they other isn't.
    return !!this.euicc_ && eSimSlots > 0;
  },

  /**
   * @param {!chromeos.cellularSetup.mojom.EuiccRemote} euicc
   * @private
   */
  fetchESimPendingProfileListForEuicc_(euicc) {
    getPendingESimProfiles(euicc).then(
        this.processESimPendingProfiles_.bind(this));
  },

  /**
   * @param {Array<!chromeos.cellularSetup.mojom.ESimProfileRemote>} profiles
   * @private
   */
  processESimPendingProfiles_(profiles) {
    this.profilesMap_ = new Map();
    const eSimPendingProfilePromises =
        profiles.map(this.createESimPendingProfilePromise_.bind(this));
    Promise.all(eSimPendingProfilePromises).then(eSimPendingProfileItems => {
      this.eSimPendingProfileItems_ = eSimPendingProfileItems;
    });
  },

  /**
   * @param {!chromeos.cellularSetup.mojom.ESimProfileRemote} profile
   * @return {!Promise<NetworkList.CustomItemState>}
   * @private
   */
  createESimPendingProfilePromise_(profile) {
    return profile.getProperties().then(response => {
      this.profilesMap_.set(response.properties.iccid, profile);
      return this.createESimPendingProfileItem_(response.properties);
    });
  },

  /**
   * @param {!chromeos.cellularSetup.mojom.ESimProfileProperties} properties
   * @return {NetworkList.CustomItemState}
   */
  createESimPendingProfileItem_(properties) {
    return {
      customItemType: properties.state ===
              chromeos.cellularSetup.mojom.ProfileState.kInstalling ?
          NetworkList.CustomItemType.ESIM_INSTALLING_PROFILE :
          NetworkList.CustomItemType.ESIM_PENDING_PROFILE,
      customItemName: String.fromCharCode(...properties.name.data),
      customItemSubtitle:
          String.fromCharCode(...properties.serviceProvider.data),
      polymerIcon: 'network:cellular-0',
      showBeforeNetworksList: false,
      customData: {
        iccid: properties.iccid,
      },
    };
  },

  /**
   * @private
   */
  onNetworksListChanged_() {
    const mojom = chromeos.networkConfig.mojom;

    const pSimNetworks = [];
    const eSimNetworks = [];
    const tetherNetworks = [];

    for (const network of this.networks) {
      if (network.type === mojom.NetworkType.kTether) {
        tetherNetworks.push(network);
        continue;
      }

      if (network.typeState.cellular && network.typeState.cellular.eid) {
        eSimNetworks.push(network);
      } else {
        pSimNetworks.push(network);
      }
    }
    this.eSimNetworks_ = eSimNetworks;
    this.pSimNetworks_ = pSimNetworks;
    this.tetherNetworks_ = tetherNetworks;
  },

  /**
   * @param {...!Array<!NetworkList.NetworkListItemType>} lists
   * @returns {boolean}
   * @private
   */
  shouldShowNetworkSublist_(...lists) {
    const totalListLength = lists.reduce((accumulator, currentList) => {
      return accumulator + currentList.length;
    }, 0);
    return totalListLength > 0;
  },

  /**
   * @param {!OncMojo.DeviceStateProperties|undefined} cellularDeviceState
   * @returns {boolean}
   * @private
   */
  shouldShowPSimSection_(cellularDeviceState) {
    const {pSimSlots} = getSimSlotCount(cellularDeviceState);
    return pSimSlots > 0;
  },

  /**
   * @param {!MultiDevicePageContentData} newData
   * @private
   */
  onMultiDevicePageContentDataChanged_(newData) {
    this.multiDevicePageContentData_ = newData;
  },

  /**
   * @param {?MultiDevicePageContentData} pageContentData
   * @returns {boolean}
   * @private
   */
  shouldShowTetherSection_(pageContentData) {
    if (!pageContentData) {
      return false;
    }
    return pageContentData.instantTetheringState ===
        MultiDeviceFeatureState.ENABLED_BY_USER;
  },

  /**
   * @param {Event} event
   * @private
   */
  onEsimLearnMoreClicked_(event) {
    event.detail.event.preventDefault();
    event.stopPropagation();

    this.fire(
        'show-cellular-setup', {pageName: CellularSetupPageName.ESIM_FLOW_UI});
  },

  /**
   * @param {!Event} e
   * @private
   */
  onESimDotsClick_(e) {
    const menu = /** @type {!CrActionMenuElement} */ (this.$$('#menu').get());
    menu.showAt(/** @type {!Element} */ (e.target));
  },

  /** @private */
  onShowEidDialogTap_() {
    const actionMenu =
        /** @type {!CrActionMenuElement} */ (this.$$('cr-action-menu'));
    actionMenu.close();
    this.shouldShowEidDialog_ = true;
  },

  /** @private */
  onCloseEidDialog_() {
    this.shouldShowEidDialog_ = false;
  },

  /**
   * @param {Event} event
   * @private
   */
  installProfile_(event) {
    if (!this.isConnectedToNonCellularNetwork) {
      this.fire('show-error-toast', this.i18n('eSimNoConnectionErrorToast'));
      return;
    }
    this.installingESimProfile_ = this.profilesMap_.get(event.detail.iccid);
    this.installingESimProfile_.installProfile('').then((response) => {
      if (response.result ===
          chromeos.cellularSetup.mojom.ProfileInstallResult.kSuccess) {
        this.eSimProfileInstallError_ = null;
        this.installingESimProfile_ = null;
      } else {
        this.eSimProfileInstallError_ = response.result;
        this.showInstallErrorDialog_();
      }
    });
  },

  /** @private */
  showInstallErrorDialog_() {
    this.shouldShowInstallErrorDialog_ = true;
  },

  /** @private */
  onCloseInstallErrorDialog_() {
    this.shouldShowInstallErrorDialog_ = false;
  },

  /**
   * @param {!OncMojo.DeviceStateProperties|undefined} cellularDeviceState
   * @param {!chromeos.networkConfig.mojom.GlobalPolicy} globalPolicy
   * @return {boolean}
   * @private
   */
  showAddESimButton_(cellularDeviceState, globalPolicy) {
    assert(!!this.euicc_);
    if (!this.deviceIsEnabled_(cellularDeviceState)) {
      return false;
    }
    if (!this.isESimPolicyEnabled_ || !globalPolicy) {
      return true;
    }
    return !globalPolicy.allowOnlyPolicyCellularNetworks;
  },

  /**
   * @param {!OncMojo.DeviceStateProperties|undefined} cellularDeviceState
   * @return {boolean} True if the device is enabled.
   * @private
   */
  deviceIsEnabled_(cellularDeviceState) {
    const mojom = chromeos.networkConfig.mojom;
    return !!cellularDeviceState &&
        cellularDeviceState.deviceState === mojom.DeviceStateType.kEnabled;
  },

  /**
   * @return {boolean}
   * @private
   */
  computeIsDeviceInhibited_() {
    if (!this.cellularDeviceState) {
      return false;
    }
    return OncMojo.deviceIsInhibited(this.cellularDeviceState);
  },

  /** @private */
  onAddEsimButtonTap_() {
    this.fire(
        'show-cellular-setup', {pageName: CellularSetupPageName.ESIM_FLOW_UI});
  },

  /*
   * Returns the add esim button. If the device does not have an EUICC, no eSIM
   * slot, or policies prohibit users from adding a network, null is returned.
   * @return {?CrIconButtonElement}
   */
  getAddEsimButton() {
    return /** @type {?CrIconButtonElement} */ (this.$$('#addESimButton'));
  },

  /**
   * @return {string} Inhibited subtext message.
   * @private
   */
  getInhibitedSubtextMessage_() {
    if (!this.cellularDeviceState) {
      return '';
    }

    const mojom = chromeos.networkConfig.mojom.InhibitReason;
    const inhibitReason = this.cellularDeviceState.inhibitReason;

    switch (inhibitReason) {
      case mojom.kInstallingProfile:
        return this.i18n('cellularNetworkInstallingProfile');
      case mojom.kRenamingProfile:
        return this.i18n('cellularNetworkRenamingProfile');
      case mojom.kRemovingProfile:
        return this.i18n('cellularNetworkRemovingProfile');
      case mojom.kConnectingToProfile:
        return this.i18n('cellularNetworkConnectingToProfile');
      case mojom.kRefreshingProfileList:
        return this.i18n('cellularNetworRefreshingProfileListProfile');
      case mojom.kResettingEuiccMemory:
        return this.i18n('cellularNetworkResettingESim');
    }

    return '';
  },
});
