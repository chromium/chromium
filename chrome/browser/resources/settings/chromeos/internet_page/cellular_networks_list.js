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
   * @param {!Array<!OncMojo.NetworkStateProperties>} list
   * @returns {boolean}
   * @private
   */
  shouldShowNetworkSublist_(list) {
    return list.length > 0;
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
