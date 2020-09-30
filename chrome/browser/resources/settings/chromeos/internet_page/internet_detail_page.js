// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'settings-internet-detail' is the settings subpage containing details
 * for a network.
 */

Polymer({
  is: 'settings-internet-detail-page',

  behaviors: [
    NetworkListenerBehavior,
    CrPolicyNetworkBehaviorMojo,
    DeepLinkingBehavior,
    settings.RouteObserverBehavior,
    I18nBehavior,
    WebUIListenerBehavior,
  ],

  properties: {
    /** The network GUID to display details for. */
    guid: String,

    /** Preferences state. */
    prefs: {
      type: Object,
      notify: true,
    },

    /** @private Indicates if wi-fi sync is enabled for the active user.  */
    isWifiSyncEnabled_: Boolean,

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
      value() {
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
      value() {
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
      value() {
        return {
          key: 'fakeAutoConnectPref',
          type: chrome.settingsPrivate.PrefType.BOOLEAN,
          value: false,
        };
      },
    },

    /**
     * The always-on VPN state as a fake preference object.
     * @private {!chrome.settingsPrivate.PrefObject|undefined}
     */
    alwaysOnVpn_: {
      type: Object,
      observer: 'alwaysOnVpnChanged_',
      value() {
        return {
          key: 'fakeAlwaysOnPref',
          type: chrome.settingsPrivate.PrefType.BOOLEAN,
          value: false,
        };
      },
    },

    /**
     * This gets initialized to managedProperties_.metered.activeValue.
     * When this is changed from the UI, a change event will update the
     * property and setMojoNetworkProperties will be called.
     * @private
     */
    meteredOverride_: {
      type: Boolean,
      value: false,
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
      value() {
        return loadTimeData.valueExists('showTechnologyBadge') &&
            loadTimeData.getBoolean('showTechnologyBadge');
      }
    },

    /**
     * Whether to show the Metered toggle.
     * @private
     */
    showMeteredToggle_: {
      type: Boolean,
      value() {
        return loadTimeData.valueExists('showMeteredToggle') &&
            loadTimeData.getBoolean('showMeteredToggle');
      }
    },

    /** @private */
    advancedExpanded_: Boolean,

    /** @private */
    networkExpanded_: Boolean,

    /** @private */
    proxyExpanded_: Boolean,

    /**
     * Used by DeepLinkingBehavior to focus this page's deep links.
     * @type {!Set<!chromeos.settings.mojom.Setting>}
     */
    supportedSettingIds: {
      type: Object,
      value: () => new Set([
        chromeos.settings.mojom.Setting.kConfigureEthernet,
        chromeos.settings.mojom.Setting.kEthernetAutoConfigureIp,
        chromeos.settings.mojom.Setting.kEthernetDns,
        chromeos.settings.mojom.Setting.kEthernetProxy,
        chromeos.settings.mojom.Setting.kDisconnectWifiNetwork,
        chromeos.settings.mojom.Setting.kPreferWifiNetwork,
        chromeos.settings.mojom.Setting.kForgetWifiNetwork,
        chromeos.settings.mojom.Setting.kWifiAutoConfigureIp,
        chromeos.settings.mojom.Setting.kWifiDns,
        chromeos.settings.mojom.Setting.kWifiProxy,
        chromeos.settings.mojom.Setting.kWifiAutoConnectToNetwork,
        chromeos.settings.mojom.Setting.kCellularSimLock,
        chromeos.settings.mojom.Setting.kCellularRoaming,
        chromeos.settings.mojom.Setting.kCellularApn,
        chromeos.settings.mojom.Setting.kDisconnectCellularNetwork,
        chromeos.settings.mojom.Setting.kCellularAutoConfigureIp,
        chromeos.settings.mojom.Setting.kCellularDns,
        chromeos.settings.mojom.Setting.kCellularProxy,
        chromeos.settings.mojom.Setting.kCellularAutoConnectToNetwork,
        chromeos.settings.mojom.Setting.kDisconnectTetherNetwork,
        chromeos.settings.mojom.Setting.kWifiMetered,
        chromeos.settings.mojom.Setting.kCellularMetered,
      ]),
    },
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

  /** @private {?settings.OsSyncBrowserProxy} */
  osSyncBrowserProxy_: null,

  /** @private {?settings.SyncBrowserProxy} */
  syncBrowserProxy_: null,

  /** @private {?chromeos.networkConfig.mojom.CrosNetworkConfigRemote} */
  networkConfig_: null,

  /**
   * Prevents re-saving incoming changes.
   * @private {boolean}
   */
  applyingChanges_: false,

  /** @override */
  attached() {
    if (loadTimeData.getBoolean('splitSettingsSyncEnabled')) {
      this.addWebUIListener(
          'os-sync-prefs-changed', this.handleOsSyncPrefsChanged_.bind(this));
      this.osSyncBrowserProxy_.sendOsSyncPrefsChanged();
    } else {
      this.addWebUIListener(
          'sync-prefs-changed', this.handleSyncPrefsChanged_.bind(this));
      this.syncBrowserProxy_.sendSyncPrefsChanged();
    }
  },

  /** @override */
  created() {
    this.browserProxy_ = settings.InternetPageBrowserProxyImpl.getInstance();
    this.networkConfig_ = network_config.MojoInterfaceProviderImpl.getInstance()
                              .getMojoServiceRemote();

    if (loadTimeData.getBoolean('splitSettingsSyncEnabled')) {
      this.osSyncBrowserProxy_ = settings.OsSyncBrowserProxyImpl.getInstance();
    } else {
      this.syncBrowserProxy_ = settings.SyncBrowserProxyImpl.getInstance();
    }
  },

  /**
   * Helper function for manually showing deep links on this page.
   * @param {!chromeos.settings.mojom.Setting} settingId
   * @param {!function():?Element} elementCallback
   * @private
   */
  afterRenderShowDeepLink(settingId, elementCallback) {
    // Wait for element to load.
    Polymer.RenderStatus.afterNextRender(this, () => {
      const deepLinkElement = elementCallback();
      if (!deepLinkElement || deepLinkElement.hidden) {
        console.warn(`Element with deep link id ${settingId} not focusable.`);
        return;
      }
      this.showDeepLinkElement(deepLinkElement);
    });
  },

  /**
   * Overridden from DeepLinkingBehavior.
   * @param {!chromeos.settings.mojom.Setting} settingId
   * @return {boolean}
   */
  beforeDeepLinkAttempt(settingId) {
    // Manually show the deep links for settings in shared elements.
    if (settingId === chromeos.settings.mojom.Setting.kCellularApn) {
      this.networkExpanded_ = true;
      this.afterRenderShowDeepLink(
          settingId, () => this.$$('network-apnlist').getApnSelect());
      // Stop deep link attempt since we completed it manually.
      return false;
    }

    if (settingId ===
            chromeos.settings.mojom.Setting.kEthernetAutoConfigureIp ||
        settingId === chromeos.settings.mojom.Setting.kWifiAutoConfigureIp ||
        settingId ===
            chromeos.settings.mojom.Setting.kCellularAutoConfigureIp) {
      this.networkExpanded_ = true;
      this.afterRenderShowDeepLink(
          settingId,
          () => this.$$('network-ip-config').getAutoConfigIpToggle());
      return false;
    }

    if (settingId === chromeos.settings.mojom.Setting.kEthernetDns ||
        settingId === chromeos.settings.mojom.Setting.kWifiDns ||
        settingId === chromeos.settings.mojom.Setting.kCellularDns) {
      this.networkExpanded_ = true;
      this.afterRenderShowDeepLink(
          settingId,
          () => this.$$('network-nameservers').getNameserverRadioButtons());
      return false;
    }

    if (settingId === chromeos.settings.mojom.Setting.kEthernetProxy ||
        settingId === chromeos.settings.mojom.Setting.kWifiProxy ||
        settingId === chromeos.settings.mojom.Setting.kCellularProxy) {
      this.proxyExpanded_ = true;
      this.afterRenderShowDeepLink(
          settingId,
          () => this.$$('network-proxy-section').getAllowSharedToggle());
      return false;
    }

    if (settingId === chromeos.settings.mojom.Setting.kWifiMetered ||
        settingId === chromeos.settings.mojom.Setting.kCellularMetered) {
      this.advancedExpanded_ = true;
      // Continue with automatically showing these deep links.
      return true;
    }

    if (settingId === chromeos.settings.mojom.Setting.kForgetWifiNetwork) {
      this.afterRenderShowDeepLink(settingId, () => {
        const forgetButton = this.$$('#forgetButton');
        if (forgetButton && !forgetButton.hidden) {
          return forgetButton;
        }
        // If forget button is hidden, show disconnect button instead.
        return this.$$('#connectDisconnect');
      });
      return false;
    }

    if (settingId === chromeos.settings.mojom.Setting.kCellularSimLock) {
      // In this rare case, toggle not focusable until after a second wait.
      // This is slightly preferable to requestAnimationFrame used within
      // network-siminfo to focus elements since it can be reproduced in
      // testing.
      Polymer.RenderStatus.afterNextRender(this, () => {
        this.afterRenderShowDeepLink(
            settingId, () => this.$$('network-siminfo').getSimLockToggle());
      });
      return false;
    }

    // Otherwise, should continue with deep link attempt.
    return true;
  },

  /**
   * settings.RouteObserverBehavior
   * @param {!settings.Route} route
   * @param {!settings.Route} oldRoute
   * @protected
   */
  currentRouteChanged(route, oldRoute) {
    if (route != settings.routes.NETWORK_DETAIL) {
      return;
    }

    const queryParams = settings.Router.getInstance().getQueryParameters();
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

    this.attemptDeepLink();
  },

  /**
   * Handler for when the sync preferences are updated.
   * @private
   */
  handleSyncPrefsChanged_(syncPrefs) {
    this.isWifiSyncEnabled_ = !!syncPrefs && syncPrefs.wifiConfigurationsSynced;
  },

  /**
   * Handler for when os sync preferences are updated.
   * @private
   */
  handleOsSyncPrefsChanged_(osSyncFeatureEnabled, osSyncPrefs) {
    this.isWifiSyncEnabled_ = osSyncFeatureEnabled && !!osSyncPrefs &&
        osSyncPrefs.osWifiConfigurationsSynced;
  },

  /**
   * @param {string} guid
   * @param {string} type
   * @param {string} name
   */
  init(guid, type, name) {
    this.guid = guid;
    // Set default properties until they are loaded.
    this.propertiesReceived_ = false;
    this.deviceState_ = null;
    this.managedProperties_ = OncMojo.getDefaultManagedProperties(
        OncMojo.getNetworkTypeFromString(type), this.guid, name);
    this.didSetFocus_ = false;
    this.getNetworkDetails_();
  },

  close() {
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

      settings.Router.getInstance().navigateToPreviousRoute();
    });
  },

  /**
   * CrosNetworkConfigObserver impl
   * @param {!Array<OncMojo.NetworkStateProperties>} networks
   */
  onActiveNetworksChanged(networks) {
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
  onNetworkStateChanged(network) {
    if (!this.guid || !this.managedProperties_) {
      return;
    }
    if (network.guid == this.guid) {
      this.getNetworkDetails_();
    }
  },

  /** CrosNetworkConfigObserver impl */
  onNetworkStateListChanged() {
    if (!this.guid || !this.managedProperties_) {
      return;
    }
    this.checkNetworkExists_();
  },

  /** CrosNetworkConfigObserver impl */
  onDeviceStateListChanged() {
    if (!this.guid || !this.managedProperties_) {
      return;
    }
    this.getDeviceState_();
  },

  /** @private */
  managedPropertiesChanged_() {
    if (!this.managedProperties_) {
      return;
    }
    this.updateAutoConnectPref_();

    const metered = this.managedProperties_.metered;
    if (metered && metered.activeValue != this.meteredOverride_) {
      this.meteredOverride_ = metered.activeValue;
    }

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

    if (!this.didSetFocus_ &&
        !settings.Router.getInstance().getQueryParameters().has('search') &&
        !this.getDeepLinkSettingId()) {
      // Unless the page was navigated to via search or has a deep linked
      // setting, focus a button once the initial state is set.
      this.didSetFocus_ = true;
      const button = this.$$('#titleDiv .action-button:not([hidden])');
      if (button) {
        Polymer.RenderStatus.afterNextRender(this, () => button.focus());
      }
    }

    if (this.shouldShowConfigureWhenNetworkLoaded_ &&
        this.managedProperties_.type ==
            chromeos.networkConfig.mojom.NetworkType.kTether) {
      // Set |this.shouldShowConfigureWhenNetworkLoaded_| back to false to
      // ensure that the Tether dialog is only shown once.
      this.shouldShowConfigureWhenNetworkLoaded_ = false;
      // Async call to ensure dialog is stamped.
      setTimeout(() => this.showTetherDialog_());
    }
  },

  /**
   * Returns true if all significant DeviceState fields match. Ignores
   * |scanning| which can be noisy and is handled separately.
   * @param {!OncMojo.DeviceStateProperties} a
   * @param {!OncMojo.DeviceStateProperties} b
   * @return {boolean}
   * @private
   */
  deviceStatesMatch_(a, b) {
    return a.type === b.type && a.macAddress === b.macAddress &&
        a.simAbsent === b.simAbsent && a.deviceState === b.deviceState &&
        a.managedNetworkAvailable === b.managedNetworkAvailable &&
        OncMojo.ipAddressMatch(a.ipv4Address, b.ipv4Address) &&
        OncMojo.ipAddressMatch(a.ipv6Address, b.ipv6Address) &&
        OncMojo.simLockStatusMatch(a.simLockStatus, b.simLockStatus);
  },

  /** @private */
  getDeviceState_() {
    if (!this.managedProperties_) {
      return;
    }
    const type = this.managedProperties_.type;
    this.networkConfig_.getDeviceStateList().then(response => {
      const devices = response.result;
      const newDeviceState =
          devices.find(device => device.type == type) || null;
      let shouldGetNetworkDetails = false;
      if (!this.deviceState_ || !newDeviceState) {
        this.deviceState_ = newDeviceState;
        shouldGetNetworkDetails = !!this.deviceState_;
      } else if (!this.deviceStatesMatch_(this.deviceState_, newDeviceState)) {
        // Only request a network state update if the deviceState changed.
        shouldGetNetworkDetails =
            this.deviceState_.deviceState != newDeviceState.deviceState;
        this.deviceState_ = newDeviceState;
      } else if (this.deviceState_.scanning != newDeviceState.scanning) {
        // Update just the scanning state to avoid interrupting other parts of
        // the UI (e.g. custom IP addresses or nameservers).
        this.deviceState_.scanning = newDeviceState.scanning;
        // Cellular properties are not updated while scanning (since they
        // may be invalid), so request them on scan completion.
        if (type === chromeos.networkConfig.mojom.NetworkType.kCellular) {
          shouldGetNetworkDetails = true;
        }
      } else if (type === chromeos.networkConfig.mojom.NetworkType.kCellular) {
        // If there are no device state property changes but type is
        // cellular, then always fetch network details. This is because
        // for cellular networks, some shill device level properties are
        // represented at network level in ONC.
        shouldGetNetworkDetails = true;
      }
      if (shouldGetNetworkDetails) {
        this.getNetworkDetails_();
      }
    });
  },

  /** @private */
  autoConnectPrefChanged_() {
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
  updateAutoConnectPref_() {
    if (!this.managedProperties_) {
      return;
    }
    const autoConnect = OncMojo.getManagedAutoConnect(this.managedProperties_);
    if (!autoConnect) {
      return;
    }

    const PolicySource = chromeos.networkConfig.mojom.PolicySource;

    let enforcement;
    let controlledBy;

    if (this.globalPolicy &&
        this.globalPolicy.allowOnlyPolicyNetworksToAutoconnect) {
      enforcement = chrome.settingsPrivate.Enforcement.ENFORCED;
      controlledBy = chrome.settingsPrivate.ControlledBy.DEVICE_POLICY;
    } else {
      switch (autoConnect.policySource) {
        case PolicySource.kUserPolicyEnforced:
        case PolicySource.kDevicePolicyEnforced:
          enforcement = chrome.settingsPrivate.Enforcement.ENFORCED;
          break;
        case PolicySource.kUserPolicyRecommended:
        case PolicySource.kDevicePolicyRecommended:
          enforcement = chrome.settingsPrivate.Enforcement.RECOMMENDED;
          break;
      }

      switch (autoConnect.policySource) {
        case PolicySource.kDevicePolicyEnforced:
        case PolicySource.kDevicePolicyRecommended:
          controlledBy = chrome.settingsPrivate.ControlledBy.DEVICE_POLICY;
          break;
        case PolicySource.kUserPolicyEnforced:
        case PolicySource.kUserPolicyRecommended:
          controlledBy = chrome.settingsPrivate.ControlledBy.USER_POLICY;
          break;
      }
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

  /**
   * @param {!CustomEvent<boolean>} e
   * @private
   */
  meteredChanged_(e) {
    if (!this.propertiesReceived_) {
      return;
    }
    const config = this.getDefaultConfigProperties_();
    config.metered = {value: e.detail.value};
    this.setMojoNetworkProperties_(config);
  },

  /** @private */
  preferNetworkChanged_() {
    if (!this.propertiesReceived_) {
      return;
    }
    const config = this.getDefaultConfigProperties_();
    config.priority = {value: this.preferNetwork_ ? 1 : 0};
    this.setMojoNetworkProperties_(config);
  },

  /** @private */
  checkNetworkExists_() {
    const filter = {
      filter: chromeos.networkConfig.mojom.FilterType.kVisible,
      networkType: chromeos.networkConfig.mojom.NetworkType.kAll,
      limit: chromeos.networkConfig.mojom.NO_LIMIT,
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
            chromeos.networkConfig.mojom.ConnectionStateType.kNotConnected;
      }
    });
  },

  /** @private */
  getNetworkDetails_() {
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
   * @param {?chromeos.networkConfig.mojom.ManagedProperties} properties
   * @private
   */
  getPropertiesCallback_(properties) {
    // Details page was closed while request was in progress, ignore the result.
    if (!this.guid) {
      return;
    }

    if (!properties) {
      console.error('Details page: GUID no longer exists: ' + this.guid);
      this.close();
      return;
    }

    this.updateManagedProperties_(properties);

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
   * @param {!chromeos.networkConfig.mojom.ManagedProperties|undefined}
   *     properties
   * @private
   */
  updateManagedProperties_(properties) {
    this.applyingChanges_ = true;
    if (this.managedProperties_ &&
        this.managedProperties_.type ===
            chromeos.networkConfig.mojom.NetworkType.kCellular &&
        this.deviceState_ && this.deviceState_.scanning) {
      // Cellular properties may be invalid while scanning, so keep the existing
      // properties instead.
      properties.typeProperties.cellular =
          this.managedProperties_.typeProperties.cellular;
    }
    this.managedProperties_ = properties;
    Polymer.RenderStatus.afterNextRender(this, () => {
      this.applyingChanges_ = false;
    });
  },

  /**
   * @param {?OncMojo.NetworkStateProperties} networkState
   * @private
   */
  getStateCallback_(networkState) {
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
      case chromeos.networkConfig.mojom.NetworkType.kCellular:
        managedProperties.typeProperties.cellular.signalStrength =
            networkState.typeState.cellular.signalStrength;
        break;
      case chromeos.networkConfig.mojom.NetworkType.kTether:
        managedProperties.typeProperties.tether.signalStrength =
            networkState.typeState.tether.signalStrength;
        break;
      case chromeos.networkConfig.mojom.NetworkType.kWiFi:
        managedProperties.typeProperties.wifi.signalStrength =
            networkState.typeState.wifi.signalStrength;
        break;
    }
    this.updateManagedProperties_(managedProperties);

    this.propertiesReceived_ = true;
    this.outOfRange_ = false;
  },

  /**
   * @param {!chromeos.networkConfig.mojom.ManagedProperties} properties
   * @return {!OncMojo.NetworkStateProperties|undefined}
   */
  getNetworkState_(properties) {
    if (!properties) {
      return undefined;
    }
    return OncMojo.managedPropertiesToNetworkState(properties);
  },

  /**
   * @return {!chromeos.networkConfig.mojom.ConfigProperties}
   * @private
   */
  getDefaultConfigProperties_() {
    return OncMojo.getDefaultConfigProperties(this.managedProperties_.type);
  },

  /**
   * @param {!chromeos.networkConfig.mojom.ConfigProperties} config
   * @private
   */
  setMojoNetworkProperties_(config) {
    if (!this.propertiesReceived_ || !this.guid || this.applyingChanges_) {
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
    settings.recordSettingChange();
  },

  /**
   * @param {!chromeos.networkConfig.mojom.ManagedProperties} managedProperties
   * @param {boolean} propertiesReceived
   * @param {boolean} outOfRange
   * @param {?OncMojo.DeviceStateProperties} deviceState
   * @return {string} The text to display for the network connection state.
   * @private
   */
  getStateText_(
      managedProperties, propertiesReceived, outOfRange, deviceState) {
    if (!managedProperties || !propertiesReceived) {
      return '';
    }

    if (this.isOutOfRangeOrNotEnabled_(outOfRange, deviceState)) {
      return managedProperties.type ==
              chromeos.networkConfig.mojom.NetworkType.kTether ?
          this.i18n('tetherPhoneOutOfRange') :
          this.i18n('networkOutOfRange');
    }

    if (managedProperties.type ==
            chromeos.networkConfig.mojom.NetworkType.kCellular &&
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
   * @param {!chromeos.networkConfig.mojom.ManagedProperties} managedProperties
   * @return {string} The text to display for auto-connect toggle label.
   * @private
   */
  getAutoConnectToggleLabel_(managedProperties) {
    return this.isCellular_(managedProperties) ?
        this.i18n('networkAutoConnectCellular') :
        this.i18n('networkAutoConnect');
  },

  /**
   * @param {!chromeos.networkConfig.mojom.ManagedProperties} managedProperties
   * @return {string} The text to display with roaming details.
   * @private
   */
  getRoamingDetails_(managedProperties) {
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
   * @param {!chromeos.networkConfig.mojom.ManagedProperties|undefined}
   *     managedProperties
   * @return {boolean} True if the network is connected.
   * @private
   */
  isConnectedState_(managedProperties) {
    return !!managedProperties &&
        OncMojo.connectionStateIsConnected(managedProperties.connectionState);
  },

  /**
   * @param {!chromeos.networkConfig.mojom.ManagedProperties|undefined}
   *     managedProperties
   * @param {boolean} outOfRange
   * @param {?OncMojo.DeviceStateProperties} deviceState
   * @return {boolean} True if the network shown cannot initiate a connection.
   * @private
   */
  isConnectionErrorState_(managedProperties, outOfRange, deviceState) {
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
    return managedProperties.type ==
        chromeos.networkConfig.mojom.NetworkType.kCellular &&
        !managedProperties.connectable;
  },

  /**
   * @param {!chromeos.networkConfig.mojom.ManagedProperties} managedProperties
   * @return {boolean}
   * @private
   */
  isRemembered_(managedProperties) {
    return !!managedProperties &&
        managedProperties.source !=
        chromeos.networkConfig.mojom.OncSource.kNone;
  },

  /**
   * @param {!chromeos.networkConfig.mojom.ManagedProperties} managedProperties
   * @return {boolean}
   * @private
   */
  isRememberedOrConnected_(managedProperties) {
    return this.isRemembered_(managedProperties) ||
        this.isConnectedState_(managedProperties);
  },

  /**
   * @param {!chromeos.networkConfig.mojom.ManagedProperties} managedProperties
   * @return {boolean}
   * @private
   */
  isCellular_(managedProperties) {
    return !!managedProperties &&
        managedProperties.type ==
        chromeos.networkConfig.mojom.NetworkType.kCellular;
  },

  /**
   * @param {!chromeos.networkConfig.mojom.ManagedProperties} managedProperties
   * @return {boolean}
   * @private
   */
  isTether_(managedProperties) {
    return !!managedProperties &&
        managedProperties.type ==
        chromeos.networkConfig.mojom.NetworkType.kTether;
  },

  /**
   * @param {!chromeos.networkConfig.mojom.ManagedProperties} managedProperties
   * @param {!chromeos.networkConfig.mojom.GlobalPolicy|undefined} globalPolicy
   * @param {boolean} managedNetworkAvailable
   * @return {boolean}
   * @private
   */
  isBlockedByPolicy_(managedProperties, globalPolicy, managedNetworkAvailable) {
    if (!managedProperties || !globalPolicy ||
        managedProperties.type !=
            chromeos.networkConfig.mojom.NetworkType.kWiFi ||
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
   * @param {!chromeos.networkConfig.mojom.ManagedProperties|undefined}
   *     managedProperties
   * @param {!chromeos.networkConfig.mojom.GlobalPolicy|undefined} globalPolicy
   * @param {boolean} managedNetworkAvailable
   * @param {?OncMojo.DeviceStateProperties} deviceState
   * @return {boolean}
   * @private
   */
  showConnect_(
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
        chromeos.networkConfig.mojom.ConnectionStateType.kNotConnected) {
      return false;
    }

    if (deviceState &&
        deviceState.deviceState !=
            chromeos.networkConfig.mojom.DeviceStateType.kEnabled) {
      return false;
    }

    // Cellular is not configurable, so we always show the connect button, and
    // disable it if 'connectable' is false.
    if (managedProperties.type ==
        chromeos.networkConfig.mojom.NetworkType.kCellular) {
      return true;
    }

    // If 'connectable' is false we show the configure button.
    return managedProperties.connectable &&
        managedProperties.type !=
        chromeos.networkConfig.mojom.NetworkType.kEthernet;
  },

  /**
   * @param {!chromeos.networkConfig.mojom.ManagedProperties|undefined}
   *     managedProperties
   * @return {boolean}
   * @private
   */
  showDisconnect_(managedProperties) {
    if (!managedProperties ||
        managedProperties.type ==
            chromeos.networkConfig.mojom.NetworkType.kEthernet) {
      return false;
    }
    return managedProperties.connectionState !=
        chromeos.networkConfig.mojom.ConnectionStateType.kNotConnected;
  },

  /**
   * @param {!chromeos.networkConfig.mojom.ManagedProperties} managedProperties
   * @return {boolean}
   * @private
   */
  showForget_(managedProperties) {
    if (!managedProperties || this.isSecondaryUser_) {
      return false;
    }
    const type = managedProperties.type;
    if (type != chromeos.networkConfig.mojom.NetworkType.kWiFi &&
        type != chromeos.networkConfig.mojom.NetworkType.kVPN) {
      return false;
    }
    if (this.isArcVpn_(managedProperties)) {
      return false;
    }
    return !this.isPolicySource(managedProperties.source) &&
        this.isRemembered_(managedProperties);
  },

  /**
   * @param {!chromeos.networkConfig.mojom.ManagedProperties} managedProperties
   * @return {boolean}
   * @private
   */
  showActivate_(managedProperties) {
    if (!managedProperties || this.isSecondaryUser_) {
      return false;
    }
    if (!this.isCellular_(managedProperties)) {
      return false;
    }
    const activation =
        managedProperties.typeProperties.cellular.activationState;
    return activation ==
        chromeos.networkConfig.mojom.ActivationStateType.kNotActivated ||
        activation ==
        chromeos.networkConfig.mojom.ActivationStateType.kPartiallyActivated;
  },

  /**
   * @param {!chromeos.networkConfig.mojom.ManagedProperties} managedProperties
   * @param {!chromeos.networkConfig.mojom.GlobalPolicy} globalPolicy
   * @param {boolean} managedNetworkAvailable
   * @return {boolean}
   * @private
   */
  showConfigure_(managedProperties, globalPolicy, managedNetworkAvailable) {
    if (!managedProperties || this.isSecondaryUser_) {
      return false;
    }
    if (this.isBlockedByPolicy_(
            managedProperties, globalPolicy, managedNetworkAvailable)) {
      return false;
    }
    const type = managedProperties.type;
    if (type == chromeos.networkConfig.mojom.NetworkType.kCellular ||
        type == chromeos.networkConfig.mojom.NetworkType.kTether) {
      return false;
    }
    if (type == chromeos.networkConfig.mojom.NetworkType.kWiFi &&
        managedProperties.typeProperties.wifi.security ==
            chromeos.networkConfig.mojom.SecurityType.kNone) {
      return false;
    }
    if (type == chromeos.networkConfig.mojom.NetworkType.kWiFi &&
        (managedProperties.connectionState !=
         chromeos.networkConfig.mojom.ConnectionStateType.kNotConnected)) {
      return false;
    }
    if (this.isArcVpn_(managedProperties) &&
        !this.isConnectedState_(managedProperties)) {
      return false;
    }
    return true;
  },

  /**
   * @param {!chromeos.networkConfig.mojom.ManagedProperties} managedProperties
   * @param {!chrome.settingsPrivate.PrefObject} vpnConfigAllowed
   * @return {boolean}
   * @private
   */
  disableForget_(managedProperties, vpnConfigAllowed) {
    if (!managedProperties) {
      return true;
    }
    return managedProperties.type ==
        chromeos.networkConfig.mojom.NetworkType.kVPN &&
        vpnConfigAllowed && !vpnConfigAllowed.value;
  },

  /**
   * @param {!chromeos.networkConfig.mojom.ManagedProperties} managedProperties
   * @param {!chrome.settingsPrivate.PrefObject} vpnConfigAllowed
   * @return {boolean}
   * @private
   */
  disableConfigure_(managedProperties, vpnConfigAllowed) {
    if (!managedProperties) {
      return true;
    }
    if (managedProperties.type ==
            chromeos.networkConfig.mojom.NetworkType.kVPN &&
        vpnConfigAllowed && !vpnConfigAllowed.value) {
      return true;
    }
    return this.isPolicySource(managedProperties.source) &&
        !this.hasRecommendedFields_(managedProperties);
  },


  /**
   * @param {!chromeos.networkConfig.mojom.ManagedProperties} managedProperties
   * @return {boolean}
   */
  hasRecommendedFields_(managedProperties) {
    if (!managedProperties) {
      return false;
    }
    for (const value of Object.values(managedProperties)) {
      if (typeof value != 'object' || value === null) {
        continue;
      }
      if ('activeValue' in value) {
        if (this.isNetworkPolicyRecommended(
                /** @type {!OncMojo.ManagedProperty} */ (value))) {
          return true;
        }
      } else if (
          this.hasRecommendedFields_(
              /** @type {!chromeos.networkConfig.mojom.ManagedProperties} */ (
                  value))) {
        return true;
      }
    }
    return false;
  },

  /**
   * @param {!chromeos.networkConfig.mojom.ManagedProperties} managedProperties
   * @return {boolean}
   * @private
   */
  showViewAccount_(managedProperties) {
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
   * @param {!chromeos.networkConfig.mojom.ManagedProperties|undefined}
   *     managedProperties
   * @param {?OncMojo.NetworkStateProperties} defaultNetwork
   * @param {boolean} propertiesReceived
   * @param {boolean} outOfRange
   * @param {!chromeos.networkConfig.mojom.GlobalPolicy|undefined} globalPolicy
   * @param {boolean} managedNetworkAvailable
   * @param {?OncMojo.DeviceStateProperties} deviceState
   * @return {boolean} Whether or not to enable the network connect button.
   * @private
   */
  enableConnect_(
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
    if (managedProperties.type ==
            chromeos.networkConfig.mojom.NetworkType.kCellular &&
        !managedProperties.connectable) {
      return false;
    }
    if (managedProperties.type ==
            chromeos.networkConfig.mojom.NetworkType.kVPN &&
        !defaultNetwork) {
      return false;
    }
    return true;
  },

  /** @private */
  updateAlwaysOnVpnPrefValue_() {
    this.alwaysOnVpn_.value = this.prefs.arc && this.prefs.arc.vpn &&
        this.prefs.arc.vpn.always_on && this.prefs.arc.vpn.always_on.lockdown &&
        this.prefs.arc.vpn.always_on.lockdown.value;
  },

  /**
   * @private
   * @return {!chrome.settingsPrivate.PrefObject}
   */
  getFakeVpnConfigPrefForEnforcement_() {
    const fakeAlwaysOnVpnEnforcementPref = {
      key: 'fakeAlwaysOnPref',
      type: chrome.settingsPrivate.PrefType.BOOLEAN,
      value: false,
    };
    // Only mark VPN networks as enforced. This fake pref also controls the
    // policy indicator on the connect/disconnect buttons, so it shouldn't be
    // shown on non-VPN networks.
    if (this.managedProperties_ &&
        this.managedProperties_.type ==
            chromeos.networkConfig.mojom.NetworkType.kVPN &&
        this.prefs && this.prefs.vpn_config_allowed &&
        !this.prefs.vpn_config_allowed.value) {
      fakeAlwaysOnVpnEnforcementPref.enforcement =
          chrome.settingsPrivate.Enforcement.ENFORCED;
      fakeAlwaysOnVpnEnforcementPref.controlledBy =
          this.prefs.vpn_config_allowed.controlledBy;
    }
    return fakeAlwaysOnVpnEnforcementPref;
  },

  /** @private */
  updateAlwaysOnVpnPrefEnforcement_() {
    const prefForEnforcement = this.getFakeVpnConfigPrefForEnforcement_();
    this.alwaysOnVpn_.enforcement = prefForEnforcement.enforcement;
    this.alwaysOnVpn_.controlledBy = prefForEnforcement.controlledBy;
  },

  /**
   * @return {!TetherConnectionDialogElement}
   * @private
   */
  getTetherDialog_() {
    return /** @type {!TetherConnectionDialogElement} */ (
        this.$$('#tetherDialog'));
  },

  /** @private */
  handleConnectTap_() {
    if (this.managedProperties_.type ==
            chromeos.networkConfig.mojom.NetworkType.kTether &&
        (!this.managedProperties_.typeProperties.tether.hasConnectedToHost)) {
      this.showTetherDialog_();
      return;
    }
    this.fireNetworkConnect_(/*bypassDialog=*/ false);
  },

  /** @private */
  onTetherConnect_() {
    this.getTetherDialog_().close();
    this.fireNetworkConnect_(/*bypassDialog=*/ true);
  },

  /**
   * @param {boolean} bypassDialog
   * @private
   */
  fireNetworkConnect_(bypassDialog) {
    assert(this.managedProperties_);
    const networkState =
        OncMojo.managedPropertiesToNetworkState(this.managedProperties_);
    this.fire(
        'network-connect',
        {networkState: networkState, bypassConnectionDialog: bypassDialog});
    settings.recordSettingChange();
  },

  /** @private */
  handleDisconnectTap_() {
    this.networkConfig_.startDisconnect(this.guid).then(response => {
      if (!response.success) {
        console.error('Disconnect failed for: ' + this.guid);
      }
    });
    settings.recordSettingChange();
  },

  /** @private */
  onConnectDisconnectTap_() {
    if (this.enableConnect_(
            this.managedProperties_, this.defaultNetwork,
            this.propertiesReceived_, this.outOfRange_, this.globalPolicy,
            this.managedNetworkAvailable, this.deviceState_)) {
      this.handleConnectTap_();
      return;
    }

    if (this.showDisconnect_(this.managedProperties_)) {
      this.handleDisconnectTap_();
      return;
    }
  },

  /**
   * @return {boolean}
   * @private
   */
  shouldConnectDisconnectButtonBeHidden_() {
    return !this.showConnect_(
               this.managedProperties_, this.globalPolicy,
               this.managedNetworkAvailable, this.deviceState_) &&
        !this.showDisconnect_(this.managedProperties_);
  },

  /**
   * @return {boolean}
   * @private
   */
  shouldConnectDisconnectButtonBeDisabled_() {
    return !this.enableConnect_(
               this.managedProperties_, this.defaultNetwork,
               this.propertiesReceived_, this.outOfRange_, this.globalPolicy,
               this.managedNetworkAvailable, this.deviceState_) &&
        !this.showDisconnect_(this.managedProperties_);
  },

  /**
   * @return {string}
   * @private
   */
  getConnectDisconnectButtonLabel_() {
    if (this.showConnect_(
            this.managedProperties_, this.globalPolicy,
            this.managedNetworkAvailable, this.deviceState_)) {
      return this.i18n('networkButtonConnect');
    }

    if (this.showDisconnect_(this.managedProperties_)) {
      return this.i18n('networkButtonDisconnect');
    }

    return '';
  },

  /** @private */
  onForgetTap_() {
    this.networkConfig_.forgetNetwork(this.guid).then(response => {
      if (!response.success) {
        console.error('Froget network failed for: ' + this.guid);
      }
      // A forgotten network no longer has a valid GUID, close the subpage.
      this.close();
    });
    settings.recordSettingChange();
  },

  /** @private */
  onActivateTap_() {
    this.browserProxy_.showCellularSetupUI(this.guid);
  },

  /** @private */
  onConfigureTap_() {
    if (this.managedProperties_ &&
        (this.isThirdPartyVpn_(this.managedProperties_) ||
         this.isArcVpn_(this.managedProperties_))) {
      this.browserProxy_.configureThirdPartyVpn(this.guid);
      settings.recordSettingChange();
      return;
    }

    this.fire('show-config', {
      guid: this.guid,
      type: OncMojo.getNetworkTypeString(this.managedProperties_.type),
      name: OncMojo.getNetworkName(this.managedProperties_)
    });
  },

  /** @private */
  onViewAccountTap_() {
    // Currently 'Account Details' is the same as the activation UI.
    this.browserProxy_.showCellularSetupUI(this.guid);
  },

  /** @type {string} */
  CR_EXPAND_BUTTON_TAG: 'CR-EXPAND-BUTTON',

  /** @private */
  showTetherDialog_() {
    this.getTetherDialog_().open();
  },

  /**
   * @return {boolean}
   * @private
   */
  showHiddenNetworkWarning_() {
    return loadTimeData.getBoolean('showHiddenNetworkWarning') &&
        !!this.autoConnectPref_ && !!this.autoConnectPref_.value &&
        !!this.managedProperties_ &&
        this.managedProperties_.type ==
        chromeos.networkConfig.mojom.NetworkType.kWiFi &&
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
  onNetworkPropertyChange_(e) {
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
   * @param {!CustomEvent<!chromeos.networkConfig.mojom.ApnProperties>} event
   * @private
   */
  onApnChange_(event) {
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
   *     value:
   * (string|!chromeos.networkConfig.mojom.IPConfigProperties|!Array<string>)
   * }>} event The network-ip-config or network-nameservers change event.
   * @private
   */
  onIPConfigChange_(event) {
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
  onProxyChange_(event) {
    if (!this.propertiesReceived_) {
      return;
    }
    const config = this.getDefaultConfigProperties_();
    config.proxySettings = event.detail;
    this.setMojoNetworkProperties_(config);
  },

  /**
   * @return {boolean} If managedProperties_ is null or this.isBlockedByPolicy_.
   * @private
   */
  propertiesMissingOrBlockedByPolicy_() {
    return !this.managedProperties_ ||
        this.isBlockedByPolicy_(
            this.managedProperties_, this.globalPolicy,
            this.managedNetworkAvailable);
  },

  /**
   * @param {!chromeos.networkConfig.mojom.ManagedProperties} managedProperties
   * @return {string} To display in the shared notice section.
   * @private
   */
  sharedString_(managedProperties) {
    if (!managedProperties.typeProperties.wifi) {
      return this.i18n('networkShared');
    } else if (managedProperties.typeProperties.wifi.isConfiguredByActiveUser) {
      return this.i18n('networkSharedOwner');
    } else {
      return this.i18n('networkSharedNotOwner');
    }
  },

  /**
   * @param {!chromeos.networkConfig.mojom.ManagedProperties} managedProperties
   * @return {string} To show in the synced notice section.
   * @private
   */
  syncedString_(managedProperties) {
    if (!managedProperties.typeProperties.wifi) {
      return '';
    } else if (!managedProperties.typeProperties.wifi.isSyncable) {
      return this.i18nAdvanced('networkNotSynced');
    } else if (
        managedProperties.source ==
        chromeos.networkConfig.mojom.OncSource.kUser) {
      return this.i18nAdvanced('networkSyncedUser');
    } else {
      return this.i18nAdvanced('networkSyncedDevice');
    }
  },

  /**
   * @param {string} name
   * @param {!chromeos.networkConfig.mojom.ManagedProperties} managedProperties
   * @param {!chromeos.networkConfig.mojom.GlobalPolicy} globalPolicy
   * @param {boolean} managedNetworkAvailable
   * @param {boolean} isSecondaryUser
   * @param {boolean} isWifiSyncEnabled
   * @return {string} Returns 'continuation' class for shared networks.
   * @private
   */
  messagesDividerClass_(
      name, managedProperties, globalPolicy, managedNetworkAvailable,
      isSecondaryUser, isWifiSyncEnabled) {
    let first;
    if (this.isBlockedByPolicy_(
            managedProperties, globalPolicy, managedNetworkAvailable)) {
      first = 'policy';
    } else if (isSecondaryUser) {
      first = 'secondary';
    } else if (this.showShared_(
                   managedProperties, globalPolicy, managedNetworkAvailable)) {
      first = 'shared';
    } else if (this.showSynced_(
                   managedProperties, globalPolicy, managedNetworkAvailable,
                   isWifiSyncEnabled)) {
      first = 'synced';
    }
    return first === name ? 'continuation' : '';
  },

  /**
   * @param {!chromeos.networkConfig.mojom.ManagedProperties} managedProperties
   * @param {!chromeos.networkConfig.mojom.GlobalPolicy} globalPolicy
   * @param {boolean} managedNetworkAvailable
   * @param {boolean} isWifiSyncEnabled
   * @return {boolean} Synced message section should be shown.
   * @private
   */
  showSynced_(
      managedProperties, globalPolicy, managedNetworkAvailable,
      isWifiSyncEnabled) {
    return !this.propertiesMissingOrBlockedByPolicy_() && isWifiSyncEnabled &&
        !!managedProperties.typeProperties.wifi;
  },

  /**
   * @param {!chromeos.networkConfig.mojom.ManagedProperties} managedProperties
   * @param {!chromeos.networkConfig.mojom.GlobalPolicy} globalPolicy
   * @param {boolean} managedNetworkAvailable
   * @return {boolean} If the shared message section should be shown.
   * @private
   */
  showShared_(managedProperties, globalPolicy, managedNetworkAvailable) {
    return !this.propertiesMissingOrBlockedByPolicy_() &&
        (managedProperties.source ==
             chromeos.networkConfig.mojom.OncSource.kDevice ||
         managedProperties.source ==
             chromeos.networkConfig.mojom.OncSource.kDevicePolicy);
  },

  /**
   * @param {!chromeos.networkConfig.mojom.ManagedProperties} managedProperties
   * @param {!chromeos.networkConfig.mojom.GlobalPolicy} globalPolicy
   * @param {boolean} managedNetworkAvailable
   * @return {boolean} True if the AutoConnect checkbox should be shown.
   * @private
   */
  showAutoConnect_(managedProperties, globalPolicy, managedNetworkAvailable) {
    return !!managedProperties &&
        managedProperties.type !=
        chromeos.networkConfig.mojom.NetworkType.kEthernet &&
        this.isRemembered_(managedProperties) &&
        !this.isArcVpn_(managedProperties) &&
        !this.isBlockedByPolicy_(
            managedProperties, globalPolicy, managedNetworkAvailable);
  },

  /**
   * @return {boolean}
   * @private
   */
  showMetered_() {
    const managedProperties = this.managedProperties_;
    return this.showMeteredToggle_ && !!managedProperties &&
        this.isRemembered_(managedProperties) &&
        (managedProperties.type ==
             chromeos.networkConfig.mojom.NetworkType.kCellular ||
         managedProperties.type ==
             chromeos.networkConfig.mojom.NetworkType.kWiFi);
  },

  /**
   * @param {!chromeos.networkConfig.mojom.ManagedProperties} managedProperties
   * @return {boolean} Whether the toggle for the Always-on VPN feature is
   * displayed.
   * @private
   */
  showAlwaysOnVpn_(managedProperties) {
    return this.isArcVpn_(managedProperties) && this.prefs.arc &&
        this.prefs.arc.vpn && this.prefs.arc.vpn.always_on &&
        this.prefs.arc.vpn.always_on.vpn_package &&
        OncMojo.getActiveValue(managedProperties.typeProperties.vpn.host) ===
        this.prefs.arc.vpn.always_on.vpn_package.value;
  },

  /** @private */
  alwaysOnVpnChanged_() {
    if (this.prefs && this.prefs.arc && this.prefs.arc.vpn &&
        this.prefs.arc.vpn.always_on && this.prefs.arc.vpn.always_on.lockdown) {
      this.set(
          'prefs.arc.vpn.always_on.lockdown.value',
          !!this.alwaysOnVpn_ && this.alwaysOnVpn_.value);
    }
  },

  /**
   * @param {!chromeos.networkConfig.mojom.ManagedProperties} managedProperties
   * @param {!chromeos.networkConfig.mojom.GlobalPolicy} globalPolicy
   * @param {boolean} managedNetworkAvailable
   * @return {boolean} True if the prefer network checkbox should be shown.
   * @private
   */
  showPreferNetwork_(managedProperties, globalPolicy, managedNetworkAvailable) {
    if (!managedProperties) {
      return false;
    }

    const type = managedProperties.type;
    if (type == chromeos.networkConfig.mojom.NetworkType.kEthernet ||
        type == chromeos.networkConfig.mojom.NetworkType.kCellular ||
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
  onPreferNetworkRowClicked_(event) {
    // Stop propagation because the toggle and policy indicator handle clicks
    // themselves.
    event.stopPropagation();
    const preferNetworkToggle =
        this.shadowRoot.querySelector('#preferNetworkToggle');
    if (!preferNetworkToggle || preferNetworkToggle.disabled) {
      return;
    }

    this.preferNetwork_ = !this.preferNetwork_;
    settings.recordSettingChange();
  },

  /**
   * @param {!Array<string>} fields
   * @return {boolean}
   * @private
   */
  hasVisibleFields_(fields) {
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
  hasInfoFields_() {
    return this.getInfoEditFieldTypes_().length > 0 ||
        this.hasVisibleFields_(this.getInfoFields_());
  },

  /**
   * @return {!Array<string>} The fields to display in the info section.
   * @private
   */
  getInfoFields_() {
    if (!this.managedProperties_) {
      return [];
    }

    /** @type {!Array<string>} */ const fields = [];
    switch (this.managedProperties_.type) {
      case chromeos.networkConfig.mojom.NetworkType.kCellular:
        fields.push(
            'cellular.activationState', 'cellular.servingOperator.name');
        if (this.managedProperties_.restrictedConnectivity) {
          fields.push('restrictedConnectivity');
        }
        break;
      case chromeos.networkConfig.mojom.NetworkType.kTether:
        fields.push(
            'tether.batteryPercentage', 'tether.signalStrength',
            'tether.carrier');
        break;
      case chromeos.networkConfig.mojom.NetworkType.kVPN:
        const vpnType = this.managedProperties_.typeProperties.vpn.type;
        switch (vpnType) {
          case chromeos.networkConfig.mojom.VpnType.kExtension:
            fields.push('vpn.providerName');
            break;
          case chromeos.networkConfig.mojom.VpnType.kArc:
            fields.push('vpn.type');
            fields.push('vpn.providerName');
            break;
          case chromeos.networkConfig.mojom.VpnType.kOpenVPN:
            fields.push(
                'vpn.type', 'vpn.host', 'vpn.openVpn.username',
                'vpn.openVpn.extraHosts');
            break;
          case chromeos.networkConfig.mojom.VpnType.kL2TPIPsec:
            fields.push('vpn.type', 'vpn.host', 'vpn.l2tp.username');
            break;
        }
        break;
      case chromeos.networkConfig.mojom.NetworkType.kWiFi:
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
  getInfoEditFieldTypes_() {
    if (!this.managedProperties_) {
      return [];
    }

    /** @dict */ const editFields = {};
    const type = this.managedProperties_.type;
    if (type == chromeos.networkConfig.mojom.NetworkType.kVPN) {
      const vpnType = this.managedProperties_.typeProperties.vpn.type;
      if (vpnType != chromeos.networkConfig.mojom.VpnType.kExtension) {
        editFields['vpn.host'] = 'String';
      }
      if (vpnType == chromeos.networkConfig.mojom.VpnType.kOpenVPN) {
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
  getAdvancedFields_() {
    if (!this.managedProperties_) {
      return [];
    }

    /** @type {!Array<string>} */ const fields = [];
    const type = this.managedProperties_.type;
    switch (type) {
      case chromeos.networkConfig.mojom.NetworkType.kCellular:
        fields.push(
            'cellular.family', 'cellular.networkTechnology',
            'cellular.servingOperator.code');
        break;
      case chromeos.networkConfig.mojom.NetworkType.kWiFi:
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
  getDeviceFields_() {
    if (!this.managedProperties_ ||
        this.managedProperties_.type !==
            chromeos.networkConfig.mojom.NetworkType.kCellular) {
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
   * @return {boolean}
   * @private
   */
  hasAdvancedSection_() {
    if (!this.managedProperties_ || !this.propertiesReceived_) {
      return false;
    }
    if (this.showMetered_()) {
      return true;
    }
    if (this.managedProperties_.type ==
        chromeos.networkConfig.mojom.NetworkType.kTether) {
      // These properties apply to the underlying WiFi network, not the Tether
      // network.
      return false;
    }
    return this.hasAdvancedFields_() || this.hasDeviceFields_();
  },

  /**
   * @return {boolean}
   * @private
   */
  hasAdvancedFields_() {
    return this.hasVisibleFields_(this.getAdvancedFields_());
  },

  /**
   * @return {boolean}
   * @private
   */
  hasDeviceFields_() {
    return this.hasVisibleFields_(this.getDeviceFields_());
  },

  /**
   * @param {!chromeos.networkConfig.mojom.ManagedProperties} managedProperties
   * @param {!chromeos.networkConfig.mojom.GlobalPolicy} globalPolicy
   * @param {boolean} managedNetworkAvailable
   * @return {boolean}
   * @private
   */
  hasNetworkSection_(managedProperties, globalPolicy, managedNetworkAvailable) {
    if (!managedProperties ||
        managedProperties.type ==
            chromeos.networkConfig.mojom.NetworkType.kTether) {
      // These settings apply to the underlying WiFi network, not the Tether
      // network.
      return false;
    }
    if (this.isBlockedByPolicy_(
            managedProperties, globalPolicy, managedNetworkAvailable)) {
      return false;
    }
    if (managedProperties.type ==
        chromeos.networkConfig.mojom.NetworkType.kCellular) {
      return true;
    }
    return this.isRememberedOrConnected_(managedProperties);
  },

  /**
   * @param {!chromeos.networkConfig.mojom.ManagedProperties} managedProperties
   * @param {!chromeos.networkConfig.mojom.GlobalPolicy} globalPolicy
   * @param {boolean} managedNetworkAvailable
   * @return {boolean}
   * @private
   */
  hasProxySection_(managedProperties, globalPolicy, managedNetworkAvailable) {
    if (!managedProperties ||
        managedProperties.type ==
            chromeos.networkConfig.mojom.NetworkType.kTether) {
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
   * @param {!chromeos.networkConfig.mojom.ManagedProperties} managedProperties
   * @return {boolean}
   * @private
   */
  showCellularChooseNetwork_(managedProperties) {
    return !!managedProperties &&
        managedProperties.type ==
        chromeos.networkConfig.mojom.NetworkType.kCellular &&
        managedProperties.typeProperties.cellular.supportNetworkScan;
  },

  /**
   * @return {boolean}
   * @private
   */
  showScanningSpinner_() {
    if (!this.managedProperties_ ||
        this.managedProperties_.type !=
            chromeos.networkConfig.mojom.NetworkType.kCellular) {
      return false;
    }
    return !!this.deviceState_ && this.deviceState_.scanning;
  },

  /**
   * @param {!chromeos.networkConfig.mojom.ManagedProperties} managedProperties
   * @return {boolean}
   * @private
   */
  showCellularSim_(managedProperties) {
    return !!managedProperties &&
        managedProperties.type ==
        chromeos.networkConfig.mojom.NetworkType.kCellular &&
        managedProperties.typeProperties.cellular.family != 'CDMA';
  },

  /**
   * @param {!chromeos.networkConfig.mojom.ManagedProperties|undefined}
   *     managedProperties
   * @return {boolean}
   * @private
   */
  isArcVpn_(managedProperties) {
    return !!managedProperties &&
        managedProperties.type ==
        chromeos.networkConfig.mojom.NetworkType.kVPN &&
        managedProperties.typeProperties.vpn.type ==
        chromeos.networkConfig.mojom.VpnType.kArc;
  },

  /**
   * @param {!chromeos.networkConfig.mojom.ManagedProperties|undefined}
   *     managedProperties
   * @return {boolean}
   * @private
   */
  isThirdPartyVpn_(managedProperties) {
    return !!managedProperties &&
        managedProperties.type ==
        chromeos.networkConfig.mojom.NetworkType.kVPN &&
        managedProperties.typeProperties.vpn.type ==
        chromeos.networkConfig.mojom.VpnType.kExtension;
  },

  /**
   * @param {string} ipAddress
   * @param {!chromeos.networkConfig.mojom.ManagedProperties} managedProperties
   * @return {boolean}
   * @private
   */
  showIpAddress_(ipAddress, managedProperties) {
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
  allPropertiesMatch_(curValue, newValue) {
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
  isOutOfRangeOrNotEnabled_(outOfRange, deviceState) {
    return outOfRange ||
        (!!deviceState &&
         deviceState.deviceState !=
             chromeos.networkConfig.mojom.DeviceStateType.kEnabled);
  },
});
