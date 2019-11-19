// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'internet-detail-dialog' is used in the login screen to show a subset of
 * internet details and allow configuration of proxy, IP, and nameservers.
 */
(function() {
'use strict';

Polymer({
  is: 'internet-detail-dialog',

  behaviors: [
    NetworkListenerBehavior,
    CrPolicyNetworkBehaviorMojo,
    I18nBehavior,
  ],

  properties: {
    /** The network GUID to display details for. */
    guid: String,

    /** @private {!chromeos.networkConfig.mojom.ManagedProperties|undefined} */
    managedProperties_: {
      type: Object,
      observer: 'managedPropertiesChanged_',
    },

    /** @private {?OncMojo.DeviceStateProperties} */
    deviceState_: {
      type: Object,
      value: null,
    },

    /**
     * Whether to show technology badge on mobile network icons.
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
   * Set to true once the action button has been focused.
   * @private {boolean}
   */
  didSetFocus_: false,

  /**
   * Set to true to once the initial properties have been received. This
   * prevents setProperties from being called when setting default properties.
   * @private {boolean}
   */
  propertiesReceived_: false,

  /** @private {?chromeos.networkConfig.mojom.CrosNetworkConfigRemote} */
  networkConfig_: null,

  /** @override */
  created: function() {
    this.networkConfig_ = network_config.MojoInterfaceProviderImpl.getInstance()
                              .getMojoServiceRemote();
  },

  /** @override */
  attached: function() {
    const dialogArgs = chrome.getVariableValue('dialogArguments');
    let type, name;
    if (dialogArgs) {
      const args = JSON.parse(dialogArgs);
      this.guid = args.guid || '';
      type = args.type || 'WiFi';
      name = args.name || type;
    } else {
      // For debugging
      const params = new URLSearchParams(document.location.search.substring(1));
      this.guid = params.get('guid') || '';
      type = params.get('type') || 'WiFi';
      name = params.get('name') || type;
    }

    if (!this.guid) {
      console.error('Invalid guid');
      this.close_();
    }

    // Set default managedProperties_ until they are loaded.
    this.propertiesReceived_ = false;
    this.deviceState_ = null;
    this.managedProperties_ = OncMojo.getDefaultManagedProperties(
        OncMojo.getNetworkTypeFromString(type), this.guid, name);
    this.getNetworkDetails_();
  },

  /** @override */
  ready: function() {
    CrOncStrings = {
      OncTypeCellular: loadTimeData.getString('OncTypeCellular'),
      OncTypeEthernet: loadTimeData.getString('OncTypeEthernet'),
      OncTypeMobile: loadTimeData.getString('OncTypeMobile'),
      OncTypeTether: loadTimeData.getString('OncTypeTether'),
      OncTypeVPN: loadTimeData.getString('OncTypeVPN'),
      OncTypeWiFi: loadTimeData.getString('OncTypeWiFi'),
      networkListItemConnected:
          loadTimeData.getString('networkListItemConnected'),
      networkListItemConnecting:
          loadTimeData.getString('networkListItemConnecting'),
      networkListItemConnectingTo:
          loadTimeData.getString('networkListItemConnectingTo'),
      networkListItemInitializing:
          loadTimeData.getString('networkListItemInitializing'),
      networkListItemLabelTemplate:
          loadTimeData.getString('networkListItemLabelTemplate'),
      networkListItemNotAvailable:
          loadTimeData.getString('networkListItemNotAvailable'),
      networkListItemScanning:
          loadTimeData.getString('networkListItemScanning'),
      networkListItemSimCardLocked:
          loadTimeData.getString('networkListItemSimCardLocked'),
      networkListItemNotConnected:
          loadTimeData.getString('networkListItemNotConnected'),
      networkListItemNoNetwork:
          loadTimeData.getString('networkListItemNoNetwork'),
      vpnNameTemplate: loadTimeData.getString('vpnNameTemplate'),
    };
  },

  /** @private */
  managedPropertiesChanged_: function() {
    assert(this.managedProperties_);

    // Focus the action button once the initial state is set.
    if (!this.didSetFocus_ &&
        this.showConnectDisconnect_(this.managedProperties_)) {
      const button = this.$$('#title .action-button:not([hidden])');
      if (button) {
        button.focus();
        this.didSetFocus_ = true;
      }
    }
  },

  /** @private */
  close_: function() {
    chrome.send('dialogClose');
  },

  /**
   * CrosNetworkConfigObserver impl
   * @param {!Array<OncMojo.NetworkStateProperties>} networks
   */
  onActiveNetworksChanged: function(networks) {
    if (!this.guid || !this.managedProperties_) {
      return;
    }
    // If the network was or is active, request an update.
    if (this.managedProperties_.connectionState !=
            chromeos.networkConfig.mojom.ConnectionStateType.kNotConnected ||
        networks.find(network => network.guid == this.guid)) {
      this.getNetworkDetails_();
    }
  },

  /**
   * CrosNetworkConfigObserver impl
   * @param {!chromeos.networkConfig.mojom.NetworkStateProperties} network
   */
  onNetworkStateChanged: function(network) {
    if (!this.guid || !this.managedProperties_) {
      return;
    }
    if (network.guid == this.guid) {
      this.getNetworkDetails_();
    }
  },

  /** CrosNetworkConfigObserver impl */
  onDeviceStateListChanged: function() {
    if (!this.guid || !this.managedProperties_) {
      return;
    }
    this.getDeviceState_();
    this.getNetworkDetails_();
  },

  /** @private */
  getNetworkDetails_: function() {
    assert(this.guid);
    this.networkConfig_.getManagedProperties(this.guid).then(response => {
      if (!response.result) {
        // Edge case, may occur when disabling. Close this.
        this.close_();
        return;
      }
      this.managedProperties_ = response.result;
      this.propertiesReceived_ = true;
      if (!this.deviceState_) {
        this.getDeviceState_();
      }
    });
  },

  /** @private */
  getDeviceState_: function() {
    if (!this.managedProperties_) {
      return;
    }
    const type = this.managedProperties_.type;
    this.networkConfig_.getDeviceStateList().then(response => {
      const devices = response.result;
      this.deviceState_ = devices.find(device => device.type == type) || null;
    });
  },

  /**
   * @param {!chromeos.networkConfig.mojom.ManagedProperties} managedProperties
   * @return {!OncMojo.NetworkStateProperties}
   */
  getNetworkState_: function(managedProperties) {
    return OncMojo.managedPropertiesToNetworkState(managedProperties);
  },

  /**
   * @return {!chromeos.networkConfig.mojom.ConfigProperties}
   * @private
   */
  getDefaultConfigProperties_: function() {
    return OncMojo.getDefaultConfigProperties(this.managedProperties_.type);
  },

  /**
   * @param {!chromeos.networkConfig.mojom.ConfigProperties} config
   * @private
   */
  setMojoNetworkProperties_: function(config) {
    if (!this.propertiesReceived_ || !this.guid) {
      return;
    }
    this.networkConfig_.setProperties(this.guid, config).then(response => {
      if (!response.success) {
        console.error('Unable to set properties: ' + JSON.stringify(config));
        // An error typically indicates invalid input; request the properties
        // to update any invalid fields.
        this.getNetworkDetails_();
      }
    });
  },

  /**
   * @param {!chromeos.networkConfig.mojom.ManagedProperties} managedProperties
   * @return {string}
   * @private
   */
  getStateText_: function(managedProperties) {
    if (!managedProperties) {
      return '';
    }
    return this.i18n(
        OncMojo.getConnectionStateString(managedProperties.connectionState));
  },

  /**
   * @param {!chromeos.networkConfig.mojom.ManagedProperties} managedProperties
   * @return {string}
   * @private
   */
  getNameText_: function(managedProperties) {
    return OncMojo.getNetworkName(managedProperties);
  },

  /**
   * @param {!chromeos.networkConfig.mojom.ManagedProperties} managedProperties
   * @return {boolean} True if the network is connected.
   * @private
   */
  isConnectedState_: function(managedProperties) {
    return OncMojo.connectionStateIsConnected(
        managedProperties.connectionState);
  },

  /**
   * @param {!chromeos.networkConfig.mojom.ManagedProperties} managedProperties
   * @return {boolean}
   * @private
   */
  isRemembered_: function(managedProperties) {
    return managedProperties.source !=
        chromeos.networkConfig.mojom.OncSource.kNone;
  },

  /**
   * @param {!chromeos.networkConfig.mojom.ManagedProperties} managedProperties
   * @return {boolean}
   * @private
   */
  isRememberedOrConnected_: function(managedProperties) {
    return this.isRemembered_(managedProperties) ||
        this.isConnectedState_(managedProperties);
  },

  /**
   * @param {!chromeos.networkConfig.mojom.ManagedProperties} managedProperties
   * @return {boolean}
   * @private
   */
  isCellular_: function(managedProperties) {
    return managedProperties.type ==
        chromeos.networkConfig.mojom.NetworkType.kCellular;
  },

  /**
   * @param {!chromeos.networkConfig.mojom.ManagedProperties} managedProperties
   * @return {boolean}
   * @private
   */
  showCellularSim_: function(managedProperties) {
    return managedProperties.type ==
        chromeos.networkConfig.mojom.NetworkType.kCellular &&
        managedProperties.typeProperties.cellular.family != 'CDMA';
  },

  /**
   * @param {!chromeos.networkConfig.mojom.ManagedProperties} managedProperties
   * @return {boolean}
   * @private
   */
  showCellularChooseNetwork_: function(managedProperties) {
    return managedProperties.type ==
        chromeos.networkConfig.mojom.NetworkType.kCellular &&
        managedProperties.typeProperties.cellular.supportNetworkScan;
  },

  /**
   * @param {!chromeos.networkConfig.mojom.ManagedProperties} managedProperties
   * @return {boolean}
   * @private
   */
  showForget_: function(managedProperties) {
    const mojom = chromeos.networkConfig.mojom;
    if (!managedProperties ||
        managedProperties.type != mojom.NetworkType.kWiFi) {
      return false;
    }
    return managedProperties.source != mojom.OncSource.kNone &&
        !this.isPolicySource(managedProperties.source);
  },

  /** @private */
  onForgetTap_: function() {
    this.networkConfig_.forgetNetwork(this.guid).then(response => {
      if (!response.success) {
        console.error('Forget network failed for: ' + this.guid);
      }
      // A forgotten network no longer has a valid GUID, close the dialog.
      this.close_();
    });
  },

  /**
   * @param {!chromeos.networkConfig.mojom.ManagedProperties} managedProperties
   * @return {string}
   * @private
   */
  getConnectDisconnectText_: function(managedProperties) {
    if (this.showConnect_(managedProperties)) {
      return this.i18n('networkButtonConnect');
    }
    return this.i18n('networkButtonDisconnect');
  },

  /**
   * @param {!chromeos.networkConfig.mojom.ManagedProperties} managedProperties
   * @return {boolean}
   * @private
   */
  showConnectDisconnect_: function(managedProperties) {
    return this.showConnect_(managedProperties) ||
        this.showDisconnect_(managedProperties);
  },

  /**
   * @param {!chromeos.networkConfig.mojom.ManagedProperties} managedProperties
   * @return {boolean}
   * @private
   */
  showConnect_: function(managedProperties) {
    return managedProperties.connectable &&
        managedProperties.type !=
        chromeos.networkConfig.mojom.NetworkType.kEthernet &&
        managedProperties.connectionState ==
        chromeos.networkConfig.mojom.ConnectionStateType.kNotConnected;
  },

  /**
   * @param {!chromeos.networkConfig.mojom.ManagedProperties} managedProperties
   * @return {boolean}
   * @private
   */
  showDisconnect_: function(managedProperties) {
    return managedProperties.type !=
        chromeos.networkConfig.mojom.NetworkType.kEthernet &&
        managedProperties.connectionState !=
        chromeos.networkConfig.mojom.ConnectionStateType.kNotConnected;
  },

  /**
   * @param {!chromeos.networkConfig.mojom.ManagedProperties} managedProperties
   * @return {boolean}
   * @private
   */
  shouldShowProxyPolicyIndicator_: function(managedProperties) {
    if (!managedProperties.proxySettings) {
      return false;
    }
    return this.isNetworkPolicyEnforced(managedProperties.proxySettings.type);
  },

  /**
   * @param {!chromeos.networkConfig.mojom.ManagedProperties} managedProperties
   * @return {boolean}
   * @private
   */
  enableConnectDisconnect_: function(managedProperties) {
    if (!this.showConnectDisconnect_(managedProperties)) {
      return false;
    }

    if (this.showConnect_(managedProperties)) {
      return this.enableConnect_(managedProperties);
    }

    return true;
  },

  /**
   * @param {!chromeos.networkConfig.mojom.ManagedProperties} managedProperties
   * @return {boolean} Whether or not to enable the network connect button.
   * @private
   */
  enableConnect_: function(managedProperties) {
    return this.showConnect_(managedProperties);
  },

  /** @private */
  onConnectDisconnectClick_: function() {
    if (!this.managedProperties_) {
      return;
    }
    if (!this.showConnect_(this.managedProperties_)) {
      this.networkConfig_.startDisconnect(this.guid);
      return;
    }

    const mojom = chromeos.networkConfig.mojom;
    const guid = this.managedProperties_.guid;
    this.networkConfig_.startConnect(this.guid).then(response => {
      switch (response.result) {
        case mojom.StartConnectResult.kSuccess:
          break;
        case mojom.StartConnectResult.kInvalidState:
        case mojom.StartConnectResult.kCanceled:
          // Ignore failures due to in-progress or cancelled connects.
          break;
        case mojom.StartConnectResult.kInvalidGuid:
        case mojom.StartConnectResult.kNotConfigured:
        case mojom.StartConnectResult.kBlocked:
        case mojom.StartConnectResult.kUnknown:
          console.error(
              'Unexpected startConnect error for: ' + guid + ' Result: ' +
              response.result.toString() + ' Message: ' + response.message);
          break;
      }
    });
  },

  /**
   * @param {!CustomEvent<!chromeos.networkConfig.mojom.ApnProperties>} event
   * @private
   */
  onApnChange_: function(event) {
    if (!this.propertiesReceived_) {
      return;
    }
    const config = this.getDefaultConfigProperties_();
    const apn = event.detail;
    config.typeConfig.cellular = {apn: apn};
    this.setMojoNetworkProperties_(config);
  },

  /**
   * Event triggered when the IP Config or NameServers element changes.
   * @param {!CustomEvent<!{
   *     field: string,
   *     value: (string|!chromeos.networkConfig.mojom.IPConfigProperties|
   *             !Array<string>)
   * }>} event The network-ip-config or network-nameservers change event.
   * @private
   */
  onIPConfigChange_: function(event) {
    if (!this.managedProperties_) {
      return;
    }
    const config = OncMojo.getUpdatedIPConfigProperties(
        this.managedProperties_, event.detail.field, event.detail.value);
    if (config) {
      this.setMojoNetworkProperties_(config);
    }
  },

  /**
   * Event triggered when the Proxy configuration element changes.
   * @param {!CustomEvent<!chromeos.networkConfig.mojom.ProxySettings>} event
   * @private
   */
  onProxyChange_: function(event) {
    if (!this.propertiesReceived_) {
      return;
    }
    const config = this.getDefaultConfigProperties_();
    config.proxySettings = event.detail;
    this.setMojoNetworkProperties_(config);
  },

  /**
   * @param {!Array<string>} fields
   * @return {boolean}
   * @private
   */
  hasVisibleFields_: function(fields) {
    return fields.some((field) => {
      const value = this.get(field, this.managedProperties_);
      return value !== undefined && value !== '';
    });
  },

  /**
   * @return {boolean}
   * @private
   */
  hasInfoFields_: function() {
    return this.hasVisibleFields_(this.getInfoFields_());
  },

  /**
   * @return {!Array<string>} The fields to display in the info section.
   * @private
   */
  getInfoFields_: function() {
    /** @type {!Array<string>} */ const fields = [];
    const type = this.managedProperties_.type;
    if (type == chromeos.networkConfig.mojom.NetworkType.kCellular) {
      fields.push(
          'cellular.homeProvider.name', 'cellular.servingOperator.name',
          'cellular.activationState', 'cellular.roamingState',
          'restrictedConnectivity', 'cellular.meid', 'cellular.esn',
          'cellular.iccid', 'cellular.imei', 'cellular.imsi', 'cellular.mdn',
          'cellular.min');
    } else if (type == chromeos.networkConfig.mojom.NetworkType.kWiFi) {
      fields.push('restrictedConnectivity');
    }
    return fields;
  },
});
})();
