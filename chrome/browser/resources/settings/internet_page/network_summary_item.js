// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Polymer element for displaying the network state for a specific
 * type and a list of networks for that type. NOTE: It both Cellular and Tether
 * technologies are available, they are combined into a single 'Mobile data'
 * section. See crbug.com/726380.
 */

(function() {

const mojom = chromeos.networkConfig.mojom;

Polymer({
  is: 'network-summary-item',

  behaviors: [
    CrPolicyNetworkBehaviorMojo,
    I18nBehavior,
  ],

  properties: {
    /**
     * Device state for the network type. This might briefly be undefined if
     * a device becomes unavailable.
     * @type {!OncMojo.DeviceStateProperties|undefined}
     */
    deviceState: {
      type: Object,
      notify: true,
    },

    /**
     * If both Cellular and Tether technologies exist, we combine the
     * sections and set this to the device state for Tether.
     * @type {!OncMojo.DeviceStateProperties|undefined}
     */
    tetherDeviceState: Object,

    /**
     * Network state for the active network.
     * @type {!OncMojo.NetworkStateProperties|undefined}
     */
    activeNetworkState: Object,

    /**
     * List of all network state data for the network type.
     * @type {!Array<!OncMojo.NetworkStateProperties>}
     */
    networkStateList: {
      type: Array,
      value: function() {
        return [];
      },
    },

    /**
     * Title line describing the network type to appear in the row's top
     * line. If it is undefined, the title text is a default from
     * CrOncStrings (see this.getTitleText_() below).
     * @type {string|undefined}
     */
    networkTitleText: String,

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

  /**
   * @param {!OncMojo.NetworkStateProperties} activeNetworkState
   * @param {!OncMojo.DeviceStateProperties|undefined} deviceState
   * @return {string}
   * @private
   */
  getNetworkStateText_: function(activeNetworkState, deviceState) {
    const stateText =
        this.getConnectionStateText_(activeNetworkState, deviceState);
    if (stateText) {
      return stateText;
    }
    // No network state, use device state.
    if (deviceState) {
      // Type specific scanning or initialization states.
      if (deviceState.type == mojom.NetworkType.kCellular) {
        if (deviceState.scanning) {
          return this.i18n('internetMobileSearching');
        }
        if (deviceState.deviceState == mojom.DeviceStateType.kUninitialized) {
          return this.i18n('internetDeviceInitializing');
        }
      } else if (deviceState.type == mojom.NetworkType.kTether) {
        if (deviceState.deviceState == mojom.DeviceStateType.kUninitialized) {
          return this.i18n('tetherEnableBluetooth');
        }
      }
      // Enabled or enabling states.
      if (deviceState.deviceState == mojom.DeviceStateType.kEnabled) {
        if (this.networkStateList.length > 0) {
          return CrOncStrings.networkListItemNotConnected;
        }
        return CrOncStrings.networkListItemNoNetwork;
      }
      if (deviceState.deviceState == mojom.DeviceStateType.kEnabling) {
        return this.i18n('internetDeviceEnabling');
      }
    }
    // No device or unknown device state, use 'off'.
    return this.i18n('deviceOff');
  },

  /**
   * @param {!OncMojo.NetworkStateProperties} networkState
   * @param {!OncMojo.DeviceStateProperties|undefined} deviceState
   * @return {string}
   * @private
   */
  getConnectionStateText_: function(networkState, deviceState) {
    if (!networkState || !networkState.guid) {
      return '';
    }
    const connectionState = networkState.connectionState;
    const name = OncMojo.getNetworkStateDisplayName(networkState);
    if (OncMojo.connectionStateIsConnected(connectionState)) {
      // Ethernet networks always have the display name 'Ethernet' so we use the
      // state text 'Connected' to avoid repeating the label in the sublabel.
      // See http://crbug.com/989907 for details.
      return networkState.type == mojom.NetworkType.kEthernet ?
          CrOncStrings.networkListItemConnected :
          name;
    }
    if (connectionState == mojom.ConnectionStateType.kConnecting) {
      return name ?
          CrOncStrings.networkListItemConnectingTo.replace('$1', name) :
          CrOncStrings.networkListItemConnecting;
    }
    if (networkState.type == mojom.NetworkType.kCellular && deviceState &&
        deviceState.scanning) {
      return this.i18n('internetMobileSearching');
    }
    return CrOncStrings.networkListItemNotConnected;
  },

  /**
   * @param {!OncMojo.NetworkStateProperties} activeNetworkState
   * @return {boolean}
   * @private
   */
  showPolicyIndicator_: function(activeNetworkState) {
    return (activeNetworkState !== undefined &&
            OncMojo.connectionStateIsConnected(
                activeNetworkState.connectionState)) ||
        this.isPolicySource(activeNetworkState.source);
  },

  /**
   * @param {!OncMojo.DeviceStateProperties|undefined} deviceState
   * @return {boolean}
   * @private
   */
  showSimInfo_: function(deviceState) {
    if (!deviceState || deviceState.type != mojom.NetworkType.kCellular) {
      return false;
    }
    return this.simLockedOrAbsent_(deviceState);
  },

  /**
   * @param {!OncMojo.DeviceStateProperties} deviceState
   * @return {boolean}
   * @private
   */
  simLockedOrAbsent_: function(deviceState) {
    if (this.deviceIsEnabled_(deviceState)) {
      return false;
    }
    if (!deviceState.simLockStatus) {
      return false;
    }
    if (deviceState.simLockStatus.simAbsent) {
      return true;
    }
    const simLockType = deviceState.simLockStatus.lockType;
    return simLockType == 'sim-pin' || simLockType == 'sim-puk';
  },

  /**
   * @param {!OncMojo.DeviceStateProperties|undefined} deviceState
   * @return {boolean} Whether or not the device state is enabled.
   * @private
   */
  deviceIsEnabled_: function(deviceState) {
    return !!deviceState &&
        deviceState.deviceState == mojom.DeviceStateType.kEnabled;
  },

  /**
   * @param {!OncMojo.DeviceStateProperties|undefined} deviceState
   * @return {boolean}
   * @private
   */
  enableToggleIsVisible_: function(deviceState) {
    if (!deviceState) {
      return false;
    }
    switch (deviceState.type) {
      case mojom.NetworkType.kEthernet:
      case mojom.NetworkType.kVPN:
        return false;
      case mojom.NetworkType.kTether:
        return true;
      case mojom.NetworkType.kWiFi:
        return deviceState.deviceState != mojom.DeviceStateType.kUninitialized;
      case mojom.NetworkType.kCellular:
        return deviceState.deviceState !=
            mojom.DeviceStateType.kUninitialized &&
            !this.simLockedOrAbsent_(deviceState);
    }
    assertNotReached();
    return false;
  },

  /**
   * @param {!OncMojo.DeviceStateProperties|undefined} deviceState
   * @return {boolean}
   * @private
   */
  enableToggleIsEnabled_: function(deviceState) {
    return this.enableToggleIsVisible_(deviceState) &&
        deviceState.deviceState != mojom.DeviceStateType.kProhibited &&
        deviceState.deviceState != mojom.DeviceStateType.kUninitialized;
  },

  /**
   * @param {!OncMojo.DeviceStateProperties|undefined} deviceState
   * @return {string}
   * @private
   */
  getToggleA11yString_: function(deviceState) {
    if (!this.enableToggleIsVisible_(deviceState)) {
      return '';
    }
    switch (deviceState.type) {
      case mojom.NetworkType.kTether:
      case mojom.NetworkType.kCellular:
        return this.i18n('internetToggleMobileA11yLabel');
      case mojom.NetworkType.kWiFi:
        return this.i18n('internetToggleWiFiA11yLabel');
    }
    assertNotReached();
    return '';
  },

  /**
   * @param {!OncMojo.NetworkStateProperties} activeNetworkState
   * @param {!OncMojo.DeviceStateProperties|undefined} deviceState
   * @param {!Array<!OncMojo.NetworkStateProperties>} networkStateList
   * @return {boolean}
   * @private
   */
  showDetailsIsVisible_: function(
      activeNetworkState, deviceState, networkStateList) {
    return this.deviceIsEnabled_(deviceState) &&
        (!!activeNetworkState.guid || networkStateList.length > 0);
  },

  /**
   * @param {!OncMojo.DeviceStateProperties|undefined} deviceState
   * @param {!Array<!OncMojo.NetworkStateProperties>} networkStateList
   * @return {boolean}
   * @private
   */
  shouldShowSubpage_: function(deviceState, networkStateList) {
    if (!deviceState) {
      return false;
    }
    const type = deviceState.type;
    if (type == mojom.NetworkType.kTether ||
        (type == mojom.NetworkType.kCellular && this.tetherDeviceState)) {
      // The "Mobile data" subpage should always be shown if Tether is
      // available, even if there are currently no associated networks.
      return true;
    }
    let minlen;
    if (type == mojom.NetworkType.kVPN) {
      // VPN subpage provides provider info so show if there are any networks.
      minlen = 1;
    } else if (type == mojom.NetworkType.kWiFi) {
      // WiFi subpage includes 'Known Networks' so always show, even if the
      // technology is still enabling / scanning, or none are visible.
      minlen = 0;
    } else {
      // By default, only show the subpage if there are 2+ networks
      minlen = 2;
    }
    return networkStateList.length >= minlen;
  },

  /**
   * @param {!Event} event The enable button event.
   * @private
   */
  onShowDetailsTap_: function(event) {
    if (!this.deviceIsEnabled_(this.deviceState)) {
      if (this.enableToggleIsEnabled_(this.deviceState)) {
        const type = this.deviceState.type;
        this.fire('device-enabled-toggled', {enabled: true, type: type});
      }
    } else if (this.shouldShowSubpage_(
                   this.deviceState, this.networkStateList)) {
      this.fire('show-networks', this.deviceState.type);
    } else if (this.activeNetworkState.guid) {
      this.fire('show-detail', this.activeNetworkState);
    } else if (this.networkStateList.length > 0) {
      this.fire('show-detail', this.networkStateList[0]);
    }
    event.stopPropagation();
  },

  /**
   * @param {!OncMojo.NetworkStateProperties} activeNetworkState
   * @param {!OncMojo.DeviceStateProperties|undefined} deviceState
   * @param {!Array<!OncMojo.NetworkStateProperties>} networkStateList
   * @return {string}
   * @private
   */
  getDetailsA11yString_: function(
      activeNetworkState, deviceState, networkStateList) {
    let a11yString = this.getNetworkTypeString_(deviceState.type);
    // If the item opens a network detail page directly then also include the
    // network display name in the A11y string to give more context.
    if (!this.shouldShowSubpage_(deviceState, networkStateList)) {
      let displayName;
      if (activeNetworkState.guid) {
        displayName = OncMojo.getNetworkStateDisplayName(activeNetworkState);
      } else if (networkStateList.length > 0) {
        displayName = OncMojo.getNetworkStateDisplayName(networkStateList[0]);
      }
      // Display name could be same as network type string. Only combine the
      // two strings if we have a valid display name.
      if (displayName !== undefined && displayName != a11yString) {
        a11yString = this.i18n(
            'internetSummaryButtonA11yLabel', a11yString, displayName);
      }
    }
    return a11yString;
  },

  /**
   * Event triggered when the enable button is toggled.
   * @param {!Event} event
   * @private
   */
  onDeviceEnabledChange_: function(event) {
    assert(this.deviceState);
    const deviceIsEnabled = this.deviceIsEnabled_(this.deviceState);
    this.fire(
        'device-enabled-toggled',
        {enabled: !deviceIsEnabled, type: this.deviceState.type});
  },

  /**
   * @return {string}
   * @private
   */
  getTitleText_: function() {
    assert(CrOncStrings);
    return this.networkTitleText ||
        this.getNetworkTypeString_(this.activeNetworkState.type);
  },

  /**
   * Make sure events in embedded components do not propagate to onDetailsTap_.
   * @param {!Event} event
   * @private
   */
  doNothing_: function(event) {
    event.stopPropagation();
  },

  /**
   * @param {!mojom.NetworkType} type
   * @return {string}
   * @private
   */
  getNetworkTypeString_: function(type) {
    // The shared Cellular/Tether subpage is referred to as "Mobile".
    // TODO(khorimoto): Remove once Cellular/Tether are split into their own
    // sections.
    if (type == mojom.NetworkType.kCellular ||
        type == mojom.NetworkType.kTether) {
      type = mojom.NetworkType.kMobile;
    }
    return this.i18n('OncType' + OncMojo.getNetworkTypeString(type));
  },
});
})();
