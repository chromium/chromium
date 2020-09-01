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
    DeepLinkingBehavior,
    settings.RouteObserverBehavior,
    settings.RouteOriginBehavior,
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
      value() {
        return [];
      },
    },

    /**
     * Dictionary of lists of network states for third party VPNs.
     * @private {!Object<!Array<!OncMojo.NetworkStateProperties>>}
     */
    thirdPartyVpns_: {
      type: Object,
      value() {
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
      value() {
        return [];
      },
    },

    /**
     * Whether to show technology badge on mobile network icons.
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

    /** @private */
    hasCompletedScanSinceLastEnabled_: {
      type: Boolean,
      value: false,
    },

    /**
     * False if VPN is disabled by policy.
     * @private {boolean}
     */
    vpnIsEnabled_: {
      type: Boolean,
      value: false,
    },

    /**
     * Contains the settingId of any deep link that wasn't able to be shown,
     * null otherwise.
     * @private {?chromeos.settings.mojom.Setting}
     */
    pendingSettingId_: {
      type: Number,
      value: null,
    },

    /**
     * Used by DeepLinkingBehavior to focus this page's deep links.
     * @type {!Set<!chromeos.settings.mojom.Setting>}
     */
    supportedSettingIds: {
      type: Object,
      value: () => new Set([
        chromeos.settings.mojom.Setting.kWifiOnOff,
        chromeos.settings.mojom.Setting.kWifiAddNetwork,
        chromeos.settings.mojom.Setting.kMobileOnOff,
        chromeos.settings.mojom.Setting.kInstantTetheringOnOff,
        chromeos.settings.mojom.Setting.kCellularAddNetwork,
      ]),
    },
  },

  /** settings.RouteOriginBehavior override */
  route_: settings.routes.INTERNET_NETWORKS,

  observers: ['deviceStateChanged_(deviceState)'],

  /** @private {number|null} */
  scanIntervalId_: null,

  /** @private  {settings.InternetPageBrowserProxy} */
  browserProxy_: null,

  /** @private {?chromeos.networkConfig.mojom.CrosNetworkConfigRemote} */
  networkConfig_: null,

  /** @override */
  created() {
    this.browserProxy_ = settings.InternetPageBrowserProxyImpl.getInstance();
    this.networkConfig_ = network_config.MojoInterfaceProviderImpl.getInstance()
                              .getMojoServiceRemote();
  },

  /** @override */
  ready() {
    this.browserProxy_.setGmsCoreNotificationsDisabledDeviceNamesCallback(
        this.onNotificationsDisabledDeviceNamesReceived_.bind(this));
    this.browserProxy_.requestGmsCoreNotificationsDisabledDeviceNames();

    this.addFocusConfig_(
        settings.routes.KNOWN_NETWORKS, '#knownNetworksSubpageButton');
  },

  /** override */
  detached() {
    this.stopScanning_();
  },

  /**
   * Overridden from DeepLinkingBehavior.
   * @param {!chromeos.settings.mojom.Setting} settingId
   * @return {boolean}
   */
  beforeDeepLinkAttempt(settingId) {
    if (settingId !== chromeos.settings.mojom.Setting.kInstantTetheringOnOff) {
      // Continue with deep linking attempt.
      return true;
    }

    // Wait for element to load.
    Polymer.RenderStatus.afterNextRender(this, () => {
      // If both Cellular and Instant Tethering are enabled, we show a special
      // toggle for Instant Tethering. If it exists, deep link to it.
      const tetherEnabled = this.$$('#tetherEnabledButton');
      if (tetherEnabled) {
        this.showDeepLinkElement(tetherEnabled);
        return;
      }
      // Otherwise, the device does not support Cellular and Instant Tethering
      // on/off is controlled by the top-level "Mobile data" toggle instead.
      const deviceEnabled = this.$$('#deviceEnabledButton');
      if (deviceEnabled) {
        this.showDeepLinkElement(deviceEnabled);
        return;
      }
      console.warn(`Element with deep link id ${settingId} not focusable.`);
    });
    // Stop deep link attempt since we completed it manually.
    return false;
  },

  /**
   * settings.RouteObserverBehavior
   * @param {!settings.Route} newRoute
   * @param {!settings.Route} oldRoute
   * @protected
   */
  currentRouteChanged(newRoute, oldRoute) {
    if (newRoute != settings.routes.INTERNET_NETWORKS) {
      this.stopScanning_();
      return;
    }
    this.init();
    settings.RouteOriginBehaviorImpl.currentRouteChanged.call(
        this, newRoute, oldRoute);

    this.attemptDeepLink().then(result => {
      if (!result.deepLinkShown && result.pendingSettingId) {
        // Store any deep link settingId that wasn't shown so we can try again
        // in getNetworkStateList_.
        this.pendingSettingId_ = result.pendingSettingId;
      }
    });
  },

  init() {
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
  onActiveNetworksChanged(networks) {
    this.getNetworkStateList_();
  },

  /** NetworkListenerBehavior override */
  onNetworkStateListChanged() {
    this.getNetworkStateList_();
  },

  /** NetworkListenerBehavior override */
  onVpnProvidersChanged() {
    if (this.deviceState.type != mojom.NetworkType.kVPN) {
      return;
    }
    this.getNetworkStateList_();
  },

  /** @private */
  deviceStateChanged_() {
    if (this.deviceState !== undefined) {
      // Set |vpnIsEnabled_| to be used for VPN special cases.
      if (this.deviceState.type === mojom.NetworkType.kVPN) {
        this.vpnIsEnabled_ = this.deviceState.deviceState ===
            chromeos.networkConfig.mojom.DeviceStateType.kEnabled;
      }

      // A scan has completed if the spinner was active (i.e., scanning was
      // active) and the device is no longer scanning.
      this.hasCompletedScanSinceLastEnabled_ = this.showSpinner &&
          !this.deviceState.scanning &&
          this.deviceState.deviceState == mojom.DeviceStateType.kEnabled;
      this.showSpinner = !!this.deviceState.scanning;
    }

    // Scans should only be triggered by the "networks" subpage.
    if (settings.Router.getInstance().getCurrentRoute() !=
        settings.routes.INTERNET_NETWORKS) {
      this.stopScanning_();
      return;
    }

    this.getNetworkStateList_();
    this.updateScanning_();
  },

  /** @private */
  updateScanning_() {
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
  shouldStartScan_() {
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
  startScanning_() {
    if (this.scanIntervalId_ != null) {
      return;
    }
    const INTERVAL_MS = 10 * 1000;
    let type = this.deviceState.type;
    if (type == mojom.NetworkType.kCellular && this.tetherDeviceState) {
      // Only request tether scan. Cellular scan is disruptive and should
      // only be triggered by explicit user action.
      type = mojom.NetworkType.kTether;
    }
    this.networkConfig_.requestNetworkScan(type);
    this.scanIntervalId_ = window.setInterval(() => {
      this.networkConfig_.requestNetworkScan(type);
    }, INTERVAL_MS);
  },

  /** @private */
  stopScanning_() {
    if (this.scanIntervalId_ == null) {
      return;
    }
    window.clearInterval(this.scanIntervalId_);
    this.scanIntervalId_ = null;
  },

  /** @private */
  getNetworkStateList_() {
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

      // Check if we have yet to focus a deep-linked element.
      if (!this.pendingSettingId_) {
        return;
      }

      this.showDeepLink(this.pendingSettingId_).then(result => {
        if (result.deepLinkShown) {
          this.pendingSettingId_ = null;
        }
      });
    });
  },

  /**
   * @param {!Array<!OncMojo.NetworkStateProperties>} networkStates
   * @private
   */
  onGetNetworks_(networkStates) {
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
  onNotificationsDisabledDeviceNamesReceived_(
      notificationsDisabledDeviceNames) {
    this.notificationsDisabledDeviceNames_ = notificationsDisabledDeviceNames;
  },

  /**
   * @param {!OncMojo.DeviceStateProperties|undefined} deviceState
   * @return {boolean} True if the device is enabled or if it is a VPN.
   *     Note: This function will always return true for VPN because VPNs can be
   *     disabled by policy only for built-in VPNs (OpenVPN & L2TP). So even
   *     when VPNs are disabled by policy; the VPN network summary item should
   *     still be visible and actionable to show details for other VPN
   *     providers.
   * @private
   */
  deviceIsEnabled_(deviceState) {
    return !!deviceState &&
        (deviceState.type == mojom.NetworkType.kVPN ||
         deviceState.deviceState == mojom.DeviceStateType.kEnabled);
  },

  /**
   * @param {!OncMojo.DeviceStateProperties|undefined} deviceState
   * @param {string} onstr
   * @param {string} offstr
   * @return {string}
   * @private
   */
  getOffOnString_(deviceState, onstr, offstr) {
    return this.deviceIsEnabled_(deviceState) ? onstr : offstr;
  },

  /**
   * @param {!OncMojo.DeviceStateProperties|undefined} deviceState
   * @return {boolean}
   * @private
   */
  enableToggleIsVisible_(deviceState) {
    return !!deviceState && deviceState.type != mojom.NetworkType.kEthernet &&
        deviceState.type != mojom.NetworkType.kVPN;
  },

  /**
   * @param {!OncMojo.DeviceStateProperties|undefined} deviceState
   * @return {boolean}
   * @private
   */
  enableToggleIsEnabled_(deviceState) {
    return !!deviceState &&
        deviceState.deviceState !=
        chromeos.networkConfig.mojom.DeviceStateType.kProhibited &&
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
   * @param {!mojom.VpnProvider} provider
   * @return {string}
   * @private
   */
  getAddThirdPartyVpnA11yString_(provider) {
    return this.i18n('internetAddThirdPartyVPN', provider.providerName || '');
  },

  /**
   * @param {!OncMojo.DeviceStateProperties|undefined} deviceState
   * @param {!mojom.GlobalPolicy} globalPolicy
   * @return {boolean}
   * @private
   */
  allowAddConnection_(deviceState, globalPolicy) {
    if (!this.deviceIsEnabled_(deviceState)) {
      return false;
    }
    return globalPolicy && !globalPolicy.allowOnlyPolicyNetworksToConnect;
  },

  /**
   * @param {!OncMojo.DeviceStateProperties|undefined} deviceState
   * @param {!mojom.GlobalPolicy} globalPolicy
   * @return {boolean}
   * @private
   */
  showAddWifiButton_(deviceState, globalPolicy) {
    if (!deviceState || deviceState.type != mojom.NetworkType.kWiFi) {
      return false;
    }
    return this.allowAddConnection_(deviceState, globalPolicy);
  },

  /**
   * @param {!OncMojo.DeviceStateProperties|undefined} deviceState
   * @param {!mojom.GlobalPolicy} globalPolicy
   * @return {boolean}
   * @private
   */
  showAddCellularButton_(deviceState, globalPolicy) {
    if (!this.isUpdatedCellularUiEnabled_) {
      return false;
    }

    if (!deviceState || deviceState.type != mojom.NetworkType.kCellular) {
      return false;
    }
    return this.allowAddConnection_(deviceState, globalPolicy);
  },

  /** @private */
  onAddWifiButtonTap_() {
    assert(this.deviceState, 'Device state is falsey - Wifi expected.');
    const type = this.deviceState.type;
    assert(type === mojom.NetworkType.kWiFi, 'Wifi type expected.');
    this.fire('show-config', {type: OncMojo.getNetworkTypeString(type)});
  },

  /** @private */
  onAddVpnButtonTap_() {
    assert(this.deviceState, 'Device state is falsey - VPN expected.');
    const type = this.deviceState.type;
    assert(type === mojom.NetworkType.kVPN, 'VPN type expected.');
    this.fire('show-config', {type: OncMojo.getNetworkTypeString(type)});
  },

  /** @private */
  onAddCellularButtonTap_() {
    assert(this.deviceState, 'Device state is falsey - Cellular expected.');
    const type = this.deviceState.type;
    assert(type === mojom.NetworkType.kCellular, 'Cellular type expected.');
    this.fire('show-cellular-setup');
  },

  /**
   * @param {!{model: !{item: !mojom.VpnProvider}}} event
   * @private
   */
  onAddThirdPartyVpnTap_(event) {
    const provider = event.model.item;
    this.browserProxy_.addThirdPartyVpn(provider.appId);
    settings.recordSettingChange();
  },

  /**
   * @param {!OncMojo.DeviceStateProperties|undefined} deviceState
   * @return {boolean}
   * @private
   */
  knownNetworksIsVisible_(deviceState) {
    return !!deviceState && deviceState.type == mojom.NetworkType.kWiFi;
  },

  /**
   * Event triggered when the known networks button is clicked.
   * @private
   */
  onKnownNetworksTap_() {
    assert(this.deviceState.type == mojom.NetworkType.kWiFi);
    this.fire('show-known-networks', this.deviceState.type);
  },

  /**
   * Event triggered when the enable button is toggled.
   * @param {!Event} event
   * @private
   */
  onDeviceEnabledChange_(event) {
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
  getThirdPartyVpnNetworks_(thirdPartyVpns, provider) {
    return thirdPartyVpns[provider.providerId] || [];
  },

  /**
   * @param {!Object<!Array<!OncMojo.NetworkStateProperties>>} thirdPartyVpns
   * @param {!mojom.VpnProvider} provider
   * @return {boolean}
   * @private
   */
  haveThirdPartyVpnNetwork_(thirdPartyVpns, provider) {
    const list = this.getThirdPartyVpnNetworks_(thirdPartyVpns, provider);
    return !!list.length;
  },

  /**
   * Event triggered when a network list item is selected.
   * @param {!{target: HTMLElement, detail: !OncMojo.NetworkStateProperties}} e
   * @private
   */
  onNetworkSelected_(e) {
    assert(this.globalPolicy);
    assert(this.defaultNetwork !== undefined);
    const networkState = e.detail;
    e.target.blur();
    if (this.canAttemptConnection_(networkState)) {
      this.fire('network-connect', {networkState: networkState});
      settings.recordSettingChange();
      return;
    }
    this.fire('show-detail', networkState);
  },

  /**
   * @param {!OncMojo.NetworkStateProperties} state The network state.
   * @return {boolean}
   * @private
   */
  isBlockedByPolicy_(state) {
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
  canAttemptConnection_(state) {
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
  tetherToggleIsVisible_(deviceState, tetherDeviceState) {
    // Do not show instant tether toggle if Updated Cellular UI is enabled.
    // This toggle will be removed from the mobile data subpage.
    if (this.isUpdatedCellularUiEnabled_) {
      return false;
    }

    return !!deviceState && deviceState.type == mojom.NetworkType.kCellular &&
        !!tetherDeviceState;
  },

  /**
   * @param {!OncMojo.DeviceStateProperties|undefined} deviceState
   * @param {!OncMojo.DeviceStateProperties|undefined} tetherDeviceState
   * @return {boolean}
   * @private
   */
  tetherToggleIsEnabled_(deviceState, tetherDeviceState) {
    return this.tetherToggleIsVisible_(deviceState, tetherDeviceState) &&
        this.enableToggleIsEnabled_(tetherDeviceState) &&
        tetherDeviceState.deviceState !=
        chromeos.networkConfig.mojom.DeviceStateType.kUninitialized;
  },

  /**
   * @param {!Event} event
   * @private
   */
  onTetherEnabledChange_(event) {
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
  matchesType_(typeString, device) {
    return !!device &&
        device.type == OncMojo.getNetworkTypeFromString(typeString);
  },

  /**
   * @param {!Array<!OncMojo.NetworkStateProperties>} networkStateList
   * @return {boolean}
   * @private
   */
  shouldShowNetworkList_(networkStateList) {
    if (!!this.deviceState &&
        this.deviceState.type === mojom.NetworkType.kVPN) {
      return this.shouldShowVpnList_(networkStateList);
    }
    return networkStateList.length > 0;
  },

  /**
   * @param {!Array<!OncMojo.NetworkStateProperties>} networkStateList
   * @return {boolean} True if native VPN is not disabled by policy and there
   *     are more than one VPN network configured.
   * @private
   */
  shouldShowVpnList_(networkStateList) {
    return this.vpnIsEnabled_ && networkStateList.length > 0;
  },

  /**
   * @param {!OncMojo.DeviceStateProperties|undefined} deviceState
   * @param {!OncMojo.DeviceStateProperties|undefined} tetherDeviceState
   * @return {string}
   * @private
   */
  getNoNetworksInnerHtml_(deviceState, tetherDeviceState) {
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
  showGmsCoreNotificationsSection_(notificationsDisabledDeviceNames) {
    return notificationsDisabledDeviceNames.length > 0;
  },

  /**
   * @param {!Array<string>} notificationsDisabledDeviceNames
   * @return {string}
   * @private
   */
  getGmsCoreNotificationsDevicesString_(notificationsDisabledDeviceNames) {
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
