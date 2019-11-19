// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Polymer element for displaying information about WiFi,
 * Cellular, or virtual networks.
 */

(function() {

const mojom = chromeos.networkConfig.mojom;

Polymer({
  is: 'settings-internet-subpage',

  behaviors: [
    NetworkListenerBehavior,
    CrPolicyNetworkBehaviorMojo,
    settings.RouteObserverBehavior,
    I18nBehavior,
  ],

  properties: {
    /**
     * Highest priority connected network or null. Provided by
     * settings-internet-page (but set in network-summary).
     * @type {?OncMojo.NetworkStateProperties|undefined}
     */
    defaultNetwork: Object,

    /**
     * Device state for the network type. Note: when both Cellular and Tether
     * are available this will always be set to the Cellular device state and
     * |tetherDeviceState| will be set to the Tether device state.
     * @type {!OncMojo.DeviceStateProperties|undefined}
     */
    deviceState: Object,

    /**
     * If both Cellular and Tether technologies exist, we combine the subpages
     * and set this to the device state for Tether.
     * @type {!OncMojo.DeviceStateProperties|undefined}
     */
    tetherDeviceState: Object,

    /** @type {!chromeos.networkConfig.mojom.GlobalPolicy|undefined} */
    globalPolicy: Object,

    /**
     * List of third party (Extension + Arc) VPN providers.
     * @type {!Array<!chromeos.networkConfig.mojom.VpnProvider>}
     */
    vpnProviders: Array,

    showSpinner: {
      type: Boolean,
      notify: true,
      value: false,
    },

    /**
     * List of all network state data for the network type.
     * @private {!Array<!OncMojo.NetworkStateProperties>}
     */
    networkStateList_: {
      type: Array,
      value: function() {
        return [];
      },
    },

    /**
     * Dictionary of lists of network states for third party VPNs.
     * @private {!Object<!Array<!OncMojo.NetworkStateProperties>>}
     */
    thirdPartyVpns_: {
      type: Object,
      value: function() {
        return {};
      },
    },

    /**
     * List of potential Tether hosts whose "Google Play Services" notifications
     * are disabled (these notifications are required to use Instant Tethering).
     * @private {!Array<string>}
     */
    notificationsDisabledDeviceNames_: {
      type: Array,
      value: function() {
        return [];
      },
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
    hasCompletedScanSinceLastEnabled_: {
      type: Boolean,
      value: false,
    },
  },

  observers: ['deviceStateChanged_(deviceState)'],

  /** @private {number|null} */
  scanIntervalId_: null,

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

  /** @override */
  ready: function() {
    this.browserProxy_.setGmsCoreNotificationsDisabledDeviceNamesCallback(
        this.onNotificationsDisabledDeviceNamesReceived_.bind(this));
    this.browserProxy_.requestGmsCoreNotificationsDisabledDeviceNames();
  },

  /** override */
  detached: function() {
    this.stopScanning_();
  },

  /**
   * settings.RouteObserverBehavior
   * @param {!settings.Route} route
   * @protected
   */
  currentRouteChanged: function(route) {
    if (route != settings.routes.INTERNET_NETWORKS) {
      this.stopScanning_();
      return;
    }
    this.init();
  },

  init: function() {
    // Clear any stale data.
    this.networkStateList_ = [];
    this.thirdPartyVpns_ = {};
    this.hasCompletedScanSinceLastEnabled_ = false;
    this.showSpinner = false;

    // Request the list of networks and start scanning if necessary.
    this.getNetworkStateList_();
    this.updateScanning_();
  },

  /**
   * NetworkListenerBehavior override
   * @param {!Array<OncMojo.NetworkStateProperties>} networks
   */
  onActiveNetworksChanged: function(networks) {
    this.getNetworkStateList_();
  },

  /** NetworkListenerBehavior override */
  onNetworkStateListChanged: function() {
    this.getNetworkStateList_();
  },

  /** NetworkListenerBehavior override */
  onVpnProvidersChanged: function() {
    if (this.deviceState.type != mojom.NetworkType.kVPN) {
      return;
    }
    this.getNetworkStateList_();
  },

  /** @private */
  deviceStateChanged_: function() {
    if (this.deviceState !== undefined) {
      // A scan has completed if the spinner was active (i.e., scanning was
      // active) and the device is no longer scanning.
      this.hasCompletedScanSinceLastEnabled_ = this.showSpinner &&
          !this.deviceState.scanning &&
          this.deviceState.deviceState == mojom.DeviceStateType.kEnabled;
      this.showSpinner = !!this.deviceState.scanning;
    }

    // Scans should only be triggered by the "networks" subpage.
    if (settings.getCurrentRoute() != settings.routes.INTERNET_NETWORKS) {
      this.stopScanning_();
      return;
    }

    this.getNetworkStateList_();
    this.updateScanning_();
  },

  /** @private */
  updateScanning_: function() {
    if (!this.deviceState) {
      return;
    }

    if (this.shouldStartScan_()) {
      this.startScanning_();
      return;
    }
  },

  /**
   * @return {boolean}
   * @private
   */
  shouldStartScan_: function() {
    // Scans should be kicked off from the Wi-Fi networks subpage.
    if (this.deviceState.type == mojom.NetworkType.kWiFi) {
      return true;
    }

    // Scans should be kicked off from the Mobile data subpage, as long as it
    // includes Tether networks.
    if (this.deviceState.type == mojom.NetworkType.kTether ||
        (this.deviceState.type == mojom.NetworkType.kCellular &&
         this.tetherDeviceState)) {
      return true;
    }

    return false;
  },

  /** @private */
  startScanning_: function() {
    if (this.scanIntervalId_ != null) {
      return;
    }
    const INTERVAL_MS = 10 * 1000;
    let type = this.deviceState.type;
    if (type == mojom.NetworkType.kCellular && this.tetherDeviceState) {
      type = mojom.NetworkType.kMobile;
    }
    this.networkConfig_.requestNetworkScan(type);
    this.scanIntervalId_ = window.setInterval(() => {
      this.networkConfig_.requestNetworkScan(type);
    }, INTERVAL_MS);
  },

  /** @private */
  stopScanning_: function() {
    if (this.scanIntervalId_ == null) {
      return;
    }
    window.clearInterval(this.scanIntervalId_);
    this.scanIntervalId_ = null;
  },

  /** @private */
  getNetworkStateList_: function() {
    if (!this.deviceState) {
      return;
    }
    const filter = {
      filter: chromeos.networkConfig.mojom.FilterType.kVisible,
      limit: chromeos.networkConfig.mojom.NO_LIMIT,
      networkType: this.deviceState.type,
    };
    this.networkConfig_.getNetworkStateList(filter).then(response => {
      this.onGetNetworks_(response.result);
    });
  },

  /**
   * @param {!Array<!OncMojo.NetworkStateProperties>} networkStates
   * @private
   */
  onGetNetworks_: function(networkStates) {
    if (!this.deviceState) {
      // Edge case when device states change before this callback.
      return;
    }

    // For the Cellular/Mobile subpage, also request Tether networks.
    if (this.deviceState.type == mojom.NetworkType.kCellular &&
        this.tetherDeviceState) {
      const filter = {
        filter: chromeos.networkConfig.mojom.FilterType.kVisible,
        limit: chromeos.networkConfig.mojom.NO_LIMIT,
        networkType: mojom.NetworkType.kTether,
      };
      this.networkConfig_.getNetworkStateList(filter).then(response => {
        const tetherNetworkStates = response.result;
        this.networkStateList_ = networkStates.concat(tetherNetworkStates);
      });
      return;
    }

    // For VPNs, separate out third party (Extension + Arc) VPNs.
    if (this.deviceState.type == mojom.NetworkType.kVPN) {
      const builtinNetworkStates = [];
      const thirdPartyVpns = {};
      networkStates.forEach(state => {
        assert(state.type == mojom.NetworkType.kVPN);
        switch (state.typeState.vpn.type) {
          case mojom.VpnType.kL2TPIPsec:
          case mojom.VpnType.kOpenVPN:
            builtinNetworkStates.push(state);
            break;
          case mojom.VpnType.kArc:
            // Only show connected Arc VPNs.
            if (!OncMojo.connectionStateIsConnected(state.connectionState)) {
              break;
            }
            // Otherwise Arc VPNs are treated the same as Extension VPNs.
          case mojom.VpnType.kExtension:
            const providerId = state.typeState.vpn.providerId;
            thirdPartyVpns[providerId] = thirdPartyVpns[providerId] || [];
            thirdPartyVpns[providerId].push(state);
            break;
        }
      });
      networkStates = builtinNetworkStates;
      this.thirdPartyVpns_ = thirdPartyVpns;
    }

    this.networkStateList_ = networkStates;
  },

  /**
   * Returns an ordered list of VPN providers for all third party VPNs and any
   * other known providers.
   * @param {!Array<!chromeos.networkConfig.mojom.VpnProvider>} vpnProviders
   * @param {!Object<!Array<!OncMojo.NetworkStateProperties>>} thirdPartyVpns
   * @return {!Array<!chromeos.networkConfig.mojom.VpnProvider>}
   * @private
   */
  getVpnProviders_(vpnProviders, thirdPartyVpns) {
    // First add providers for configured thirdPartyVpns. This list will
    // generally be empty or small.
    const configuredProviders = [];
    for (const vpnList of Object.values(thirdPartyVpns)) {
      assert(vpnList.length > 0);
      // All vpns in the list will have the same type and provider id.
      const vpn = vpnList[0].typeState.vpn;
      const provider = {
        type: vpn.type,
        providerId: vpn.providerId,
        providerName: vpn.providerName || vpn.providerId,
        appId: '',
        lastLaunchTime: {internalValue: 0}
      };
      configuredProviders.push(provider);
    }
    // Next update or append known third party providers.
    const unconfiguredProviders = [];
    for (const provider of vpnProviders) {
      const idx = configuredProviders.findIndex(
          p => p.providerId == provider.providerId);
      if (idx >= 0) {
        configuredProviders[idx] = provider;
      } else {
        unconfiguredProviders.push(provider);
      }
    }
    return configuredProviders.concat(unconfiguredProviders);
  },

  /**
   * @param {!Array<string>} notificationsDisabledDeviceNames
   * @private
   */
  onNotificationsDisabledDeviceNamesReceived_: function(
      notificationsDisabledDeviceNames) {
    this.notificationsDisabledDeviceNames_ = notificationsDisabledDeviceNames;
  },

  /**
   * @param {!OncMojo.DeviceStateProperties|undefined} deviceState
   * @return {boolean} Whether or not the device state is enabled.
   * @private
   */
  deviceIsEnabled_: function(deviceState) {
    return !!deviceState &&
        deviceState.deviceState ==
        chromeos.networkConfig.mojom.DeviceStateType.kEnabled;
  },

  /**
   * @param {!OncMojo.DeviceStateProperties|undefined} deviceState
   * @param {string} onstr
   * @param {string} offstr
   * @return {string}
   * @private
   */
  getOffOnString_: function(deviceState, onstr, offstr) {
    return this.deviceIsEnabled_(deviceState) ? onstr : offstr;
  },

  /**
   * @param {!OncMojo.DeviceStateProperties|undefined} deviceState
   * @return {boolean}
   * @private
   */
  enableToggleIsVisible_: function(deviceState) {
    return !!deviceState && deviceState.type != mojom.NetworkType.kEthernet &&
        deviceState.type != mojom.NetworkType.kVPN;
  },

  /**
   * @param {!OncMojo.DeviceStateProperties|undefined} deviceState
   * @return {boolean}
   * @private
   */
  enableToggleIsEnabled_: function(deviceState) {
    return !!deviceState &&
        deviceState.deviceState !=
        chromeos.networkConfig.mojom.DeviceStateType.kProhibited;
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
   * @param {!mojom.VpnProvider} provider
   * @return {string}
   * @private
   */
  getAddThirdPartyVpnA11yString_: function(provider) {
    return this.i18n('internetAddThirdPartyVPN', provider.providerName || '');
  },

  /**
   * @param {!mojom.GlobalPolicy} globalPolicy
   * @return {boolean}
   * @private
   */
  allowAddConnection_: function(globalPolicy) {
    return globalPolicy && !globalPolicy.allowOnlyPolicyNetworksToConnect;
  },

  /**
   * @param {!OncMojo.DeviceStateProperties|undefined} deviceState
   * @param {!mojom.GlobalPolicy} globalPolicy
   * @return {boolean}
   * @private
   */
  showAddButton_: function(deviceState, globalPolicy) {
    if (!deviceState || deviceState.type != mojom.NetworkType.kWiFi) {
      return false;
    }
    if (!this.deviceIsEnabled_(deviceState)) {
      return false;
    }
    return this.allowAddConnection_(globalPolicy);
  },

  /** @private */
  onAddButtonTap_: function() {
    assert(this.deviceState);
    const type = this.deviceState.type;
    assert(type != mojom.NetworkType.kCellular);
    this.fire('show-config', {type: OncMojo.getNetworkTypeString(type)});
  },

  /**
   * @param {!{model: !{item: !mojom.VpnProvider}}} event
   * @private
   */
  onAddThirdPartyVpnTap_: function(event) {
    const provider = event.model.item;
    this.browserProxy_.addThirdPartyVpn(provider.appId);
  },

  /**
   * @param {!OncMojo.DeviceStateProperties|undefined} deviceState
   * @return {boolean}
   * @private
   */
  knownNetworksIsVisible_: function(deviceState) {
    return !!deviceState && deviceState.type == mojom.NetworkType.kWiFi;
  },

  /**
   * Event triggered when the known networks button is clicked.
   * @private
   */
  onKnownNetworksTap_: function() {
    assert(this.deviceState.type == mojom.NetworkType.kWiFi);
    this.fire('show-known-networks', this.deviceState.type);
  },

  /**
   * Event triggered when the enable button is toggled.
   * @param {!Event} event
   * @private
   */
  onDeviceEnabledChange_: function(event) {
    assert(this.deviceState);
    this.fire('device-enabled-toggled', {
      enabled: !this.deviceIsEnabled_(this.deviceState),
      type: this.deviceState.type
    });
  },

  /**
   * @param {!Object<!Array<!OncMojo.NetworkStateProperties>>} thirdPartyVpns
   * @param {!mojom.VpnProvider} provider
   * @return {!Array<!OncMojo.NetworkStateProperties>}
   * @private
   */
  getThirdPartyVpnNetworks_: function(thirdPartyVpns, provider) {
    return thirdPartyVpns[provider.providerId] || [];
  },

  /**
   * @param {!Object<!Array<!OncMojo.NetworkStateProperties>>} thirdPartyVpns
   * @param {!mojom.VpnProvider} provider
   * @return {boolean}
   * @private
   */
  haveThirdPartyVpnNetwork_: function(thirdPartyVpns, provider) {
    const list = this.getThirdPartyVpnNetworks_(thirdPartyVpns, provider);
    return !!list.length;
  },

  /**
   * Event triggered when a network list item is selected.
   * @param {!{target: HTMLElement, detail: !OncMojo.NetworkStateProperties}} e
   * @private
   */
  onNetworkSelected_: function(e) {
    assert(this.globalPolicy);
    assert(this.defaultNetwork !== undefined);
    const networkState = e.detail;
    e.target.blur();
    if (this.canAttemptConnection_(networkState)) {
      this.fire('network-connect', {networkState: networkState});
      return;
    }
    this.fire('show-detail', networkState);
  },

  /**
   * @param {!OncMojo.NetworkStateProperties} state The network state.
   * @return {boolean}
   * @private
   */
  isBlockedByPolicy_: function(state) {
    if (state.type != mojom.NetworkType.kWiFi ||
        this.isPolicySource(state.source) || !this.globalPolicy) {
      return false;
    }
    return !!this.globalPolicy.allowOnlyPolicyNetworksToConnect ||
        (!!this.globalPolicy.allowOnlyPolicyNetworksToConnectIfAvailable &&
         !!this.deviceState && !!this.deviceState.managedNetworkAvailable) ||
        (!!this.globalPolicy.blockedHexSsids &&
         this.globalPolicy.blockedHexSsids.includes(
             state.typeState.wifi.hexSsid));
  },

  /**
   * Determines whether or not it is possible to attempt a connection to the
   * provided network (e.g., whether it's possible to connect or configure the
   * network for connection).
   * @param {!OncMojo.NetworkStateProperties} state The network state.
   * @private
   */
  canAttemptConnection_: function(state) {
    if (state.connectionState != mojom.ConnectionStateType.kNotConnected) {
      return false;
    }
    if (this.isBlockedByPolicy_(state)) {
      return false;
    }
    if (state.type == mojom.NetworkType.kVPN &&
        (!this.defaultNetwork ||
         !OncMojo.connectionStateIsConnected(
             this.defaultNetwork.connectionState))) {
      return false;
    }
    // Cellular networks do not have a configuration flow, so it's not possible
    // to attempt a connection if the network is not conncetable.
    if (state.type == mojom.NetworkType.kCellular && !state.connectable) {
      return false;
    }
    return true;
  },

  /**
   * @param {!OncMojo.DeviceStateProperties|undefined} deviceState
   * @param {!OncMojo.DeviceStateProperties|undefined} tetherDeviceState
   * @return {boolean}
   * @private
   */
  tetherToggleIsVisible_: function(deviceState, tetherDeviceState) {
    return !!deviceState && deviceState.type == mojom.NetworkType.kCellular &&
        !!tetherDeviceState;
  },

  /**
   * @param {!OncMojo.DeviceStateProperties|undefined} deviceState
   * @param {!OncMojo.DeviceStateProperties|undefined} tetherDeviceState
   * @return {boolean}
   * @private
   */
  tetherToggleIsEnabled_: function(deviceState, tetherDeviceState) {
    return this.tetherToggleIsVisible_(deviceState, tetherDeviceState) &&
        this.enableToggleIsEnabled_(tetherDeviceState) &&
        tetherDeviceState.deviceState !=
        chromeos.networkConfig.mojom.DeviceStateType.kUninitialized;
  },

  /**
   * @param {!Event} event
   * @private
   */
  onTetherEnabledChange_: function(event) {
    this.fire('device-enabled-toggled', {
      enabled: !this.deviceIsEnabled_(this.tetherDeviceState),
      type: mojom.NetworkType.kTether,
    });
    event.stopPropagation();
  },

  /**
   * @param {string} typeString
   * @param {OncMojo.DeviceStateProperties} device
   * @return {boolean}
   * @private
   */
  matchesType_: function(typeString, device) {
    return device &&
        device.type == OncMojo.getNetworkTypeFromString(typeString);
  },

  /**
   * @param {!Array<!OncMojo.NetworkStateProperties>} networkStateList
   * @return {boolean}
   * @private
   */
  shouldShowNetworkList_: function(networkStateList) {
    return networkStateList.length > 0;
  },

  /**
   * @param {!OncMojo.DeviceStateProperties|undefined} deviceState
   * @param {!OncMojo.DeviceStateProperties|undefined} tetherDeviceState
   * @return {string}
   * @private
   */
  getNoNetworksInnerHtml_: function(deviceState, tetherDeviceState) {
    const type = deviceState.type;
    if (type == mojom.NetworkType.kTether ||
        (type == mojom.NetworkType.kCellular && this.tetherDeviceState)) {
      return this.i18nAdvanced('internetNoNetworksMobileData');
    }

    if (type == mojom.NetworkType.kVPN) {
      return this.i18n('internetNoNetworks');
    }

    // If a scan has not yet completed since the device was last enabled, it may
    // be the case that scan results are still in the process of arriving, so
    // display a message stating that scanning is in progress. If a scan has
    // already completed and there are still no networks present, this implies
    // that there has been sufficient time to find a network, so display a
    // messages stating that there are no networks. See https://crbug.com/974169
    // for more details.
    return this.hasCompletedScanSinceLastEnabled_ ?
        this.i18n('internetNoNetworks') :
        this.i18n('networkScanningLabel');
  },

  /**
   * @param {!Array<string>} notificationsDisabledDeviceNames
   * @return {boolean}
   * @private
   */
  showGmsCoreNotificationsSection_: function(notificationsDisabledDeviceNames) {
    return notificationsDisabledDeviceNames.length > 0;
  },

  /**
   * @param {!Array<string>} notificationsDisabledDeviceNames
   * @return {string}
   * @private
   */
  getGmsCoreNotificationsDevicesString_: function(
      notificationsDisabledDeviceNames) {
    if (notificationsDisabledDeviceNames.length == 1) {
      return this.i18n(
          'gmscoreNotificationsOneDeviceSubtitle',
          notificationsDisabledDeviceNames[0]);
    }

    if (notificationsDisabledDeviceNames.length == 2) {
      return this.i18n(
          'gmscoreNotificationsTwoDevicesSubtitle',
          notificationsDisabledDeviceNames[0],
          notificationsDisabledDeviceNames[1]);
    }

    return this.i18n('gmscoreNotificationsManyDevicesSubtitle');
  },
});
})();
