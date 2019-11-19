// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * This element provides a layer between the settings-multidevice-subpage
 * element and the internet_page folder's network-summary-item. It is
 * responsible for loading initial tethering network data from the
 * networkConfig mojo API as well as updating the data in real time. It
 * serves a role comparable to the internet_page's network-summary element.
 */

Polymer({
  is: 'settings-multidevice-tether-item',

  behaviors: [
    NetworkListenerBehavior,
    MultiDeviceFeatureBehavior,
  ],

  properties: {
    /**
     * The device state for tethering.
     * @private {?OncMojo.DeviceStateProperties|undefined}
     */
    deviceState_: Object,

    /**
     * The network state for a potential tethering host phone. Note that there
     * is at most one because only one MultiDevice host phone is allowed on an
     * account at a given time.
     * @private {?OncMojo.NetworkStateProperties|undefined}
     */
    activeNetworkState_: Object,

    /**
     * Alias for allowing Polymer bindings to settings.routes.
     * @type {?SettingsRoutes}
     */
    routes: {
      type: Object,
      value: settings.routes,
    },

    /**
     * Whether to show technology badge on mobile network icon.
     * @private
     */
    showTechnologyBadge_: {
      type: Boolean,
      value: function() {
        return loadTimeData.valueExists('showTechnologyBadge') &&
            loadTimeData.getBoolean('showTechnologyBadge');
      }
    },
  },

  /** @private {?chromeos.networkConfig.mojom.CrosNetworkConfigRemote} */
  networkConfig_: null,

  /** @override */
  created: function() {
    this.networkConfig_ = network_config.MojoInterfaceProviderImpl.getInstance()
                              .getMojoServiceRemote();
  },

  /** @override */
  attached: function() {
    this.updateTetherDeviceState_();
    this.updateTetherNetworkState_();
  },

  /**
   * CrosNetworkConfigObserver impl
   * Note that any change to leading to a new active network will also trigger
   * onNetworkStateListChanged, triggering updateTetherNetworkState_ and
   * rendering this callback redundant. As a result, we return early if the
   * active network is not changed.
   * @param {!Array<chromeos.networkConfig.mojom.NetworkStateProperties>}
   *     networks
   * @private
   */
  onActiveNetworksChanged: function(networks) {
    const guid = this.activeNetworkState_.guid;
    if (!networks.find(network => network.guid == guid)) {
      return;
    }
    this.networkConfig_.getNetworkState(guid).then(response => {
      if (response.result) {
        this.activeNetworkState_ = response.result;
      }
    });
  },

  /** CrosNetworkConfigObserver impl */
  onNetworkStateListChanged: function() {
    this.updateTetherNetworkState_();
  },

  /** CrosNetworkConfigObserver impl */
  onDeviceStateListChanged: function() {
    this.updateTetherDeviceState_();
  },

  /**
   * Retrieves device states (OncMojo.DeviceStateProperties) and sets
   * this.deviceState_ to the retrieved Tether device state (or undefined if
   * there is none). Note that crosNetworkConfig.getDeviceStateList retrieves at
   * most one device per NetworkType so there will be at most one Tether device
   * state.
   * @private
   */
  updateTetherDeviceState_: function() {
    this.networkConfig_.getDeviceStateList().then(response => {
      const kTether = chromeos.networkConfig.mojom.NetworkType.kTether;
      const deviceStates = response.result;
      const deviceState =
          deviceStates.find(deviceState => deviceState.type == kTether);
      this.deviceState_ = deviceState || {
        deviceState: chromeos.networkConfig.mojom.DeviceStateType.kDisabled,
        managedNetworkAvailable: false,
        scanning: false,
        simAbsent: false,
        type: kTether,
      };
    });
  },

  /**
   * Retrieves all Instant Tethering network states
   * (OncMojo.NetworkStateProperties). Note that there is at most one because
   * only one host is allowed on an account at a given time. Then it sets
   * this.activeNetworkState_ to that network if there is one or a dummy object
   * with an empty string for a GUID otherwise.
   * @private
   */
  updateTetherNetworkState_: function() {
    const kTether = chromeos.networkConfig.mojom.NetworkType.kTether;
    const filter = {
      filter: chromeos.networkConfig.mojom.FilterType.kVisible,
      limit: 1,
      networkType: kTether,
    };
    this.networkConfig_.getNetworkStateList(filter).then(response => {
      const networks = response.result;
      this.activeNetworkState_ =
          networks[0] || OncMojo.getDefaultNetworkState(kTether);
    });
  },

  /**
   * Returns an array containing the active network state if there is one
   * (note that if there is not GUID will be falsy).  Returns an empty array
   * otherwise.
   * @return {!Array<chromeos.networkConfig.mojom.NetworkStateProperties>}
   * @private
   */
  getNetworkStateList_: function() {
    return this.activeNetworkState_.guid ? [this.activeNetworkState_] : [];
  },

  /**
   * @return {!URLSearchParams}
   * @private
   */
  getTetherNetworkUrlSearchParams_: function() {
    return new URLSearchParams('type=Tether');
  },
});
