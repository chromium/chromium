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
      value() {
        return [];
      },
    },

    /**
     * Title line describing the network type to appear in the row's top
     * line. If it is undefined, the title text is set to a default value.
     * @type {string|undefined}
     */
    networkTitleText: String,

    /**
     * Whether to show technology badge on mobile network icon.
     * @private
     */
    showTechnologyBadge_: {
      type: Boolean,
      value() {
        return loadTimeData.valueExists('showTechnologyBadge') &&
            loadTimeData.getBoolean('showTechnologyBadge');
      }
    },

    /** @private */
    isUpdatedCellularUiEnabled_: {
      type: Boolean,
      value() {
        return loadTimeData.getBoolean('updatedCellularActivationUi');
      }
    },
  },

  /*
   * Returns the device enabled toggle element.
   * @return {?CrToggleElement}
   */
  getDeviceEnabledToggle() {
    return this.$$('#deviceEnabledButton');
  },

  /**
   * @return {string}
   * @private
   */
  getNetworkStateText_() {
    // If SIM Locked, show warning message instead of connection state.
    if (this.shouldShowLockedWarningMessage_(this.deviceState)) {
      return this.i18n('networkSimLockedSubtitle');
    }

    const stateText =
        this.getConnectionStateText_(this.activeNetworkState, this.deviceState);
    if (stateText) {
      return stateText;
    }
    // No network state, use device state.
    const deviceState = this.deviceState;
    if (deviceState) {
      // Type specific scanning or initialization states.
      if (deviceState.type === mojom.NetworkType.kCellular) {
        if (deviceState.scanning) {
          return this.i18n('internetMobileSearching');
        }
        if (deviceState.deviceState === mojom.DeviceStateType.kUninitialized) {
          return this.i18n('internetDeviceInitializing');
        }
        if (deviceState.deviceState === mojom.DeviceStateType.kDisabling) {
          return this.i18n('internetDeviceDisabling');
        }
        if (OncMojo.deviceIsInhibited(deviceState)) {
          return this.i18n('internetDeviceBusy');
        }
      } else if (deviceState.type === mojom.NetworkType.kTether) {
        if (deviceState.deviceState === mojom.DeviceStateType.kUninitialized) {
          return this.i18n('tetherEnableBluetooth');
        }
      }
      // Enabled or enabling states.
      if (deviceState.deviceState === mojom.DeviceStateType.kEnabled) {
        if (this.networkStateList.length > 0) {
          return this.i18n('networkListItemNotConnected');
        }
        return this.i18n('networkListItemNoNetwork');
      }
      if (deviceState.deviceState === mojom.DeviceStateType.kEnabling) {
        return this.i18n('internetDeviceEnabling');
      }
    }
    // No device or unknown device state, use 'off'.
    return this.i18n('deviceOff');
  },

  /**
   * @param {!OncMojo.NetworkStateProperties|undefined} networkState
   * @param {!OncMojo.DeviceStateProperties|undefined} deviceState
   * @return {string}
   * @private
   */
  getConnectionStateText_(networkState, deviceState) {
    if (!networkState || !networkState.guid) {
      return '';
    }
    const connectionState = networkState.connectionState;
    const name = OncMojo.getNetworkStateDisplayName(networkState);
    if (OncMojo.connectionStateIsConnected(connectionState)) {
      // Ethernet networks always have the display name 'Ethernet' so we use the
      // state text 'Connected' to avoid repeating the label in the sublabel.
      // See http://crbug.com/989907 for details.
      return networkState.type === mojom.NetworkType.kEthernet ?
          this.i18n('networkListItemConnected') :
          name;
    }
    if (connectionState === mojom.ConnectionStateType.kConnecting) {
      return name ? this.i18n('networkListItemConnectingTo', name) :
                    this.i18n('networkListItemConnecting');
    }
    if (networkState.type === mojom.NetworkType.kCellular && deviceState) {
      // If there is no cellular SIM and the updated UI flag is disabled,
      // simply display 'Off'. See b/162564761 for details.
      if (deviceState.simAbsent &&
          !loadTimeData.getBoolean('updatedCellularActivationUi')) {
        return this.i18n('deviceOff');
      }
      if (deviceState.scanning) {
        return this.i18n('internetMobileSearching');
      }
    }
    return this.i18n('networkListItemNotConnected');
  },

  /**
   * @param {!OncMojo.NetworkStateProperties} activeNetworkState
   * @return {boolean}
   * @private
   */
  showPolicyIndicator_(activeNetworkState) {
    return (activeNetworkState !== undefined &&
            OncMojo.connectionStateIsConnected(
                activeNetworkState.connectionState)) ||
        this.isPolicySource(activeNetworkState.source) ||
        this.isProhibitedVpn_();
  },

  /**
   * @param {!OncMojo.NetworkStateProperties} activeNetworkState
   * @return {!CrPolicyIndicatorType} Device policy indicator for VPN when
   *     disabled by policy and an indicator corresponding to the source of the
   *     active network state otherwise.
   * @private
   */
  getPolicyIndicatorType_(activeNetworkState) {
    if (this.isProhibitedVpn_()) {
      return this.getIndicatorTypeForSource(
          chromeos.networkConfig.mojom.OncSource.kDevicePolicy);
    }
    return this.getIndicatorTypeForSource(activeNetworkState.source);
  },

  /**
   * @param {!OncMojo.DeviceStateProperties|undefined} deviceState
   * @return {boolean}
   * @private
   */
  showSimInfo_(deviceState) {
    if (!deviceState || deviceState.type !== mojom.NetworkType.kCellular) {
      return false;
    }

    const {pSimSlots, eSimSlots} = getSimSlotCount(deviceState);
    if (this.isUpdatedCellularUiEnabled_ && eSimSlots > 0) {
      // Do not show simInfo if |updatedCellularActivationUi| flag is enabled
      // and if we are using an eSIM enabled device.
      return false;
    }
    return this.simLockedOrAbsent_(deviceState);
  },

  /**
   * @param {!OncMojo.DeviceStateProperties|undefined} deviceState
   * @return {string}
   * @private
   */
  getNetworkStateClass_(deviceState) {
    if (this.shouldShowLockedWarningMessage_(deviceState)) {
      return 'locked-warning-message';
    }
    return 'network-state';
  },

  /**
   * @param {!OncMojo.DeviceStateProperties|undefined} deviceState
   * @return {boolean}
   * @private
   */
  shouldShowLockedWarningMessage_(deviceState) {
    if (!deviceState || deviceState.type !== mojom.NetworkType.kCellular ||
        !deviceState.simLockStatus || !this.isUpdatedCellularUiEnabled_) {
      return false;
    }

    return !!deviceState.simLockStatus.lockType;
  },

  /**
   * @param {!OncMojo.DeviceStateProperties|undefined} deviceState
   * @return {boolean}
   * @private
   */
  simLockedOrAbsent_(deviceState) {
    if (!deviceState) {
      return false;
    }
    if (deviceState.simAbsent) {
      return true;
    }
    if (!deviceState.simLockStatus) {
      return false;
    }
    const simLockType = deviceState.simLockStatus.lockType;
    return simLockType === 'sim-pin' || simLockType === 'sim-puk';
  },

  /**
   * @param {!OncMojo.DeviceStateProperties|undefined} deviceState
   * @return {boolean} True if the device is enabled or if it is a VPN or if
   *     we are in the state of inhibited. Note:
   *     This function will always return true for VPNs because VPNs can be
   *     disabled by policy only for built-in VPNs (OpenVPN & L2TP), but always
   *     enabled for other VPN providers. To know whether built-in VPNs are
   *     disabled, use builtInVpnProhibited_() instead.
   * @private
   */
  deviceIsEnabled_(deviceState) {
    return !!deviceState &&
        (deviceState.type === mojom.NetworkType.kVPN ||
         deviceState.deviceState === mojom.DeviceStateType.kEnabled ||
         OncMojo.deviceIsInhibited(deviceState));
  },

  /**
   * @param {!OncMojo.DeviceStateProperties|undefined} deviceState
   * @return {boolean}
   * @private
   */
  enableToggleIsVisible_(deviceState) {
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
        return deviceState.deviceState !== mojom.DeviceStateType.kUninitialized;
      case mojom.NetworkType.kCellular:
        return (deviceState.deviceState !==
                    mojom.DeviceStateType.kUninitialized &&
                !this.simLockedOrAbsent_(deviceState)) ||
            (this.isUpdatedCellularUiEnabled_ &&
             this.simLockedOrAbsent_(deviceState));
    }
    assertNotReached();
    return false;
  },

  /**
   * @param {!OncMojo.DeviceStateProperties|undefined} deviceState
   * @return {boolean}
   * @private
   */
  enableToggleIsEnabled_(deviceState) {
    return this.enableToggleIsVisible_(deviceState) &&
        deviceState.deviceState !== mojom.DeviceStateType.kProhibited &&
        !OncMojo.deviceIsInhibited(deviceState) &&
        !OncMojo.deviceStateIsIntermediate(deviceState.deviceState);
  },

  /**
   * @param {!OncMojo.DeviceStateProperties|undefined} deviceState
   * @return {string}
   * @private
   */
  getToggleA11yString_(deviceState) {
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
   * @param {!OncMojo.DeviceStateProperties|undefined} deviceState
   * @return {string}
   * @private
   */
  getToggleA11yDescribedBy_(deviceState) {
    // Use network state text to describe toggle for uninitialized tether
    // device. This announces details about enabling bluetooth.
    if (this.enableToggleIsVisible_(deviceState) &&
        deviceState.type === mojom.NetworkType.kTether &&
        deviceState.deviceState === mojom.DeviceStateType.kUninitialized) {
      return 'networkState';
    }
    return '';
  },

  /**
   * @return {boolean} True if VPNs are disabled by policy and the current
   *     device is VPN.
   * @private
   */
  isProhibitedVpn_() {
    return !!this.deviceState &&
        this.deviceState.type === mojom.NetworkType.kVPN &&
        this.builtInVpnProhibited_(this.deviceState);
  },

  /**
   * @param {!chromeos.networkConfig.mojom.VpnType} vpnType
   * @return {boolean}
   * @private
   */
  isBuiltInVpnType_(vpnType) {
    return vpnType === chromeos.networkConfig.mojom.VpnType.kL2TPIPsec ||
        vpnType === chromeos.networkConfig.mojom.VpnType.kOpenVPN;
  },

  /**
   * @param {!Array<!OncMojo.NetworkStateProperties>} networkStateList
   * @return {boolean} True if at least one non-native VPN is configured.
   * @private
   */
  hasNonBuiltInVpn_(networkStateList) {
    const nonBuiltInVpnIndex = networkStateList.findIndex((networkState) => {
      return !this.isBuiltInVpnType_(networkState.typeState.vpn.type);
    });
    return nonBuiltInVpnIndex !== -1;
  },

  /**
   * @param {!OncMojo.DeviceStateProperties|undefined} deviceState
   * @return {boolean} True if the built-in VPNs are disabled by policy.
   * @private
   */
  builtInVpnProhibited_(deviceState) {
    return !!deviceState &&
        deviceState.deviceState ===
        chromeos.networkConfig.mojom.DeviceStateType.kProhibited;
  },

  /**
   * @param {!OncMojo.DeviceStateProperties|undefined} deviceState
   * @param {!Array<!OncMojo.NetworkStateProperties>} networkStateList
   * @return {boolean} True if there is any configured VPN for a non-disabled
   *     VPN provider. Note: Only built-in VPN providers can be disabled by
   *     policy at the moment.
   * @private
   */
  anyVpnExists_(deviceState, networkStateList) {
    return this.hasNonBuiltInVpn_(networkStateList) ||
        (!this.builtInVpnProhibited_(deviceState) &&
         networkStateList.length > 0);
  },

  /**
   * @param {!OncMojo.NetworkStateProperties|undefined} activeNetworkState
   * @param {!OncMojo.DeviceStateProperties|undefined} deviceState
   * @param {!Array<!OncMojo.NetworkStateProperties>} networkStateList
   * @return {boolean}
   * @private
   */
  shouldShowDetails_(activeNetworkState, deviceState, networkStateList) {
    if (!!deviceState && deviceState.type === mojom.NetworkType.kVPN) {
      return this.anyVpnExists_(deviceState, networkStateList);
    }

    return this.deviceIsEnabled_(deviceState) &&
        (!!activeNetworkState.guid || networkStateList.length > 0);
  },

  /**
   * @param {!OncMojo.DeviceStateProperties|undefined} deviceState
   * @param {!Array<!OncMojo.NetworkStateProperties>} networkStateList
   * @return {boolean}
   * @private
   */
  shouldShowSubpage_(deviceState, networkStateList) {
    if (!deviceState) {
      return false;
    }
    const type = deviceState.type;

    if (type === mojom.NetworkType.kTether ||
        (type === mojom.NetworkType.kCellular && this.tetherDeviceState)) {
      // The "Mobile data" subpage should always be shown if Tether is
      // available, even if there are currently no associated networks.
      return true;
    }

    if (type === mojom.NetworkType.kCellular) {
      if (this.isUpdatedCellularUiEnabled_) {
        if (OncMojo.deviceIsInhibited(deviceState)) {
          // The "Mobile data" subpage should be shown if the device state is
          // inhibited and the flag is enabled.
          return true;
        }
        // When network type is Cellular and |updatedCellularActivationUi| is
        // enabled, always show "Mobile data" subpage, when eSim is available
        // or multiple pSimSlots are available
        const {pSimSlots, eSimSlots} = getSimSlotCount(deviceState);
        if (eSimSlots > 0 || pSimSlots > 1) {
          return true;
        }
      } else if (this.simLockedOrAbsent_(deviceState)) {
        // No subpage should be shown if the SIM is locked/absent. The user
        // should unlock their SIM before a subpage is possible.
        return false;
      }
    }

    if (type === mojom.NetworkType.kVPN) {
      return this.anyVpnExists_(deviceState, networkStateList);
    }

    let minlen;
    if (type === mojom.NetworkType.kWiFi) {
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
   * This handles clicking the network summary item row. Clicking this row can
   * lead to toggling device enablement or showing the corresponding networks
   * list or showing details about a network or doing nothing based on the
   * device and networks states.
   * @param {!Event} event The enable button event.
   * @private
   */
  onShowDetailsTap_(event) {
    if (!this.deviceIsEnabled_(this.deviceState)) {
      if (this.enableToggleIsEnabled_(this.deviceState)) {
        const type = this.deviceState.type;
        this.fire('device-enabled-toggled', {enabled: true, type: type});
      }
    } else if (this.shouldShowSubpage_(
                   this.deviceState, this.networkStateList)) {
      this.fire('show-networks', this.deviceState.type);
    } else if (this.shouldShowDetails_(
                   this.activeNetworkState, this.deviceState,
                   this.networkStateList)) {
      if (this.activeNetworkState.guid) {
        this.fire('show-detail', this.activeNetworkState);
      } else if (this.networkStateList.length > 0) {
        this.fire('show-detail', this.networkStateList[0]);
      }
    }
    event.stopPropagation();
  },

  /**
   * @param {!OncMojo.NetworkStateProperties} activeNetworkState
   * @param {!OncMojo.DeviceStateProperties|undefined} deviceState
   * @param {!Array<!OncMojo.NetworkStateProperties>} networkStateList
   * @return {boolean}
   * @private
   */
  isItemActionable_(activeNetworkState, deviceState, networkStateList) {
    // The boolean logic here matches onShowDetailsTap_ method that handles the
    // item click event.

    if (!this.deviceIsEnabled_(this.deviceState)) {
      // When device is disabled, tapping the item flips the enable toggle. So
      // the item is actionable only when the toggle is enabled.
      return this.enableToggleIsEnabled_(this.deviceState);
    }

    // Item is actionable if tapping should show either networks subpage or the
    // network details page.
    return this.shouldShowSubpage_(this.deviceState, this.networkStateList) ||
        this.shouldShowDetails_(
            activeNetworkState, deviceState, networkStateList);
  },

  /**
   * @param {!OncMojo.NetworkStateProperties} activeNetworkState
   * @param {!OncMojo.DeviceStateProperties|undefined} deviceState
   * @param {!Array<!OncMojo.NetworkStateProperties>} networkStateList
   * @return {boolean}
   * @private
   */
  showArrowButton_(activeNetworkState, deviceState, networkStateList) {
    // If SIM info is shown on the right side of the item, no arrow should be
    // shown.
    if (this.showSimInfo_(deviceState)) {
      return false;
    }
    if (!this.deviceIsEnabled_(deviceState)) {
      return false;
    }
    return this.shouldShowSubpage_(deviceState, networkStateList) ||
        this.shouldShowDetails_(
            activeNetworkState, deviceState, networkStateList);
  },

  /**
   * Event triggered when the enable button is toggled.
   * @param {!Event} event
   * @private
   */
  onDeviceEnabledChange_(event) {
    assert(this.deviceState);
    const deviceIsEnabled = this.deviceIsEnabled_(this.deviceState);
    this.fire(
        'device-enabled-toggled',
        {enabled: !deviceIsEnabled, type: this.deviceState.type});
    // Set the device state to enabling or disabling until updated.
    this.deviceState.deviceState = deviceIsEnabled ?
        mojom.DeviceStateType.kDisabling :
        mojom.DeviceStateType.kEnabling;
  },

  /**
   * @return {string}
   * @private
   */
  getTitleText_() {
    return this.networkTitleText ||
        this.getNetworkTypeString_(this.activeNetworkState.type);
  },

  /**
   * Make sure events in embedded components do not propagate to onDetailsTap_.
   * @param {!Event} event
   * @private
   */
  doNothing_(event) {
    event.stopPropagation();
  },

  /**
   * @param {!mojom.NetworkType} type
   * @return {string}
   * @private
   */
  getNetworkTypeString_(type) {
    // The shared Cellular/Tether subpage is referred to as "Mobile".
    // TODO(khorimoto): Remove once Cellular/Tether are split into their own
    // sections.
    if (type === mojom.NetworkType.kCellular ||
        type === mojom.NetworkType.kTether) {
      type = mojom.NetworkType.kMobile;
    }
    return this.i18n('OncType' + OncMojo.getNetworkTypeString(type));
  },
});
})();
