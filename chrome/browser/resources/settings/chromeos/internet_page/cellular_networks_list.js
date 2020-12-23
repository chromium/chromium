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
     * The list of pending eSIM profiles to display after the list of eSIM
     * networks.
     * @type {!Array<NetworkList.CustomItemState>}
     */
    eSimPendingProfiles_: {
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
    }
  },

  listeners: {
    'close-eid-popup': 'toggleEidPopup_',
  },

  /** @private {?chromeos.networkConfig.mojom.CrosNetworkConfigRemote} */
  networkConfig_: null,

  /** @override */
  created() {
    this.networkConfig_ = network_config.MojoInterfaceProviderImpl.getInstance()
                              .getMojoServiceRemote();
    this.fetchESimPendingProfileList_();
  },

  /** @private */
  fetchESimPendingProfileList_() {
    cellular_setup.getESimManagerRemote()
        .getAvailableEuiccs()
        .then(response => {
          if (response.euiccs.length > 0) {
            return cellular_setup.getPendingESimProfiles(response.euiccs[0]);
          }
          throw new Error('No EUICCs available.');
        })
        .then(profiles => {
          const pendingProfilePromises = profiles.map(profile => {
            return profile.getProperties().then(response => {
              return {
                customItemType: NetworkList.CustomItemType.ESIM_PENDING_PROFILE,
                customItemName:
                    String.fromCharCode(...response.properties.name.data),
                customItemSubtitle: String.fromCharCode(
                    ...response.properties.serviceProvider.data),
                polymerIcon: 'network:cellular-0',
                showBeforeNetworksList: false,
                customData: {
                  iccid: response.properties.iccid,
                },
              };
            });
          });
          Promise.all(pendingProfilePromises).then(profiles => {
            this.eSimPendingProfiles_ = profiles;
          });
        })
        .catch(error => {
          console.error(error);
        });
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
  }
});
