// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Polymer element for displaying the network state for a specific
 * type and a list of networks for that type. NOTE: It both Cellular and Tether
 * technologies are available, they are combined into a single 'Mobile data'
 * section. See crbug.com/726380.
 */

Polymer({
  is: 'network-summary-item',

  behaviors: [CrPolicyNetworkBehavior, I18nBehavior],

  properties: {
    /**
     * Device state for the network type. This might briefly be undefined if a
     * device becomes unavailable.
     * @type {!CrOnc.DeviceStateProperties|undefined}
     */
    deviceState: Object,

    /**
     * If both Cellular and Tether technologies exist, we combine the sections
     * and set this to the device state for Tether.
     * @type {!CrOnc.DeviceStateProperties|undefined}
     */
    tetherDeviceState: Object,

    /**
     * Network state for the active network.
     * @type {!CrOnc.NetworkStateProperties|undefined}
     */
    activeNetworkState: Object,

    /**
     * List of all network state data for the network type.
     * @type {!Array<!CrOnc.NetworkStateProperties>}
     */
    networkStateList: {
      type: Array,
      value: function() {
        return [];
      },
    },

    /**
     * Interface for networkingPrivate calls, passed from internet_page.
     * @type {!NetworkingPrivate}
     */
    networkingPrivate: Object,

    /**
     * Title line describing the network type to appear in the row's top line.
     * If it is undefined, the title text is a default from CrOncStrings (see
     * this.getTitleText_() below).
     * @type {string|undefined}
     */
    networkTitleText: String,
  },

  /**
   * @param {!CrOnc.NetworkStateProperties} activeNetworkState
   * @param {!CrOnc.DeviceStateProperties|undefined} deviceState
   * @return {string}
   * @private
   */
  getNetworkStateText_: function(activeNetworkState, deviceState) {
    const stateText = this.getConnectionStateText_(activeNetworkState);
    if (stateText)
      return stateText;
    // No network state, use device state.
    if (deviceState) {
      // Type specific scanning or initialization states.
      if (deviceState.Type == CrOnc.Type.CELLULAR) {
        if (deviceState.Scanning)
          return this.i18n('internetMobileSearching');
        if (deviceState.State == CrOnc.DeviceState.UNINITIALIZED)
          return this.i18n('internetDeviceInitializing');
      } else if (deviceState.Type == CrOnc.Type.TETHER) {
        if (deviceState.State == CrOnc.DeviceState.UNINITIALIZED)
          return this.i18n('tetherEnableBluetooth');
      }
      // Enabled or enabling states.
      if (deviceState.State == CrOnc.DeviceState.ENABLED) {
        if (this.networkStateList.length > 0)
          return CrOncStrings.networkListItemNotConnected;
        return CrOncStrings.networkListItemNoNetwork;
      }
      if (deviceState.State == CrOnc.DeviceState.ENABLING)
        return this.i18n('internetDeviceEnabling');
    }
    // No device or unknown device state, use 'off'.
    return this.i18n('deviceOff');
  },

  /**
   * @param {!CrOnc.NetworkStateProperties} networkState
   * @return {string}
   * @private
   */
  getConnectionStateText_: function(networkState) {
    const state = networkState ? networkState.ConnectionState : null;
    if (!state)
      return '';
    const name = CrOnc.getNetworkName(networkState);
    switch (state) {
      case CrOnc.ConnectionState.CONNECTED:
        return name;
      case CrOnc.ConnectionState.CONNECTING:
        if (name)
          return CrOncStrings.networkListItemConnectingTo.replace('$1', name);
        return CrOncStrings.networkListItemConnecting;
      case CrOnc.ConnectionState.NOT_CONNECTED:
        if (networkState.Type == CrOnc.Type.CELLULAR && networkState.Cellular &&
            networkState.Cellular.Scanning) {
          return this.i18n('internetMobileSearching');
        }
        return CrOncStrings.networkListItemNotConnected;
    }
    assertNotReached();
    return state;
  },

  /**
   * @param {!CrOnc.NetworkStateProperties} activeNetworkState
   * @return {boolean}
   * @private
   */
  showPolicyIndicator_: function(activeNetworkState) {
    return (activeNetworkState !== undefined &&
            activeNetworkState.ConnectionState ==
                CrOnc.ConnectionState.CONNECTED) ||
        this.isPolicySource(activeNetworkState.Source);
  },

  /**
   * @param {!CrOnc.DeviceStateProperties|undefined} deviceState
   * @return {boolean}
   * @private
   */
  showSimInfo_: function(deviceState) {
    if (!deviceState || deviceState.Type != CrOnc.Type.CELLULAR)
      return false;
    return this.simLockedOrAbsent_(deviceState);
  },

  /**
   * @param {!CrOnc.DeviceStateProperties} deviceState
   * @return {boolean}
   * @private
   */
  simLockedOrAbsent_: function(deviceState) {
    if (this.deviceIsEnabled_(deviceState))
      return false;
    if (deviceState.SIMPresent === false)
      return true;
    const simLockType =
        deviceState.SIMLockStatus ? deviceState.SIMLockStatus.LockType : '';
    return simLockType == CrOnc.LockType.PIN ||
        simLockType == CrOnc.LockType.PUK;
  },

  /**
   * Returns a NetworkProperties object for <network-siminfo> built from
   * the device properties (since there will be no active network).
   * @param {!CrOnc.DeviceStateProperties|undefined} deviceState
   * @return {!CrOnc.NetworkProperties|undefined}
   * @private
   */
  getCellularState_: function(deviceState) {
    if (!deviceState)
      return undefined;
    return {
      GUID: '',
      Type: CrOnc.Type.CELLULAR,
      Cellular: {
        SIMLockStatus: deviceState.SIMLockStatus,
        SIMPresent: deviceState.SIMPresent,
      },
    };
  },

  /**
   * @param {!CrOnc.DeviceStateProperties|undefined} deviceState
   * @return {boolean} Whether or not the device state is enabled.
   * @private
   */
  deviceIsEnabled_: function(deviceState) {
    return !!deviceState && deviceState.State == CrOnc.DeviceState.ENABLED;
  },

  /**
   * @param {!CrOnc.DeviceStateProperties|undefined} deviceState
   * @return {boolean}
   * @private
   */
  enableToggleIsVisible_: function(deviceState) {
    if (!deviceState)
      return false;
    switch (deviceState.Type) {
      case CrOnc.Type.ETHERNET:
      case CrOnc.Type.VPN:
        return false;
      case CrOnc.Type.TETHER:
        return true;
      case CrOnc.Type.WI_FI:
      case CrOnc.Type.WI_MAX:
        return deviceState.State != CrOnc.DeviceState.UNINITIALIZED;
      case CrOnc.Type.CELLULAR:
        return deviceState.State != CrOnc.DeviceState.UNINITIALIZED &&
            !this.simLockedOrAbsent_(deviceState);
    }
    assertNotReached();
    return false;
  },

  /**
   * @param {!CrOnc.DeviceStateProperties|undefined} deviceState
   * @return {boolean}
   * @private
   */
  enableToggleIsEnabled_: function(deviceState) {
    return this.enableToggleIsVisible_(deviceState) &&
        deviceState.State != CrOnc.DeviceState.PROHIBITED &&
        deviceState.State != CrOnc.DeviceState.UNINITIALIZED;
  },

  /**
   * @param {!CrOnc.DeviceStateProperties|undefined} deviceState
   * @return {string}
   * @private
   */
  getToggleA11yString_: function(deviceState) {
    if (!this.enableToggleIsVisible_(deviceState))
      return '';
    switch (deviceState.Type) {
      case CrOnc.Type.TETHER:
      case CrOnc.Type.CELLULAR:
        return this.i18n('internetToggleMobileA11yLabel');
      case CrOnc.Type.WI_FI:
        return this.i18n('internetToggleWiFiA11yLabel');
      case CrOnc.Type.WI_MAX:
        return this.i18n('internetToggleWiMAXA11yLabel');
    }
    assertNotReached();
    return '';
  },

  /**
   * @param {!CrOnc.NetworkStateProperties} activeNetworkState
   * @param {!CrOnc.DeviceStateProperties|undefined} deviceState
   * @param {!Array<!CrOnc.NetworkStateProperties>} networkStateList
   * @return {boolean}
   * @private
   */
  showDetailsIsVisible_: function(
      activeNetworkState, deviceState, networkStateList) {
    return this.deviceIsEnabled_(deviceState) &&
        (!!activeNetworkState.GUID || networkStateList.length > 0);
  },

  /**
   * @param {!CrOnc.DeviceStateProperties|undefined} deviceState
   * @param {!Array<!CrOnc.NetworkStateProperties>} networkStateList
   * @return {boolean}
   * @private
   */
  shouldShowSubpage_: function(deviceState, networkStateList) {
    if (!deviceState)
      return false;
    const type = deviceState.Type;
    if (type == CrOnc.Type.TETHER ||
        (type == CrOnc.Type.CELLULAR && this.tetherDeviceState)) {
      // The "Mobile data" subpage should always be shown if Tether networks are
      // available, even if there are currently no associated networks.
      return true;
    }
    const minlen = (type == CrOnc.Type.WI_FI || type == CrOnc.Type.VPN) ? 1 : 2;
    return networkStateList.length >= minlen;
  },

  /**
   * @param {!Event} event The enable button event.
   * @private
   */
  onShowDetailsTap_: function(event) {
    if (!this.deviceIsEnabled_(this.deviceState)) {
      if (this.enableToggleIsEnabled_(this.deviceState)) {
        this.fire(
            'device-enabled-toggled',
            {enabled: true, type: this.deviceState.Type});
      }
    } else if (this.shouldShowSubpage_(
                   this.deviceState, this.networkStateList)) {
      this.fire('show-networks', {type: this.deviceState.Type});
    } else if (this.activeNetworkState.GUID) {
      this.fire('show-detail', this.activeNetworkState);
    } else if (this.networkStateList.length > 0) {
      this.fire('show-detail', this.networkStateList[0]);
    }
    event.stopPropagation();
  },

  /**
   * @param {!CrOnc.NetworkStateProperties} activeNetworkState
   * @param {!CrOnc.DeviceStateProperties|undefined} deviceState
   * @param {!Array<!CrOnc.NetworkStateProperties>} networkStateList
   * @return {string}
   * @private
   */
  getDetailsA11yString_: function(
      activeNetworkState, deviceState, networkStateList) {
    if (!this.shouldShowSubpage_(deviceState, networkStateList)) {
      if (activeNetworkState.GUID) {
        return CrOnc.getNetworkName(activeNetworkState);
      } else if (networkStateList.length > 0) {
        return CrOnc.getNetworkName(networkStateList[0]);
      }
    }
    return this.i18n('OncType' + deviceState.Type);
  },

  /**
   * Event triggered when the enable button is toggled.
   * @param {!Event} event
   * @private
   */
  onDeviceEnabledChange_: function(event) {
    const deviceIsEnabled = this.deviceIsEnabled_(this.deviceState);
    const type = this.deviceState ? this.deviceState.Type : '';
    this.fire(
        'device-enabled-toggled', {enabled: !deviceIsEnabled, type: type});
  },

  /**
   * @return {string}
   * @private
   */
  getTitleText_: function() {
    return this.networkTitleText ||
        CrOncStrings['OncType' + this.activeNetworkState.Type];
  },

  /**
   * Make sure events in embedded components do not propagate to onDetailsTap_.
   * @param {!Event} event
   * @private
   */
  doNothing_: function(event) {
    event.stopPropagation();
  },
});
