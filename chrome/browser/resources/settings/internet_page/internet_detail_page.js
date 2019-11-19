// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'settings-internet-detail' is the settings subpage containing details
 * for a network.
 */
(function() {
'use strict';

const mojom = chromeos.networkConfig.mojom;

Polymer({
  is: 'settings-internet-detail-page',

  behaviors: [
    NetworkListenerBehavior,
    CrPolicyNetworkBehaviorMojo,
    settings.RouteObserverBehavior,
    I18nBehavior,
  ],

  properties: {
    /** The network GUID to display details for. */
    guid: String,

    /** Preferences state. */
    prefs: {
      type: Object,
      notify: true,
    },

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
     * Whether the user is a secondary user.
     * @private
     */
    isSecondaryUser_: {
      type: Boolean,
      value: function() {
        return loadTimeData.getBoolean('isSecondaryUser');
      },
      readOnly: true,
    },

    /**
     * Email address for the primary user.
     * @private
     */
    primaryUserEmail_: {
      type: String,
      value: function() {
        return loadTimeData.getBoolean('isSecondaryUser') ?
            loadTimeData.getString('primaryUserEmail') :
            '';
      },
      readOnly: true,
    },

    /**
     * Whether the network has been lost (e.g., has gone out of range). A
     * network is considered to be lost when a OnNetworkStateListChanged
     * is signaled and the new network list does not contain the GUID of the
     * current network.
     * @private
     */
    outOfRange_: {
      type: Boolean,
      value: false,
    },

    /**
     * Highest priority connected network or null.
     * @type {?OncMojo.NetworkStateProperties}
     */
    defaultNetwork: {
      type: Object,
      value: null,
    },

    /** @type {!chromeos.networkConfig.mojom.GlobalPolicy|undefined} */
    globalPolicy: Object,

    /**
     * Whether a managed network is available in the visible network list.
     * @private {boolean}
     */
    managedNetworkAvailable: {
      type: Boolean,
      value: false,
    },

    /**
     * The network AutoConnect state as a fake preference object.
     * @private {!chrome.settingsPrivate.PrefObject|undefined}
     */
    autoConnectPref_: {
      type: Object,
      observer: 'autoConnectPrefChanged_',
    },

    /**
     * The always-on VPN state as a fake preference object.
     * @private {!chrome.settingsPrivate.PrefObject|undefined}
     */
    alwaysOnVpn_: {
      type: Object,
      observer: 'alwaysOnVpnChanged_',
      value: function() {
        return {
          key: 'fakeAlwaysOnPref',
          type: chrome.settingsPrivate.PrefType.BOOLEAN,
          value: false,
        };
      }
    },

    /**
     * The network preferred state.
     * @private
     */
    preferNetwork_: {
      type: Boolean,
      value: false,
      observer: 'preferNetworkChanged_',
    },

    /**
     * The network IP Address.
     * @private
     */
    ipAddress_: {
      type: String,
      value: '',
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

    /** @private */
    advancedExpanded_: Boolean,

    /** @private */
    networkExpanded_: Boolean,

    /** @private */
    proxyExpanded_: Boolean,
  },

  observers: [
    'updateAlwaysOnVpnPrefValue_(prefs.arc.vpn.always_on.*)',
    'updateAlwaysOnVpnPrefEnforcement_(managedProperties_,' +
        'prefs.vpn_config_allowed.*)',
    'updateAutoConnectPref_(globalPolicy)',
    'autoConnectPrefChanged_(autoConnectPref_.*)',
    'alwaysOnVpnChanged_(alwaysOnVpn_.*)',
  ],

  /** @private {boolean} */
  didSetFocus_: false,

  /**
   * Set to true to once the initial properties have been received. This
   * prevents setProperties from being called when setting default properties.
   * @private {boolean}
   */
  propertiesReceived_: false,

  /**
   * Set in currentRouteChanged() if the showConfigure URL query
   * parameter is set to true. The dialog cannot be shown until the
   * network properties have been fetched in managedPropertiesChanged_().
   * @private {boolean}
   */
  shouldShowConfigureWhenNetworkLoaded_: false,

  /** @private  {settings.InternetPageBrowserProxy} */
  browserProxy_: null,

  /** @private {?chromeos.networkConfig.mojom.CrosNetworkConfigRemote} */
  networkConfig_: null,

  /** @override */
  created: function() {
    this.browserProxy_ = settings.InternetPageBrowserProxyImpl.getInstance();
    this.networkConfig_ = network_config.MojoInterfaceProviderImpl.getInstance()
                              .getMojoServiceRemote();
  },

  /**
   * settings.RouteObserverBehavior
   * @param {!settings.Route} route
   * @param {!settings.Route} oldRoute
   * @protected
   */
  currentRouteChanged: function(route, oldRoute) {
    if (route != settings.routes.NETWORK_DETAIL) {
      return;
    }

    const queryParams = settings.getQueryParameters();
    const guid = queryParams.get('guid') || '';
    if (!guid) {
      console.error('No guid specified for page:' + route);
      this.close();
    }

    this.shouldShowConfigureWhenNetworkLoaded_ =
        queryParams.get('showConfigure') == 'true';
    const type = queryParams.get('type') || 'WiFi';
    const name = queryParams.get('name') || type;
    this.init(guid, type, name);
  },

  /**
   * @param {string} guid
   * @param {string} type
   * @param {string} name
   */
  init: function(guid, type, name) {
    this.guid = guid;
    // Set default properties until they are loaded.
    this.propertiesReceived_ = false;
    this.deviceState_ = null;
    this.managedProperties_ = OncMojo.getDefaultManagedProperties(
        OncMojo.getNetworkTypeFromString(type), this.guid, name);
    this.didSetFocus_ = false;
    this.getNetworkDetails_();
  },

  close: function() {
    // If the page is already closed, return early to avoid navigating backward
    // erroneously.
    if (!this.guid) {
      return;
    }

    this.guid = '';

    // Delay navigating to allow other subpages to load first.
    requestAnimationFrame(() => {
      // Clear network properties before navigating away to ensure that a future
      // navigation back to the details page does not show a flicker of
      // incorrect text. See https://crbug.com/905986.
      this.managedProperties_ = undefined;
      this.propertiesReceived_ = false;

      settings.navigateToPreviousRoute();
    });
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
            mojom.ConnectionStateType.kNotConnected ||
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
  onNetworkStateListChanged: function() {
    if (!this.guid || !this.managedProperties_) {
      return;
    }
    this.checkNetworkExists_();
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
  managedPropertiesChanged_: function() {
    if (!this.managedProperties_) {
      return;
    }
    this.updateAutoConnectPref_();

    const priority = this.managedProperties_.priority;
    if (priority) {
      const preferNetwork = priority.activeValue > 0;
      if (preferNetwork != this.preferNetwork_) {
        this.preferNetwork_ = preferNetwork;
      }
    }

    // Set the IPAddress property to the IPv4 Address.
    const ipv4 = OncMojo.getIPConfigForType(this.managedProperties_, 'IPv4');
    this.ipAddress_ = (ipv4 && ipv4.ipAddress) || '';

    // Update the detail page title.
    const networkName = OncMojo.getNetworkName(this.managedProperties_);
    this.parentNode.pageTitle = networkName;
    Polymer.dom.flush();

    if (!this.didSetFocus_) {
      // Focus a button once the initial state is set.
      this.didSetFocus_ = true;
      const button = this.$$('#titleDiv .action-button:not([hidden])');
      if (button) {
        Polymer.RenderStatus.afterNextRender(this, () => button.focus());
      }
    }

    if (this.shouldShowConfigureWhenNetworkLoaded_ &&
        this.managedProperties_.type == mojom.NetworkType.kTether) {
      // Set |this.shouldShowConfigureWhenNetworkLoaded_| back to false to
      // ensure that the Tether dialog is only shown once.
      this.shouldShowConfigureWhenNetworkLoaded_ = false;
      // Async call to ensure dialog is stamped.
      setTimeout(() => this.showTetherDialog_());
    }
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

  /** @private */
  autoConnectPrefChanged_: function() {
    if (!this.propertiesReceived_) {
      return;
    }
    const config = this.getDefaultConfigProperties_();
    config.autoConnect = {value: !!this.autoConnectPref_.value};
    this.setMojoNetworkProperties_(config);
  },

  /**
   * Updates auto-connect pref value.
   * @private
   */
  updateAutoConnectPref_: function() {
    if (!this.managedProperties_) {
      return;
    }
    const autoConnect = OncMojo.getManagedAutoConnect(this.managedProperties_);
    if (!autoConnect) {
      return;
    }

    let enforcement;
    let controlledBy;
    if (autoConnect.enforced ||
        (!!this.globalPolicy &&
         !!this.globalPolicy.allowOnlyPolicyNetworksToAutoconnect)) {
      enforcement = chrome.settingsPrivate.Enforcement.ENFORCED;
      controlledBy = chrome.settingsPrivate.ControlledBy.DEVICE_POLICY;
    }

    if (this.autoConnectPref_ &&
        this.autoConnectPref_.value == autoConnect.activeValue &&
        enforcement == this.autoConnectPref_.enforcement &&
        controlledBy == this.autoConnectPref_.controlledBy) {
      return;
    }

    const newPrefValue = {
      key: 'fakeAutoConnectPref',
      value: autoConnect.activeValue,
      type: chrome.settingsPrivate.PrefType.BOOLEAN,
    };
    if (enforcement) {
      newPrefValue.enforcement = enforcement;
      newPrefValue.controlledBy = controlledBy;
    }

    this.autoConnectPref_ = newPrefValue;
  },

  /** @private */
  preferNetworkChanged_: function() {
    if (!this.propertiesReceived_) {
      return;
    }
    const config = this.getDefaultConfigProperties_();
    config.priority = {value: this.preferNetwork_ ? 1 : 0};
    this.setMojoNetworkProperties_(config);
  },

  /** @private */
  checkNetworkExists_: function() {
    const filter = {
      filter: mojom.FilterType.kVisible,
      networkType: mojom.NetworkType.kAll,
      limit: mojom.NO_LIMIT,
    };
    this.networkConfig_.getNetworkState(this.guid).then(response => {
      if (response.result) {
        // Don't update the state, a change event will trigger the update.
        return;
      }
      this.outOfRange_ = true;
      if (this.managedProperties_) {
        // Set the connection state since we won't receive an update for a non
        // existent network.
        this.managedProperties_.connectionState =
            mojom.ConnectionStateType.kNotConnected;
      }
    });
  },

  /** @private */
  getNetworkDetails_: function() {
    assert(this.guid);
    if (this.isSecondaryUser_) {
      this.networkConfig_.getNetworkState(this.guid).then(response => {
        this.getStateCallback_(response.result);
      });
    } else {
      this.networkConfig_.getManagedProperties(this.guid).then(response => {
        this.getPropertiesCallback_(response.result);
      });
    }
  },

  /**
   * @param {?mojom.ManagedProperties} properties
   * @private
   */
  getPropertiesCallback_: function(properties) {
    // Details page was closed while request was in progress, ignore the result.
    if (!this.guid) {
      return;
    }

    if (!properties) {
      console.error('Details page: GUID no longer exists: ' + this.guid);
      this.close();
      return;
    }

    this.managedProperties_ = properties;
    // Detail page should not be shown when Arc VPN is not connected.
    if (this.isArcVpn_(this.managedProperties_) &&
        !this.isConnectedState_(this.managedProperties_)) {
      this.guid = '';
      this.close();
    }
    this.propertiesReceived_ = true;
    this.outOfRange_ = false;
    if (!this.deviceState_) {
      this.getDeviceState_();
    }
  },

  /**
   * @param {?OncMojo.NetworkStateProperties} networkState
   * @private
   */
  getStateCallback_: function(networkState) {
    if (!networkState) {
      // Edge case, may occur when disabling. Close this.
      this.close();
      return;
    }

    const managedProperties = OncMojo.getDefaultManagedProperties(
        networkState.type, networkState.guid, networkState.name);
    managedProperties.connectable = networkState.connectable;
    managedProperties.connectionState = networkState.connectionState;
    switch (networkState.type) {
      case mojom.NetworkType.kCellular:
        managedProperties.typeProperties.cellular.signalStrength =
            networkState.typeState.cellular.signalStrength;
        break;
      case mojom.NetworkType.kTether:
        managedProperties.typeProperties.tether.signalStrength =
            networkState.typeState.tether.signalStrength;
        break;
      case mojom.NetworkType.kWiFi:
        managedProperties.typeProperties.wifi.signalStrength =
            networkState.typeState.wifi.signalStrength;
        break;
    }
    this.managedProperties_ = managedProperties;

    this.propertiesReceived_ = true;
    this.outOfRange_ = false;
  },

  /**
   * @param {!mojom.ManagedProperties} properties
   * @return {!OncMojo.NetworkStateProperties|undefined}
   */
  getNetworkState_: function(properties) {
    if (!properties) {
      return undefined;
    }
    return OncMojo.managedPropertiesToNetworkState(properties);
  },

  /**
   * @return {!mojom.ConfigProperties}
   * @private
   */
  getDefaultConfigProperties_: function() {
    return OncMojo.getDefaultConfigProperties(this.managedProperties_.type);
  },

  /**
   * @param {!mojom.ConfigProperties} config
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
   * @param {!mojom.ManagedProperties} managedProperties
   * @param {boolean} propertiesReceived
   * @param {boolean} outOfRange
   * @param {?OncMojo.DeviceStateProperties} deviceState
   * @return {string} The text to display for the network connection state.
   * @private
   */
  getStateText_: function(
      managedProperties, propertiesReceived, outOfRange, deviceState) {
    if (!managedProperties || !propertiesReceived) {
      return '';
    }

    if (this.isOutOfRangeOrNotEnabled_(outOfRange, deviceState)) {
      return managedProperties.type == mojom.NetworkType.kTether ?
          this.i18n('tetherPhoneOutOfRange') :
          this.i18n('networkOutOfRange');
    }

    if (managedProperties.type == mojom.NetworkType.kCellular &&
        !managedProperties.connectable) {
      if (managedProperties.typeProperties.cellular.homeProvider &&
          managedProperties.typeProperties.cellular.homeProvider.name) {
        return this.i18n(
            'cellularContactSpecificCarrier',
            managedProperties.typeProperties.cellular.homeProvider.name);
      }
      return this.i18n('cellularContactDefaultCarrier');
    }

    return this.i18n(
        OncMojo.getConnectionStateString(managedProperties.connectionState));
  },

  /**
   * @param {!mojom.ManagedProperties} managedProperties
   * @return {string} The text to display for auto-connect toggle label.
   * @private
   */
  getAutoConnectToggleLabel_: function(managedProperties) {
    return this.isCellular_(managedProperties) ?
        this.i18n('networkAutoConnectCellular') :
        this.i18n('networkAutoConnect');
  },

  /**
   * @param {!mojom.ManagedProperties} managedProperties
   * @return {string} The text to display with roaming details.
   * @private
   */
  getRoamingDetails_: function(managedProperties) {
    if (!this.isCellular_(managedProperties)) {
      return '';
    }
    if (!managedProperties.typeProperties.cellular.allowRoaming) {
      return this.i18n('networkAllowDataRoamingDisabled');
    }

    return managedProperties.typeProperties.cellular.roamingState == 'Roaming' ?
        this.i18n('networkAllowDataRoamingEnabledRoaming') :
        this.i18n('networkAllowDataRoamingEnabledHome');
  },

  /**
   * @param {!mojom.ManagedProperties|undefined} managedProperties
   * @return {boolean} True if the network is connected.
   * @private
   */
  isConnectedState_: function(managedProperties) {
    return !!managedProperties &&
        OncMojo.connectionStateIsConnected(managedProperties.connectionState);
  },

  /**
   * @param {!mojom.ManagedProperties|undefined} managedProperties
   * @param {boolean} outOfRange
   * @param {?OncMojo.DeviceStateProperties} deviceState
   * @return {boolean} True if the network shown cannot initiate a connection.
   * @private
   */
  isConnectionErrorState_: function(
      managedProperties, outOfRange, deviceState) {
    if (this.isOutOfRangeOrNotEnabled_(outOfRange, deviceState)) {
      return true;
    }

    if (!managedProperties) {
      return false;
    }

    // It's still possible to initiate a connection to a network if it is not
    // connectable as long as the network has an associated configuration flow.
    // Cellular networks do not have a configuration flow, so a Cellular network
    // that is not connectable represents an error state.
    return managedProperties.type == mojom.NetworkType.kCellular &&
        !managedProperties.connectable;
  },

  /**
   * @param {!mojom.ManagedProperties} managedProperties
   * @return {boolean}
   * @private
   */
  isRemembered_: function(managedProperties) {
    return !!managedProperties &&
        managedProperties.source != mojom.OncSource.kNone;
  },

  /**
   * @param {!mojom.ManagedProperties} managedProperties
   * @return {boolean}
   * @private
   */
  isRememberedOrConnected_: function(managedProperties) {
    return this.isRemembered_(managedProperties) ||
        this.isConnectedState_(managedProperties);
  },

  /**
   * @param {!mojom.ManagedProperties} managedProperties
   * @return {boolean}
   * @private
   */
  isCellular_: function(managedProperties) {
    return !!managedProperties &&
        managedProperties.type == mojom.NetworkType.kCellular;
  },

  /**
   * @param {!mojom.ManagedProperties} managedProperties
   * @return {boolean}
   * @private
   */
  isTether_: function(managedProperties) {
    return !!managedProperties &&
        managedProperties.type == mojom.NetworkType.kTether;
  },

  /**
   * @param {!mojom.ManagedProperties} managedProperties
   * @param {!mojom.GlobalPolicy|undefined} globalPolicy
   * @param {boolean} managedNetworkAvailable
   * @return {boolean}
   * @private
   */
  isBlockedByPolicy_: function(
      managedProperties, globalPolicy, managedNetworkAvailable) {
    if (!managedProperties || !globalPolicy ||
        managedProperties.type != mojom.NetworkType.kWiFi ||
        this.isPolicySource(managedProperties.source)) {
      return false;
    }
    const hexSsid =
        OncMojo.getActiveString(managedProperties.typeProperties.wifi.hexSsid);
    return !!globalPolicy.allowOnlyPolicyNetworksToConnect ||
        (!!globalPolicy.allowOnlyPolicyNetworksToConnectIfAvailable &&
         !!managedNetworkAvailable) ||
        (!!hexSsid && !!globalPolicy.blockedHexSsids &&
         globalPolicy.blockedHexSsids.includes(hexSsid));
  },

  /**
   * @param {!mojom.ManagedProperties} managedProperties
   * @param {!mojom.GlobalPolicy} globalPolicy
   * @param {boolean} managedNetworkAvailable
   * @param {?OncMojo.DeviceStateProperties} deviceState
   * @return {boolean}
   * @private
   */
  showConnect_: function(
      managedProperties, globalPolicy, managedNetworkAvailable, deviceState) {
    if (!managedProperties) {
      return false;
    }

    if (this.isBlockedByPolicy_(
            managedProperties, globalPolicy, managedNetworkAvailable)) {
      return false;
    }

    // TODO(lgcheng@) support connect Arc VPN from UI once Android support API
    // to initiate a VPN session.
    if (this.isArcVpn_(managedProperties)) {
      return false;
    }

    if (managedProperties.connectionState !=
        mojom.ConnectionStateType.kNotConnected) {
      return false;
    }

    if (deviceState &&
        deviceState.deviceState !=
            chromeos.networkConfig.mojom.DeviceStateType.kEnabled) {
      return false;
    }

    // Cellular is not configurable, so we always show the connect button, and
    // disable it if 'connectable' is false.
    if (managedProperties.type == mojom.NetworkType.kCellular) {
      return true;
    }

    // If 'connectable' is false we show the configure button.
    return managedProperties.connectable &&
        managedProperties.type != mojom.NetworkType.kEthernet;
  },

  /**
   * @param {!mojom.ManagedProperties} managedProperties
   * @return {boolean}
   * @private
   */
  showDisconnect_: function(managedProperties) {
    if (!managedProperties ||
        managedProperties.type == mojom.NetworkType.kEthernet) {
      return false;
    }
    return managedProperties.connectionState !=
        mojom.ConnectionStateType.kNotConnected;
  },

  /**
   * @param {!mojom.ManagedProperties} managedProperties
   * @return {boolean}
   * @private
   */
  showForget_: function(managedProperties) {
    if (!managedProperties || this.isSecondaryUser_) {
      return false;
    }
    const type = managedProperties.type;
    if (type != mojom.NetworkType.kWiFi && type != mojom.NetworkType.kVPN) {
      return false;
    }
    if (this.isArcVpn_(managedProperties)) {
      return false;
    }
    return !this.isPolicySource(managedProperties.source) &&
        this.isRemembered_(managedProperties);
  },

  /**
   * @param {!mojom.ManagedProperties} managedProperties
   * @return {boolean}
   * @private
   */
  showActivate_: function(managedProperties) {
    if (!managedProperties || this.isSecondaryUser_) {
      return false;
    }
    if (!this.isCellular_(managedProperties)) {
      return false;
    }
    const activation =
        managedProperties.typeProperties.cellular.activationState;
    return activation == mojom.ActivationStateType.kNotActivated ||
        activation == mojom.ActivationStateType.kPartiallyActivated;
  },

  /**
   * @param {!mojom.ManagedProperties} managedProperties
   * @param {!mojom.GlobalPolicy} globalPolicy
   * @param {boolean} managedNetworkAvailable
   * @return {boolean}
   * @private
   */
  showConfigure_: function(
      managedProperties, globalPolicy, managedNetworkAvailable) {
    if (!managedProperties || this.isSecondaryUser_) {
      return false;
    }
    if (this.isBlockedByPolicy_(
            managedProperties, globalPolicy, managedNetworkAvailable)) {
      return false;
    }
    const type = managedProperties.type;
    if (type == mojom.NetworkType.kCellular ||
        type == mojom.NetworkType.kTether) {
      return false;
    }
    if (type == mojom.NetworkType.kWiFi &&
        managedProperties.typeProperties.wifi.security ==
            mojom.SecurityType.kNone) {
      return false;
    }
    if (type == mojom.NetworkType.kWiFi &&
        (managedProperties.connectionState !=
         mojom.ConnectionStateType.kNotConnected)) {
      return false;
    }
    if (this.isArcVpn_(managedProperties) &&
        !this.isConnectedState_(managedProperties)) {
      return false;
    }
    return true;
  },

  /**
   * @param {!mojom.ManagedProperties} managedProperties
   * @param {!chrome.settingsPrivate.PrefObject} vpnConfigAllowed
   * @return {boolean}
   * @private
   */
  disableForget_: function(managedProperties, vpnConfigAllowed) {
    if (!managedProperties) {
      return true;
    }
    return managedProperties.type == mojom.NetworkType.kVPN &&
        vpnConfigAllowed && !vpnConfigAllowed.value;
  },

  /**
   * @param {!mojom.ManagedProperties} managedProperties
   * @param {!chrome.settingsPrivate.PrefObject} vpnConfigAllowed
   * @return {boolean}
   * @private
   */
  disableConfigure_: function(managedProperties, vpnConfigAllowed) {
    if (!managedProperties) {
      return true;
    }
    if (managedProperties.type == mojom.NetworkType.kVPN && vpnConfigAllowed &&
        !vpnConfigAllowed.value) {
      return true;
    }
    return this.isPolicySource(managedProperties.source) &&
        !this.hasRecommendedFields_(managedProperties);
  },


  /**
   * @param {!mojom.ManagedProperties} managedProperties
   * @return {boolean}
   */
  hasRecommendedFields_: function(managedProperties) {
    if (!managedProperties) {
      return false;
    }
    for (const key of Object.keys(managedProperties)) {
      const value = managedProperties[key];
      if (typeof value != 'object' || value === null) {
        continue;
      }
      if ('activeValue' in value) {
        if (this.isNetworkPolicyRecommended(value)) {
          return true;
        }
      } else if (this.hasRecommendedFields_(value)) {
        return true;
      }
    }
    return false;
  },

  /**
   * @param {!mojom.ManagedProperties} managedProperties
   * @return {boolean}
   * @private
   */
  showViewAccount_: function(managedProperties) {
    if (!managedProperties || this.isSecondaryUser_) {
      return false;
    }

    // Show either the 'Activate' or the 'View Account' button (Cellular only).
    if (!this.isCellular_(managedProperties) ||
        this.showActivate_(managedProperties)) {
      return false;
    }

    const paymentPortal =
        managedProperties.typeProperties.cellular.paymentPortal;
    if (!paymentPortal || !paymentPortal.url) {
      return false;
    }

    // Only show for connected networks or LTE networks with a valid MDN.
    if (!this.isConnectedState_(managedProperties)) {
      const technology =
          managedProperties.typeProperties.cellular.networkTechnology;
      if (technology != 'LTE' && technology != 'LTEAdvanced') {
        return false;
      }
      if (!managedProperties.typeProperties.cellular.mdn) {
        return false;
      }
    }

    return true;
  },

  /**
   * @param {!mojom.ManagedProperties} managedProperties
   * @param {?OncMojo.NetworkStateProperties} defaultNetwork
   * @param {boolean} propertiesReceived
   * @param {boolean} outOfRange
   * @param {!mojom.GlobalPolicy} globalPolicy
   * @param {boolean} managedNetworkAvailable
   * @param {?OncMojo.DeviceStateProperties} deviceState
   * @return {boolean} Whether or not to enable the network connect button.
   * @private
   */
  enableConnect_: function(
      managedProperties, defaultNetwork, propertiesReceived, outOfRange,
      globalPolicy, managedNetworkAvailable, deviceState) {
    if (!this.showConnect_(
            managedProperties, globalPolicy, managedNetworkAvailable,
            deviceState)) {
      return false;
    }
    if (!propertiesReceived || outOfRange) {
      return false;
    }
    // Cellular networks are not configurable, so we show a disabled 'Connect'
    // button when not connectable.
    if (managedProperties.type == mojom.NetworkType.kCellular &&
        !managedProperties.connectable) {
      return false;
    }
    if (managedProperties.type == mojom.NetworkType.kVPN && !defaultNetwork) {
      return false;
    }
    return true;
  },

  /** @private */
  updateAlwaysOnVpnPrefValue_: function() {
    this.alwaysOnVpn_.value = this.prefs.arc && this.prefs.arc.vpn &&
        this.prefs.arc.vpn.always_on && this.prefs.arc.vpn.always_on.lockdown &&
        this.prefs.arc.vpn.always_on.lockdown.value;
  },

  /**
   * @private
   * @return {!chrome.settingsPrivate.PrefObject}
   */
  getFakeVpnConfigPrefForEnforcement_: function() {
    const fakeAlwaysOnVpnEnforcementPref = {
      key: 'fakeAlwaysOnPref',
      type: chrome.settingsPrivate.PrefType.BOOLEAN,
      value: false,
    };
    // Only mark VPN networks as enforced. This fake pref also controls the
    // policy indicator on the connect/disconnect buttons, so it shouldn't be
    // shown on non-VPN networks.
    if (this.managedProperties_ &&
        this.managedProperties_.type == mojom.NetworkType.kVPN && this.prefs &&
        this.prefs.vpn_config_allowed && !this.prefs.vpn_config_allowed.value) {
      fakeAlwaysOnVpnEnforcementPref.enforcement =
          chrome.settingsPrivate.Enforcement.ENFORCED;
      fakeAlwaysOnVpnEnforcementPref.controlledBy =
          this.prefs.vpn_config_allowed.controlledBy;
    }
    return fakeAlwaysOnVpnEnforcementPref;
  },

  /** @private */
  updateAlwaysOnVpnPrefEnforcement_: function() {
    const prefForEnforcement = this.getFakeVpnConfigPrefForEnforcement_();
    this.alwaysOnVpn_.enforcement = prefForEnforcement.enforcement;
    this.alwaysOnVpn_.controlledBy = prefForEnforcement.controlledBy;
  },

  /**
   * @return {!TetherConnectionDialogElement}
   * @private
   */
  getTetherDialog_: function() {
    return /** @type {!TetherConnectionDialogElement} */ (
        this.$$('#tetherDialog'));
  },

  /** @private */
  onConnectTap_: function() {
    if (this.managedProperties_.type == mojom.NetworkType.kTether &&
        (!this.managedProperties_.typeProperties.tether.hasConnectedToHost)) {
      this.showTetherDialog_();
      return;
    }
    this.fireNetworkConnect_(/*bypassDialog=*/ false);
  },

  /** @private */
  onTetherConnect_: function() {
    this.getTetherDialog_().close();
    this.fireNetworkConnect_(/*bypassDialog=*/ true);
  },

  /**
   * @param {boolean} bypassDialog
   * @private
   */
  fireNetworkConnect_: function(bypassDialog) {
    assert(this.managedProperties_);
    const networkState =
        OncMojo.managedPropertiesToNetworkState(this.managedProperties_);
    this.fire(
        'network-connect',
        {networkState: networkState, bypassConnectionDialog: bypassDialog});
  },

  /** @private */
  onDisconnectTap_: function() {
    this.networkConfig_.startDisconnect(this.guid).then(response => {
      if (!response.success) {
        console.error('Disconnect failed for: ' + this.guid);
      }
    });
  },

  /** @private */
  onForgetTap_: function() {
    this.networkConfig_.forgetNetwork(this.guid).then(response => {
      if (!response.success) {
        console.error('Froget network failed for: ' + this.guid);
      }
      // A forgotten network no longer has a valid GUID, close the subpage.
      this.close();
    });
  },

  /** @private */
  onActivateTap_: function() {
    this.browserProxy_.showCellularSetupUI(this.guid);
  },

  /** @private */
  onConfigureTap_: function() {
    if (this.managedProperties_ &&
        (this.isThirdPartyVpn_(this.managedProperties_) ||
         this.isArcVpn_(this.managedProperties_))) {
      this.browserProxy_.configureThirdPartyVpn(this.guid);
      return;
    }

    this.fire('show-config', {
      guid: this.guid,
      type: OncMojo.getNetworkTypeString(this.managedProperties_.type),
      name: OncMojo.getNetworkName(this.managedProperties_)
    });
  },

  /** @private */
  onViewAccountTap_: function() {
    // Currently 'Account Details' is the same as the activation UI.
    this.browserProxy_.showCellularSetupUI(this.guid);
  },

  /** @type {string} */
  CR_EXPAND_BUTTON_TAG: 'CR-EXPAND-BUTTON',

  /** @private */
  showTetherDialog_: function() {
    this.getTetherDialog_().open();
  },

  /**
   * @return {boolean}
   * @private
   */
  showHiddenNetworkWarning_: function() {
    return loadTimeData.getBoolean('showHiddenNetworkWarning') &&
        !!this.autoConnectPref_ && !!this.autoConnectPref_.value &&
        !!this.managedProperties_ &&
        this.managedProperties_.type == mojom.NetworkType.kWiFi &&
        !!OncMojo.getActiveValue(
            this.managedProperties_.typeProperties.wifi.hiddenSsid);
  },

  /**
   * Event triggered for elements associated with network properties.
   * @param {!CustomEvent<!{
   *     field: string,
   *     value: (string|number|boolean|!Array<string>)
   * }>} e
   * @private
   */
  onNetworkPropertyChange_: function(e) {
    if (!this.propertiesReceived_) {
      return;
    }
    const field = e.detail.field;
    const value = e.detail.value;
    const config = this.getDefaultConfigProperties_();
    const valueType = typeof value;
    if (valueType != 'string' && valueType != 'number' &&
        valueType != 'boolean' && !Array.isArray(value)) {
      console.error(
          'Unexpected property change event, Key: ' + field +
          ' Value: ' + JSON.stringify(value));
      return;
    }
    OncMojo.setConfigProperty(config, field, value);
    // Ensure that any required configuration properties for partial
    // configurations are set.
    const vpnConfig = config.typeConfig.vpn;
    if (vpnConfig) {
      vpnConfig.type = this.managedProperties_.typeProperties.vpn.type;
      if (vpnConfig.openVpn && vpnConfig.openVpn.saveCredentials == undefined) {
        vpnConfig.openVpn.saveCredentials = false;
      }
      if (vpnConfig.l2tp && vpnConfig.l2tp.saveCredentials == undefined) {
        vpnConfig.l2tp.saveCredentials = false;
      }
    }
    this.setMojoNetworkProperties_(config);
  },

  /**
   * @param {!CustomEvent<!mojom.ApnProperties>} event
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
   *     value: (string|!mojom.IPConfigProperties|!Array<string>)
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
   * @param {!CustomEvent<!mojom.ProxySettings>} event
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
   * @param {!mojom.ManagedProperties} managedProperties
   * @param {!mojom.GlobalPolicy} globalPolicy
   * @param {boolean} managedNetworkAvailable
   * @return {boolean} True if the shared message should be shown.
   * @private
   */
  showShared_: function(
      managedProperties, globalPolicy, managedNetworkAvailable) {
    return !!managedProperties &&
        (managedProperties.source == mojom.OncSource.kDevice ||
         managedProperties.source == mojom.OncSource.kDevicePolicy) &&
        !this.isBlockedByPolicy_(
            managedProperties, globalPolicy, managedNetworkAvailable);
  },

  /**
   * @param {!mojom.ManagedProperties} managedProperties
   * @param {!mojom.GlobalPolicy} globalPolicy
   * @param {boolean} managedNetworkAvailable
   * @return {boolean} True if the AutoConnect checkbox should be shown.
   * @private
   */
  showAutoConnect_: function(
      managedProperties, globalPolicy, managedNetworkAvailable) {
    return !!managedProperties &&
        managedProperties.type != mojom.NetworkType.kEthernet &&
        this.isRemembered_(managedProperties) &&
        !this.isArcVpn_(managedProperties) &&
        !this.isBlockedByPolicy_(
            managedProperties, globalPolicy, managedNetworkAvailable);
  },

  /**
   * @param {!mojom.ManagedProperties} managedProperties
   * @return {boolean} Whether the toggle for the Always-on VPN feature is
   * displayed.
   * @private
   */
  showAlwaysOnVpn_: function(managedProperties) {
    return this.isArcVpn_(managedProperties) && this.prefs.arc &&
        this.prefs.arc.vpn && this.prefs.arc.vpn.always_on &&
        this.prefs.arc.vpn.always_on.vpn_package &&
        OncMojo.getActiveValue(managedProperties.typeProperties.vpn.host) ===
        this.prefs.arc.vpn.always_on.vpn_package.value;
  },

  /** @private */
  alwaysOnVpnChanged_: function() {
    if (this.prefs && this.prefs.arc && this.prefs.arc.vpn &&
        this.prefs.arc.vpn.always_on && this.prefs.arc.vpn.always_on.lockdown) {
      this.set(
          'prefs.arc.vpn.always_on.lockdown.value',
          !!this.alwaysOnVpn_ && this.alwaysOnVpn_.value);
    }
  },

  /**
   * @param {!mojom.ManagedProperties} managedProperties
   * @param {!mojom.GlobalPolicy} globalPolicy
   * @param {boolean} managedNetworkAvailable
   * @return {boolean} True if the prefer network checkbox should be shown.
   * @private
   */
  showPreferNetwork_: function(
      managedProperties, globalPolicy, managedNetworkAvailable) {
    if (!managedProperties) {
      return false;
    }

    const type = managedProperties.type;
    if (type == mojom.NetworkType.kEthernet ||
        type == mojom.NetworkType.kCellular ||
        this.isArcVpn_(managedProperties)) {
      return false;
    }

    return this.isRemembered_(managedProperties) &&
        !this.isBlockedByPolicy_(
            managedProperties, globalPolicy, managedNetworkAvailable);
  },

  /**
   * @param {Event} event
   * @private
   */
  onPreferNetworkRowClicked_: function(event) {
    // Stop propagation because the toggle and policy indicator handle clicks
    // themselves.
    event.stopPropagation();
    const preferNetworkToggle =
        this.shadowRoot.querySelector('#preferNetworkToggle');
    if (!preferNetworkToggle || preferNetworkToggle.disabled) {
      return;
    }

    this.preferNetwork_ = !this.preferNetwork_;
  },

  /**
   * @param {!Array<string>} fields
   * @return {boolean}
   * @private
   */
  hasVisibleFields_: function(fields) {
    for (let i = 0; i < fields.length; ++i) {
      const key = OncMojo.getManagedPropertyKey(fields[i]);
      const value = this.get(key, this.managedProperties_);
      if (value !== undefined && value !== '') {
        return true;
      }
    }
    return false;
  },

  /**
   * @return {boolean}
   * @private
   */
  hasInfoFields_: function() {
    return this.getInfoEditFieldTypes_().length > 0 ||
        this.hasVisibleFields_(this.getInfoFields_());
  },

  /**
   * @return {!Array<string>} The fields to display in the info section.
   * @private
   */
  getInfoFields_: function() {
    if (!this.managedProperties_) {
      return [];
    }

    /** @type {!Array<string>} */ const fields = [];
    switch (this.managedProperties_.type) {
      case mojom.NetworkType.kCellular:
        fields.push(
            'cellular.activationState', 'cellular.servingOperator.name');
        if (this.managedProperties_.restrictedConnectivity) {
          fields.push('restrictedConnectivity');
        }
        break;
      case mojom.NetworkType.kTether:
        fields.push(
            'tether.batteryPercentage', 'tether.signalStrength',
            'tether.carrier');
        break;
      case mojom.NetworkType.kVPN:
        const vpnType = this.managedProperties_.typeProperties.vpn.type;
        switch (vpnType) {
          case mojom.VpnType.kExtension:
            fields.push('vpn.providerName');
            break;
          case mojom.VpnType.kArc:
            fields.push('vpn.type');
            fields.push('vpn.providerName');
            break;
          case mojom.VpnType.kOpenVPN:
            fields.push(
                'vpn.type', 'vpn.host', 'vpn.openVpn.username',
                'vpn.openVpn.extraHosts');
            break;
          case mojom.VpnType.kL2TPIPsec:
            fields.push('vpn.type', 'vpn.host', 'vpn.l2tp.username');
            break;
        }
        break;
      case mojom.NetworkType.kWiFi:
        if (this.managedProperties_.restrictedConnectivity) {
          fields.push('restrictedConnectivity');
        }
        break;
    }
    return fields;
  },

  /**
   * Provides the list of editable fields to <network-property-list>.
   * NOTE: Entries added to this list must be reflected in ConfigProperties in
   * chromeos.network_config.mojom and handled in the service implementation.
   * @return {!Object} A dictionary of editable fields in the info section.
   * @private
   */
  getInfoEditFieldTypes_: function() {
    if (!this.managedProperties_) {
      return [];
    }

    /** @dict */ const editFields = {};
    const type = this.managedProperties_.type;
    if (type == mojom.NetworkType.kVPN) {
      const vpnType = this.managedProperties_.typeProperties.vpn.type;
      if (vpnType != mojom.VpnType.kExtension) {
        editFields['vpn.host'] = 'String';
      }
      if (vpnType == mojom.VpnType.kOpenVPN) {
        editFields['vpn.openVpn.username'] = 'String';
        editFields['vpn.openVpn.extraHosts'] = 'StringArray';
      }
    }
    return editFields;
  },

  /**
   * @return {!Array<string>} The fields to display in the Advanced section.
   * @private
   */
  getAdvancedFields_: function() {
    if (!this.managedProperties_) {
      return [];
    }

    /** @type {!Array<string>} */ const fields = [];
    const type = this.managedProperties_.type;
    switch (type) {
      case mojom.NetworkType.kCellular:
        fields.push(
            'cellular.family', 'cellular.networkTechnology',
            'cellular.servingOperator.code');
        break;
      case mojom.NetworkType.kWiFi:
        fields.push(
            'wifi.ssid', 'wifi.bssid', 'wifi.signalStrength', 'wifi.security',
            'wifi.eap.outer', 'wifi.eap.inner', 'wifi.eap.subjectMatch',
            'wifi.eap.identity', 'wifi.eap.anonymousIdentity',
            'wifi.frequency');
        break;
    }
    return fields;
  },

  /**
   * @return {!Array<string>} The fields to display in the device section.
   * @private
   */
  getDeviceFields_: function() {
    if (!this.managedProperties_ ||
        this.managedProperties_.type !== mojom.NetworkType.kCellular) {
      return [];
    }

    return [
      'cellular.homeProvider.name', 'cellular.homeProvider.country',
      'cellular.homeProvider.code', 'cellular.manufacturer', 'cellular.modelId',
      'cellular.firmwareRevision', 'cellular.hardwareRevision', 'cellular.esn',
      'cellular.iccid', 'cellular.imei', 'cellular.imsi', 'cellular.mdn',
      'cellular.meid', 'cellular.min'
    ];
  },

  /**
   * @param {!mojom.ManagedProperties} managedProperties
   * @param {boolean} propertiesReceived
   * @return {boolean}
   * @private
   */
  showAdvanced_: function(managedProperties, propertiesReceived) {
    if (!managedProperties || !propertiesReceived) {
      return false;
    }
    if (managedProperties.type == mojom.NetworkType.kTether) {
      // These settings apply to the underlying WiFi network, not the Tether
      // network.
      return false;
    }
    return this.hasAdvancedFields_() || this.hasDeviceFields_();
  },

  /**
   * @return {boolean}
   * @private
   */
  hasAdvancedFields_: function() {
    return this.hasVisibleFields_(this.getAdvancedFields_());
  },

  /**
   * @return {boolean}
   * @private
   */
  hasDeviceFields_: function() {
    return this.hasVisibleFields_(this.getDeviceFields_());
  },

  /**
   * @return {boolean}
   * @private
   */
  hasAdvancedOrDeviceFields_: function() {
    return this.hasAdvancedFields_() || this.hasDeviceFields_();
  },

  /**
   * @param {!mojom.ManagedProperties} managedProperties
   * @param {!mojom.GlobalPolicy} globalPolicy
   * @param {boolean} managedNetworkAvailable
   * @return {boolean}
   * @private
   */
  hasNetworkSection_: function(
      managedProperties, globalPolicy, managedNetworkAvailable) {
    if (!managedProperties ||
        managedProperties.type == mojom.NetworkType.kTether) {
      // These settings apply to the underlying WiFi network, not the Tether
      // network.
      return false;
    }
    if (this.isBlockedByPolicy_(
            managedProperties, globalPolicy, managedNetworkAvailable)) {
      return false;
    }
    if (managedProperties.type == mojom.NetworkType.kCellular) {
      return true;
    }
    return this.isRememberedOrConnected_(managedProperties);
  },

  /**
   * @param {!mojom.ManagedProperties} managedProperties
   * @param {!mojom.GlobalPolicy} globalPolicy
   * @param {boolean} managedNetworkAvailable
   * @return {boolean}
   * @private
   */
  hasProxySection_: function(
      managedProperties, globalPolicy, managedNetworkAvailable) {
    if (!managedProperties ||
        managedProperties.type == mojom.NetworkType.kTether) {
      // Proxy settings apply to the underlying WiFi network, not the Tether
      // network.
      return false;
    }
    if (this.isBlockedByPolicy_(
            managedProperties, globalPolicy, managedNetworkAvailable)) {
      return false;
    }
    return this.isRememberedOrConnected_(managedProperties);
  },

  /**
   * @param {!mojom.ManagedProperties} managedProperties
   * @return {boolean}
   * @private
   */
  showCellularChooseNetwork_: function(managedProperties) {
    return !!managedProperties &&
        managedProperties.type == mojom.NetworkType.kCellular &&
        managedProperties.typeProperties.cellular.supportNetworkScan;
  },

  /**
   * @return {boolean}
   * @private
   */
  showScanningSpinner_: function() {
    if (!this.managedProperties_ ||
        this.managedProperties_.type != mojom.NetworkType.kCellular) {
      return false;
    }
    return !!this.deviceState_ && this.deviceState_.scanning;
  },

  /**
   * @param {!mojom.ManagedProperties} managedProperties
   * @return {boolean}
   * @private
   */
  showCellularSim_: function(managedProperties) {
    return !!managedProperties &&
        managedProperties.type == mojom.NetworkType.kCellular &&
        managedProperties.typeProperties.cellular.family != 'CDMA';
  },

  /**
   * @param {!mojom.ManagedProperties|undefined} managedProperties
   * @return {boolean}
   * @private
   */
  isArcVpn_: function(managedProperties) {
    return !!managedProperties &&
        managedProperties.type == mojom.NetworkType.kVPN &&
        managedProperties.typeProperties.vpn.type == mojom.VpnType.kArc;
  },

  /**
   * @param {!mojom.ManagedProperties|undefined} managedProperties
   * @return {boolean}
   * @private
   */
  isThirdPartyVpn_: function(managedProperties) {
    return !!managedProperties &&
        managedProperties.type == mojom.NetworkType.kVPN &&
        managedProperties.typeProperties.vpn.type == mojom.VpnType.kExtension;
  },

  /**
   * @param {string} ipAddress
   * @param {!mojom.ManagedProperties} managedProperties
   * @return {boolean}
   * @private
   */
  showIpAddress_: function(ipAddress, managedProperties) {
    // Arc Vpn does not currently pass IP configuration to ChromeOS. IP address
    // property holds an internal IP address Android uses to talk to ChromeOS.
    // TODO(lgcheng@) Show correct IP address when we implement IP configuration
    // correctly.
    if (this.isArcVpn_(managedProperties)) {
      return false;
    }

    // Cellular IP addresses are shown under the network details section.
    if (this.isCellular_(managedProperties)) {
      return false;
    }

    return !!ipAddress && this.isConnectedState_(managedProperties);
  },

  /**
   * @param {!Object} curValue
   * @param {!Object} newValue
   * @return {boolean} True if all properties set in |newValue| are equal to
   *     the corresponding properties in |curValue|. Note: Not all properties
   *     of |curValue| need to be specified in |newValue| for this to return
   *     true.
   * @private
   */
  allPropertiesMatch_: function(curValue, newValue) {
    for (const key in newValue) {
      if (newValue[key] != curValue[key]) {
        return false;
      }
    }
    return true;
  },

  /**
   * @param {boolean} outOfRange
   * @param {?OncMojo.DeviceStateProperties} deviceState
   * @return {boolean}
   * @private
   */
  isOutOfRangeOrNotEnabled_: function(outOfRange, deviceState) {
    return outOfRange ||
        (!!deviceState &&
         deviceState.deviceState !=
             chromeos.networkConfig.mojom.DeviceStateType.kEnabled);
  },
});
})();
