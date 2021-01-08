// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Polymer element for displaying a summary of Cellular network
 * states
 */

Polymer({
  is: 'cellular-networks-list',

  behaviors: [
    ESimManagerListenerBehavior,
    I18nBehavior,
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
     * Device state for the network type.
     * @type {!OncMojo.DeviceStateProperties|undefined}
     */
    deviceState: Object,

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
    shouldShowEidPopup_: {
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
    }
  },

  listeners: {
    'close-eid-popup': 'toggleEidPopup_',
    'install-profile': 'installProfile_',
  },

  /** @private {?chromeos.networkConfig.mojom.CrosNetworkConfigRemote} */
  networkConfig_: null,

  /** @override */
  created() {
    this.networkConfig_ = network_config.MojoInterfaceProviderImpl.getInstance()
                              .getMojoServiceRemote();
    this.fetchESimPendingProfileList_();
  },

  /**
   * @param {!chromeos.cellularSetup.mojom.EuiccRemote} euicc
   * ESimManagerListenerBehavior override
   */
  onProfileListChanged(euicc) {
    this.fetchESimPendingProfileListForEuicc_(euicc);
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
    cellular_setup.getESimManagerRemote()
        .getAvailableEuiccs()
        .then(response => {
          if (response.euiccs.length > 0) {
            // Use first available euicc as current. Only single Euicc modules are
            // currently supported.
            this.euicc_ = response.euiccs[0];
            return this.fetchESimPendingProfileListForEuicc_(this.euicc_);
          }
          this.euicc_ = null;
        });
  },

  /**
   * @param {!chromeos.cellularSetup.mojom.EuiccRemote} euicc
   * @private
   */
  fetchESimPendingProfileListForEuicc_(euicc) {
    cellular_setup.getPendingESimProfiles(euicc).then(
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
  async onNetworksListChanged_() {
    const mojom = chromeos.networkConfig.mojom;

    const pSimNetworks = [];
    const eSimNetworks = [];
    const tetherNetworks = [];

    for (const network of this.networks) {
      if (network.type === mojom.NetworkType.kTether) {
        tetherNetworks.push(network);
        continue;
      }

      const managedPropertiesResponse =
          await this.networkConfig_.getManagedProperties(network.guid);
      if (!managedPropertiesResponse || !managedPropertiesResponse.result) {
        console.error(
            'Unable to get managed properties for network. guid=',
            network.guid);
        continue;
      }

      if (managedPropertiesResponse.result.typeProperties.cellular.eid) {
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
   * @param {Event} event
   * @private
   */
  onEsimLearnMoreClicked_(event) {
    event.detail.event.preventDefault();
    event.stopPropagation();

    this.fire(
        'show-cellular-setup',
        {pageName: cellularSetup.CellularSetupPageName.ESIM_FLOW_UI});
  },

  /**
   * @param {Event} event
   * @private
   */
  onPsimLearnMoreClicked_(event) {
    event.detail.event.preventDefault();
    event.stopPropagation();

    this.fire(
        'show-cellular-setup',
        {pageName: cellularSetup.CellularSetupPageName.PSIM_FLOW_UI});
  },


  /** @private */
  toggleEidPopup_() {
    this.shouldShowEidPopup_ = !this.shouldShowEidPopup_;

    if (this.shouldShowEidPopup_) {
      Polymer.RenderStatus.afterNextRender(this, () => {
        this.$$('.eid-popup').focus();
      });
    }
  },

  /**
   * @param {Event} event
   * @private
   */
  installProfile_(event) {
    const profileIccid = event.detail.iccid;
    const profile = this.profilesMap_.get(profileIccid);
    profile.installProfile('').then(
        () => {
            // TODO(crbug.com/1093185) Show error if install fails.
            // Show confirmation code page if required.
        });
  },
});
