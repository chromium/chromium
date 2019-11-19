// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Polymer element for displaying a summary of network states
 * by type: Ethernet, WiFi, Cellular, and VPN.
 */

(function() {

const mojom = chromeos.networkConfig.mojom;

Polymer({
  is: 'network-summary',

  behaviors: [
    NetworkListenerBehavior,
  ],

  properties: {
    /**
     * Highest priority connected network or null. Set here to update
     * internet-page which updates internet-subpage and internet-detail-page.
     * @type {?OncMojo.NetworkStateProperties}
     */
    defaultNetwork: {
      type: Object,
      value: null,
      notify: true,
    },

    /**
     * The device state for each network device type. We initialize this to
     * include a disabled WiFi type since WiFi is always present. This reduces
     * the amount of visual change on first load.
     * @private {!Object<!OncMojo.DeviceStateProperties>}
     */
    deviceStates: {
      type: Object,
      value: function() {
        const result = {};
        result[chromeos.networkConfig.mojom.NetworkType.kWiFi] = {
          deviceState: chromeos.networkConfig.mojom.DeviceStateType.kDisabled,
          type: chromeos.networkConfig.mojom.NetworkType.kWiFi,
        };
        return result;
      },
      notify: true,
    },

    /**
     * Array of active network states, one per device type. Initialized to
     * include a default WiFi state (see deviceStates comment).
     * @private {!Array<!OncMojo.NetworkStateProperties>}
     */
    activeNetworkStates_: {
      type: Array,
      value: function() {
        return [OncMojo.getDefaultNetworkState(mojom.NetworkType.kWiFi)];
      },
    },

    /**
     * List of network state data for each network type.
     * @private {!Object<!Array<!OncMojo.NetworkStateProperties>>}
     */
    networkStateLists_: {
      type: Object,
      value: function() {
        const result = {};
        result[mojom.NetworkType.kWiFi] = [];
        return result;
      },
    },
  },

  /** @private {?chromeos.networkConfig.mojom.CrosNetworkConfigRemote} */
  networkConfig_: null,

  /**
   * Set of GUIDs identifying active networks, one for each type.
   * @private {?Set<string>}
   */
  activeNetworkIds_: null,

  /** @override */
  created: function() {
    this.networkConfig_ = network_config.MojoInterfaceProviderImpl.getInstance()
                              .getMojoServiceRemote();
  },

  /** @override */
  attached: function() {
    this.getNetworkLists_();
  },

  /**
   * CrosNetworkConfigObserver impl
   * Updates any matching existing active networks. Note: newly active networks
   * will trigger onNetworkStateListChanged which triggers getNetworkLists_.
   * @param {!Array<OncMojo.NetworkStateProperties>} networks
   */
  onActiveNetworksChanged: function(networks) {
    if (!this.activeNetworkIds_) {
      // Initial list of networks not received yet.
      return;
    }
    networks.forEach(network => {
      const index = this.activeNetworkStates_.findIndex(
          state => state.guid == network.guid);
      if (index != -1) {
        this.set(['activeNetworkStates_', index], network);
      }
    });
  },

  /** CrosNetworkConfigObserver impl */
  onNetworkStateListChanged: function() {
    this.getNetworkLists_();
  },

  /** CrosNetworkConfigObserver impl */
  onDeviceStateListChanged: function() {
    this.getNetworkLists_();
  },

  /**
   * Requests the list of device states and network states from Chrome.
   * Updates deviceStates, activeNetworkStates, and networkStateLists once the
   * results are returned from Chrome.
   * @private
   */
  getNetworkLists_: function() {
    // First get the device states.
    this.networkConfig_.getDeviceStateList().then(response => {
      // Second get the network states.
      this.getNetworkStates_(response.result);
    });
  },

  /**
   * Requests the list of network states from Chrome. Updates
   * activeNetworkStates and networkStateLists once the results are returned
   * from Chrome.
   * @param {!Array<!OncMojo.DeviceStateProperties>} deviceStateList
   * @private
   */
  getNetworkStates_: function(deviceStateList) {
    const filter = {
      filter: chromeos.networkConfig.mojom.FilterType.kVisible,
      limit: chromeos.networkConfig.mojom.NO_LIMIT,
      networkType: mojom.NetworkType.kAll,
    };
    this.networkConfig_.getNetworkStateList(filter).then(response => {
      this.updateNetworkStates_(response.result, deviceStateList);
    });
  },

  /**
   * Called after network states are received from getNetworks.
   * @param {!Array<!OncMojo.NetworkStateProperties>} networkStates The state
   *     properties for all visible networks.
   * @param {!Array<!OncMojo.DeviceStateProperties>} deviceStateList
   * @private
   */
  updateNetworkStates_: function(networkStates, deviceStateList) {
    const newDeviceStates = {};
    for (const device of deviceStateList) {
      newDeviceStates[device.type] = device;
    }

    const orderedNetworkTypes = [
      mojom.NetworkType.kEthernet,
      mojom.NetworkType.kWiFi,
      mojom.NetworkType.kCellular,
      mojom.NetworkType.kTether,
      mojom.NetworkType.kVPN,
    ];

    // Clear any current networks.
    const activeNetworkStatesByType =
        /** @type {!Map<mojom.NetworkType, !OncMojo.NetworkStateProperties>} */
        (new Map);

    // Complete list of states by type.
    const newNetworkStateLists = {};
    for (const type of orderedNetworkTypes) {
      newNetworkStateLists[type] = [];
    }

    let firstConnectedNetwork = null;
    networkStates.forEach(function(networkState) {
      const type = networkState.type;
      if (!activeNetworkStatesByType.has(type)) {
        activeNetworkStatesByType.set(type, networkState);
        if (!firstConnectedNetwork &&
            networkState.type != mojom.NetworkType.kVPN &&
            OncMojo.connectionStateIsConnected(networkState.connectionState)) {
          firstConnectedNetwork = networkState;
        }
      }
      newNetworkStateLists[type].push(networkState);
    }, this);

    this.defaultNetwork = firstConnectedNetwork;

    // Create a VPN entry in deviceStates if there are any VPN networks.
    if (newNetworkStateLists[mojom.NetworkType.kVPN].length > 0) {
      newDeviceStates[mojom.NetworkType.kVPN] = {
        type: mojom.NetworkType.kVPN,
        deviceState: chromeos.networkConfig.mojom.DeviceStateType.kEnabled,
      };
    }

    // Push the active networks onto newActiveNetworkStates in order based on
    // device priority, creating an empty state for devices with no networks.
    const newActiveNetworkStates = [];
    this.activeNetworkIds_ = new Set;
    for (const type of orderedNetworkTypes) {
      const device = newDeviceStates[type];
      if (!device) {
        continue;  // The technology for this device type is unavailable.
      }

      // If both 'Tether' and 'Cellular' technologies exist, merge the network
      // lists and do not add an active network for 'Tether' so that there is
      // only one 'Mobile data' section / subpage.
      if (type == mojom.NetworkType.kTether &&
          newDeviceStates[mojom.NetworkType.kCellular]) {
        newNetworkStateLists[mojom.NetworkType.kCellular] =
            newNetworkStateLists[mojom.NetworkType.kCellular].concat(
                newNetworkStateLists[mojom.NetworkType.kTether]);
        continue;
      }

      // Note: The active state for 'Cellular' may be a Tether network if both
      // types are enabled but no Cellular network exists (edge case).
      const networkState =
          this.getActiveStateForType_(activeNetworkStatesByType, type);
      if (networkState.source == mojom.OncSource.kNone &&
          device.deviceState == mojom.DeviceStateType.kProhibited) {
        // Prohibited technologies are enforced by the device policy.
        networkState.source =
            chromeos.networkConfig.mojom.OncSource.kDevicePolicy;
      }
      newActiveNetworkStates.push(networkState);
      this.activeNetworkIds_.add(networkState.guid);
    }

    this.deviceStates = newDeviceStates;
    this.networkStateLists_ = newNetworkStateLists;
    // Set activeNetworkStates last to rebuild the dom-repeat.
    this.activeNetworkStates_ = newActiveNetworkStates;
  },

  /**
   * Returns the active network state for |type| or a default network state.
   * If there is no 'Cellular' network, return the active 'Tether' network if
   * any since the two types are represented by the same section / subpage.
   * @param {!Map<mojom.NetworkType, !OncMojo.NetworkStateProperties>}
   *     activeStatesByType
   * @param {!mojom.NetworkType} type
   * @return {!OncMojo.NetworkStateProperties|undefined}
   * @private
   */
  getActiveStateForType_: function(activeStatesByType, type) {
    let activeState = activeStatesByType.get(type);
    if (!activeState && type == mojom.NetworkType.kCellular) {
      activeState = activeStatesByType.get(mojom.NetworkType.kTether);
    }
    return activeState || OncMojo.getDefaultNetworkState(type);
  },

  /**
   * Provides an id string for summary items. Used in tests.
   * @param {!OncMojo.NetworkStateProperties} network
   * @return {string}
   * @private
   */
  getTypeString_: function(network) {
    return OncMojo.getNetworkTypeString(network.type);
  },

  /**
   * @param {!Object<!OncMojo.DeviceStateProperties>} deviceStates
   * @return {!OncMojo.DeviceStateProperties|undefined}
   * @private
   */
  getTetherDeviceState_: function(deviceStates) {
    return this.deviceStates[mojom.NetworkType.kTether];
  },
});
})();
