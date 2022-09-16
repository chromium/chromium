// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Polymer element for displaying a summary of Cellular network
 * states
 */

import 'chrome://resources/cr_components/chromeos/cellular_setup/cellular_eid_dialog.js';
import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';
import 'chrome://resources/cr_elements/cr_icons.css.js';
import 'chrome://resources/polymer/v3_0/iron-flex-layout/iron-flex-layout-classes.js';
import 'chrome://resources/cr_elements/policy/cr_policy_indicator.js';
import 'chrome://resources/cr_elements/cr_shared_style.css.js';
import 'chrome://resources/cr_elements/cr_shared_vars.css.js';
import '../os_settings_icons_css.js';
import './esim_install_error_dialog.js';
import 'chrome://resources/cr_elements/cr_action_menu/cr_action_menu.js';

import {CellularSetupPageName} from 'chrome://resources/cr_components/chromeos/cellular_setup/cellular_types.js';
import {ESimManagerListenerBehavior, ESimManagerListenerBehaviorInterface} from 'chrome://resources/cr_components/chromeos/cellular_setup/esim_manager_listener_behavior.js';
import {getEuicc, getPendingESimProfiles} from 'chrome://resources/cr_components/chromeos/cellular_setup/esim_manager_utils.js';
import {getSimSlotCount} from 'chrome://resources/cr_components/chromeos/network/cellular_utils.js';
import {MojoInterfaceProvider, MojoInterfaceProviderImpl} from 'chrome://resources/cr_components/chromeos/network/mojo_interface_provider.js';
import {NetworkList} from 'chrome://resources/cr_components/chromeos/network/network_list_types.js';
import {OncMojo} from 'chrome://resources/cr_components/chromeos/network/onc_mojo.js';
import {assert, assertNotReached} from 'chrome://resources/js/assert.m.js';
import {I18nBehavior, I18nBehaviorInterface} from 'chrome://resources/cr_elements/i18n_behavior.js';
import {WebUIListenerBehavior, WebUIListenerBehaviorInterface} from 'chrome://resources/cr_elements/web_ui_listener_behavior.js';
import {html, mixinBehaviors, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {MultiDeviceBrowserProxy, MultiDeviceBrowserProxyImpl} from '../multidevice_page/multidevice_browser_proxy.js';
import {MultiDeviceFeatureState, MultiDevicePageContentData} from '../multidevice_page/multidevice_constants.js';

/**
 * @constructor
 * @extends {PolymerElement}
 * @implements {ESimManagerListenerBehaviorInterface}
 * @implements {I18nBehaviorInterface}
 * @implements {WebUIListenerBehaviorInterface}
 */
const CellularNetworksListElementBase = mixinBehaviors(
    [ESimManagerListenerBehavior, I18nBehavior, WebUIListenerBehavior],
    PolymerElement);

/** @polymer */
class CellularNetworksListElement extends CellularNetworksListElementBase {
  static get is() {
    return 'cellular-networks-list';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
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
       * Dictionary mapping pending eSIM profile iccids to pending eSIM
       * profiles.
       * @type {!Map<string, ash.cellularSetup.mojom.ESimProfileRemote>}
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
       * @private {?ash.cellularSetup.mojom.EuiccRemote}
       */
      euicc_: {
        type: Object,
        value: null,
      },

      /**
       * The current eSIM profile being installed.
       * @type {?ash.cellularSetup.mojom.ESimProfileRemote}
       * @private
       */
      installingESimProfile_: {
        type: Object,
        value: null,
      },

      /**
       * The error code returned when eSIM profile install attempt was made.
       * @type {?ash.cellularSetup.mojom.ProfileInstallResult}
       * @private
       */
      eSimProfileInstallError_: {
        type: Object,
        value: null,
      },

      /**
       * Multi-device page data used to determine if the tether section should
       * be shown or not.
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
    };
  }

  /** @override */
  constructor() {
    super();

    /** @private {!chromeos.networkConfig.mojom.CrosNetworkConfigRemote} */
    this.networkConfig_ =
        MojoInterfaceProviderImpl.getInstance().getMojoServiceRemote();
    this.fetchEuiccAndESimPendingProfileList_();
  }

  /** @override */
  ready() {
    super.ready();

    this.addEventListener('install-profile', this.installProfile_);

    this.addWebUIListener(
        'settings.updateMultidevicePageContentData',
        this.onMultiDevicePageContentDataChanged_.bind(this));

    const browserProxy = MultiDeviceBrowserProxyImpl.getInstance();
    browserProxy.getPageContentData().then(
        this.onMultiDevicePageContentDataChanged_.bind(this));
  }

  /**
   * @param {!ash.cellularSetup.mojom.EuiccRemote} euicc
   * ESimManagerListenerBehavior override
   */
  onProfileListChanged(euicc) {
    this.fetchESimPendingProfileListForEuicc_(euicc);
  }

  /**
   * ESimManagerListenerBehavior override
   */
  onAvailableEuiccListChanged() {
    this.fetchEuiccAndESimPendingProfileList_();
  }

  /**
   * @param {!ash.cellularSetup.mojom.ESimProfileRemote} profile
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
              ash.cellularSetup.mojom.ProfileState.kInstalling ?
          NetworkList.CustomItemType.ESIM_INSTALLING_PROFILE :
          NetworkList.CustomItemType.ESIM_PENDING_PROFILE;
    });
  }

  /** @private */
  fetchEuiccAndESimPendingProfileList_() {
    getEuicc().then(euicc => {
      if (!euicc) {
        return;
      }
      this.euicc_ = euicc;

      // Restricting managed cellular network should not show pending eSIM
      // profiles.
      if (this.globalPolicy &&
          this.globalPolicy.allowOnlyPolicyCellularNetworks) {
        this.eSimPendingProfileItems_ = [];
        return;
      }

      this.fetchESimPendingProfileListForEuicc_(euicc);
    });
  }

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
  }

  /**
   * @param {!ash.cellularSetup.mojom.EuiccRemote} euicc
   * @private
   */
  fetchESimPendingProfileListForEuicc_(euicc) {
    getPendingESimProfiles(euicc).then(
        this.processESimPendingProfiles_.bind(this));
  }

  /**
   * @param {Array<!ash.cellularSetup.mojom.ESimProfileRemote>} profiles
   * @private
   */
  processESimPendingProfiles_(profiles) {
    this.profilesMap_ = new Map();
    const eSimPendingProfilePromises =
        profiles.map(this.createESimPendingProfilePromise_.bind(this));
    Promise.all(eSimPendingProfilePromises).then(eSimPendingProfileItems => {
      this.eSimPendingProfileItems_ = eSimPendingProfileItems;
    });
  }

  /**
   * @param {!ash.cellularSetup.mojom.ESimProfileRemote} profile
   * @return {!Promise<NetworkList.CustomItemState>}
   * @private
   */
  createESimPendingProfilePromise_(profile) {
    return profile.getProperties().then(response => {
      this.profilesMap_.set(response.properties.iccid, profile);
      return this.createESimPendingProfileItem_(response.properties);
    });
  }

  /**
   * @param {!ash.cellularSetup.mojom.ESimProfileProperties} properties
   * @return {NetworkList.CustomItemState}
   */
  createESimPendingProfileItem_(properties) {
    return {
      customItemType: properties.state ===
              ash.cellularSetup.mojom.ProfileState.kInstalling ?
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
  }

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
  }

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
  }

  /**
   * @param {!OncMojo.DeviceStateProperties|undefined} cellularDeviceState
   * @returns {boolean}
   * @private
   */
  shouldShowPSimSection_(pSimNetworks, cellularDeviceState) {
    const {pSimSlots} = getSimSlotCount(cellularDeviceState);
    if (pSimSlots > 0) {
      return true;
    }
    // Dual MBIM currently doesn't support eSIM hotswap (b/229619768), which
    // leads Hermes to always show two Eids after swap with pSIM. So, we should
    // also check if there's pSimNetworks available to work around this
    // limitation.
    return this.shouldShowNetworkSublist_(pSimNetworks);
  }

  /**
   * @param {!MultiDevicePageContentData} newData
   * @private
   */
  onMultiDevicePageContentDataChanged_(newData) {
    this.multiDevicePageContentData_ = newData;
  }

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
  }

  /**
   * @param {Event} event
   * @private
   */
  onEsimLearnMoreClicked_(event) {
    event.detail.event.preventDefault();
    event.stopPropagation();

    const showCellularSetupEvent = new CustomEvent('show-cellular-setup', {
      bubbles: true,
      composed: true,
      detail: {pageName: CellularSetupPageName.ESIM_FLOW_UI},
    });
    this.dispatchEvent(showCellularSetupEvent);
  }

  /**
   * @param {!Event} e
   * @private
   */
  onESimDotsClick_(e) {
    const menu = /** @type {!CrActionMenuElement} */ (
        this.shadowRoot.querySelector('#menu').get());
    menu.showAt(/** @type {!HTMLElement} */ (e.target));
  }

  /** @private */
  onShowEidDialogTap_() {
    const actionMenu =
        /** @type {!CrActionMenuElement} */ (
            this.shadowRoot.querySelector('cr-action-menu'));
    actionMenu.close();
    this.shouldShowEidDialog_ = true;
  }

  /** @private */
  onCloseEidDialog_() {
    this.shouldShowEidDialog_ = false;
  }

  /**
   * @param {Event} event
   * @private
   */
  installProfile_(event) {
    if (!this.isConnectedToNonCellularNetwork) {
      const event = new CustomEvent('show-error-toast', {
        bubbles: true,
        composed: true,
        detail: this.i18n('eSimNoConnectionErrorToast'),
      });
      this.dispatchEvent(event);
      return;
    }
    this.installingESimProfile_ = this.profilesMap_.get(event.detail.iccid);
    this.installingESimProfile_.installProfile('').then((response) => {
      if (response.result ===
          ash.cellularSetup.mojom.ProfileInstallResult.kSuccess) {
        this.eSimProfileInstallError_ = null;
        this.installingESimProfile_ = null;
      } else {
        this.eSimProfileInstallError_ = response.result;
        this.showInstallErrorDialog_();
      }
    });
  }

  /** @private */
  showInstallErrorDialog_() {
    this.shouldShowInstallErrorDialog_ = true;
  }

  /** @private */
  onCloseInstallErrorDialog_() {
    this.shouldShowInstallErrorDialog_ = false;
  }

  /**
   * @param {!OncMojo.DeviceStateProperties|undefined} cellularDeviceState
   * @return {boolean}
   * @private
   */
  shouldShowAddESimButton_(cellularDeviceState) {
    assert(!!this.euicc_);
    return this.deviceIsEnabled_(cellularDeviceState);
  }

  /**
   * Return true if the add cellular button should be disabled.
   * @param {!OncMojo.DeviceStateProperties|undefined} cellularDeviceState
   * @param {!chromeos.networkConfig.mojom.GlobalPolicy} globalPolicy
   * @return {boolean}
   * @private
   */
  isAddESimButtonDisabled_(cellularDeviceState, globalPolicy) {
    if (this.isDeviceInhibited_) {
      return true;
    }
    if (!this.deviceIsEnabled_(cellularDeviceState)) {
      return true;
    }
    if (!globalPolicy) {
      return false;
    }
    return globalPolicy.allowOnlyPolicyCellularNetworks;
  }

  /**
   * Return true if the policy indicator that next to the add cellular button
   * should be shown. This policy icon indicates the reason of disabling the
   * add cellular button.
   * @param {!chromeos.networkConfig.mojom.GlobalPolicy} globalPolicy
   * @return {boolean}
   * @private
   */
  shouldShowAddESimPolicyIcon_(globalPolicy) {
    return globalPolicy && globalPolicy.allowOnlyPolicyCellularNetworks;
  }

  /**
   * @param {!OncMojo.DeviceStateProperties|undefined} cellularDeviceState
   * @return {boolean} True if the device is enabled.
   * @private
   */
  deviceIsEnabled_(cellularDeviceState) {
    const mojom = chromeos.networkConfig.mojom;
    return !!cellularDeviceState &&
        cellularDeviceState.deviceState === mojom.DeviceStateType.kEnabled;
  }

  /**
   * @return {boolean}
   * @private
   */
  computeIsDeviceInhibited_() {
    if (!this.cellularDeviceState) {
      return false;
    }
    return OncMojo.deviceIsInhibited(this.cellularDeviceState);
  }

  /** @private */
  onAddEsimButtonTap_() {
    const event = new CustomEvent('show-cellular-setup', {
      bubbles: true,
      composed: true,
      detail: {pageName: CellularSetupPageName.ESIM_FLOW_UI},
    });
    this.dispatchEvent(event);
  }

  /*
   * Returns the add esim button. If the device does not have an EUICC, no eSIM
   * slot, or policies prohibit users from adding a network, null is returned.
   * @return {?HTMLElement}
   */
  getAddEsimButton() {
    return /** @type {?HTMLElement} */ (
        this.shadowRoot.querySelector('#addESimButton'));
  }

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
  }

  /**
   * Return true if the "No available eSIM profiles" subtext message or
   * download eSIM profile link should be shown in eSIM section. This message
   * should not be shown when adding new eSIM profiles.
   * @return {boolean}
   * @private
   */
  shouldShowNoESimMessageOrDownloadLink_(
      inhibitReason, eSimNetworks, eSimPendingProfiles) {
    const mojom = chromeos.networkConfig.mojom.InhibitReason;
    if (inhibitReason === mojom.kInstallingProfile) {
      return false;
    }

    return !this.shouldShowNetworkSublist_(eSimNetworks, eSimPendingProfiles);
  }

  /**
   * Return true if the "No available eSIM profiles" subtext message should be
   * shown in eSIM section. This message should not be shown when the download
   * eSIM profile link is shown.
   * @return {boolean}
   * @private
   */
  shouldShowNoESimSubtextMessage_() {
    if (this.globalPolicy &&
        this.globalPolicy.allowOnlyPolicyCellularNetworks) {
      return true;
    }

    return false;
  }
}

customElements.define(
    CellularNetworksListElement.is, CellularNetworksListElement);
