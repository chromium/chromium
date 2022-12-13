// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Polymer element for displaying information about WiFi,
 * Cellular, or virtual networks.
 */

import 'chrome://resources/ash/common/network/network_list.js';
import 'chrome://resources/cr_components/localized_link/localized_link.js';
import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';
import 'chrome://resources/cr_elements/cr_link_row/cr_link_row.js';
import 'chrome://resources/cr_elements/cr_shared_style.css.js';
import 'chrome://resources/cr_elements/cr_shared_vars.css.js';
import 'chrome://resources/cr_elements/cr_toggle/cr_toggle.js';
import 'chrome://resources/cr_elements/md_select.css.js';
import 'chrome://resources/cr_elements/policy/cr_policy_indicator.js';
import 'chrome://resources/polymer/v3_0/iron-flex-layout/iron-flex-layout-classes.js';
import 'chrome://resources/polymer/v3_0/iron-icon/iron-icon.js';
import '../../settings_shared.css.js';
import '../os_settings_icons.css.js';
import './cellular_networks_list.js';
import './network_always_on_vpn.js';

import {I18nBehavior, I18nBehaviorInterface} from 'chrome://resources/ash/common/i18n_behavior.js';
import {CrPolicyNetworkBehaviorMojo, CrPolicyNetworkBehaviorMojoInterface} from 'chrome://resources/ash/common/network/cr_policy_network_behavior_mojo.js';
import {MojoInterfaceProvider, MojoInterfaceProviderImpl} from 'chrome://resources/ash/common/network/mojo_interface_provider.js';
import {NetworkListenerBehavior, NetworkListenerBehaviorInterface} from 'chrome://resources/ash/common/network/network_listener_behavior.js';
import {OncMojo} from 'chrome://resources/ash/common/network/onc_mojo.js';
import {assert, assertNotReached} from 'chrome://resources/ash/common/assert.js';
import {loadTimeData} from 'chrome://resources/ash/common/load_time_data.m.js';
import {AlwaysOnVpnMode, AlwaysOnVpnProperties, CrosNetworkConfigRemote, FilterType, GlobalPolicy, NO_LIMIT, VpnProvider, VpnType} from 'chrome://resources/mojo/chromeos/services/network_config/public/mojom/cros_network_config.mojom-webui.js';
import {ConnectionStateType, DeviceStateType, NetworkType} from 'chrome://resources/mojo/chromeos/services/network_config/public/mojom/network_types.mojom-webui.js';
import {afterNextRender, html, mixinBehaviors, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {Setting} from '../../mojom-webui/setting.mojom-webui.js';
import {Route, Router} from '../router.js';
import {DeepLinkingBehavior, DeepLinkingBehaviorInterface} from '../deep_linking_behavior.js';
import {recordSettingChange} from '../metrics_recorder.js';
import {routes} from '../os_route.js';
import {RouteObserverBehavior, RouteObserverBehaviorInterface} from '../route_observer_behavior.js';
import {RouteOriginBehavior, RouteOriginBehaviorImpl, RouteOriginBehaviorInterface} from '../route_origin_behavior.js';

import {InternetPageBrowserProxy, InternetPageBrowserProxyImpl} from './internet_page_browser_proxy.js';

/**
 * TODO(crbug/1315757) The following type definitions are only needed for
 * Closure compiler and can be removed when this file is converted to TS.
 *
 * @constructor
 * @extends {HTMLElement}
 */
export function CellularNetworksListElement() {}

/** @return {?HTMLElement} */
CellularNetworksListElement.prototype.getAddEsimButton = function() {};

/**
 * @constructor
 * @extends {PolymerElement}
 * @implements {NetworkListenerBehaviorInterface}
 * @implements {CrPolicyNetworkBehaviorMojoInterface}
 * @implements {DeepLinkingBehaviorInterface}
 * @implements {RouteObserverBehaviorInterface}
 * @implements {RouteOriginBehaviorInterface}
 * @implements {I18nBehaviorInterface}
 */
const SettingsInternetSubpageElementBase = mixinBehaviors(
    [
      NetworkListenerBehavior,
      CrPolicyNetworkBehaviorMojo,
      DeepLinkingBehavior,
      RouteObserverBehavior,
      RouteOriginBehavior,
      I18nBehavior,
    ],
    PolymerElement);

/** @polymer */
class SettingsInternetSubpageElement extends
    SettingsInternetSubpageElementBase {
  static get is() {
    return 'settings-internet-subpage';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
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

      /** @type {!GlobalPolicy|undefined} */
      globalPolicy: Object,

      /**
       * List of third party (Extension + Arc) VPN providers.
       * @type {!Array<!VpnProvider>}
       */
      vpnProviders: Array,

      showSpinner: {
        type: Boolean,
        notify: true,
        value: false,
      },

      isConnectedToNonCellularNetwork: {
        type: Boolean,
      },

      isCellularSetupActive: {
        type: Boolean,
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

      /** @private */
      isShowingVpn_: {
        type: Boolean,
        computed: 'computeIsShowingVpn_(deviceState)',
        reflectToAttribute: true,
      },

      /**
       * Whether the browser/ChromeOS is managed by their organization
       * through enterprise policies.
       * @private
       */
      isManaged_: {
        type: Boolean,
        value() {
          return loadTimeData.getBoolean('isManaged');
        },
      },

      /**
       * Always-on VPN operating mode.
       * @private {!AlwaysOnVpnMode|undefined}
       */
      alwaysOnVpnMode_: Number,

      /**
       * Always-on VPN service automatically started on login.
       * @private {!string|undefined}
       */
      alwaysOnVpnService_: String,

      /**
       * List of potential Tether hosts whose "Google Play Services"
       * notifications are disabled (these notifications are required to use
       * Instant Tethering).
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
        },
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
       * @private {?Setting}
       */
      pendingSettingId_: {
        type: Number,
        value: null,
      },

      /**
       * Used by DeepLinkingBehavior to focus this page's deep links.
       * @type {!Set<!Setting>}
       */
      supportedSettingIds: {
        type: Object,
        value: () => new Set([
          Setting.kWifiOnOff,
          Setting.kWifiAddNetwork,
          Setting.kMobileOnOff,
          Setting.kInstantTetheringOnOff,
          Setting.kAddESimNetwork,
        ]),
      },
    };
  }

  static get observers() {
    return [
      'deviceStateChanged_(deviceState)',
      'onAlwaysOnVpnChanged_(alwaysOnVpnMode_, alwaysOnVpnService_)',
    ];
  }

  /** @override */
  constructor() {
    super();

    /** RouteOriginBehavior override */
    this.route_ = routes.INTERNET_NETWORKS;

    /** @private {number|null} */
    this.scanIntervalId_ = null;

    /** @private  {!InternetPageBrowserProxy} */
    this.browserProxy_ = InternetPageBrowserProxyImpl.getInstance();

    /** @private {!CrosNetworkConfigRemote} */
    this.networkConfig_ =
        MojoInterfaceProviderImpl.getInstance().getMojoServiceRemote();
  }

  /** @override */
  ready() {
    super.ready();

    this.browserProxy_.setGmsCoreNotificationsDisabledDeviceNamesCallback(
        (notificationsDisabledDeviceNames) => {
          this.notificationsDisabledDeviceNames_ =
              notificationsDisabledDeviceNames;
        });
    this.browserProxy_.requestGmsCoreNotificationsDisabledDeviceNames();

    this.addFocusConfig(routes.KNOWN_NETWORKS, '#knownNetworksSubpageButton');
  }

  /** override */
  disconnectedCallback() {
    super.disconnectedCallback();

    this.stopScanning_();
  }

  /**
   * Overridden from DeepLinkingBehavior.
   * @param {!Setting} settingId
   * @return {boolean}
   */
  beforeDeepLinkAttempt(settingId) {
    if (settingId === Setting.kAddESimNetwork) {
      afterNextRender(this, () => {
        const deepLinkElement =
            /** @type {CellularNetworksListElement} */ (
                this.shadowRoot.querySelector('cellular-networks-list'))
                .getAddEsimButton();
        if (!deepLinkElement || deepLinkElement.hidden) {
          console.warn(`Element with deep link id ${settingId} not focusable.`);
          return;
        }
        this.showDeepLinkElement(deepLinkElement);
      });
      return false;
    }

    if (settingId === Setting.kInstantTetheringOnOff) {
      // Wait for element to load.
      afterNextRender(this, () => {
        // If both Cellular and Instant Tethering are enabled, we show a special
        // toggle for Instant Tethering. If it exists, deep link to it.
        const tetherEnabled =
            this.shadowRoot.querySelector('#tetherEnabledButton');
        if (tetherEnabled) {
          this.showDeepLinkElement(tetherEnabled);
          return;
        }
        // Otherwise, the device does not support Cellular and Instant Tethering
        // on/off is controlled by the top-level "Mobile data" toggle instead.
        const deviceEnabled =
            this.shadowRoot.querySelector('#deviceEnabledButton');
        if (deviceEnabled) {
          this.showDeepLinkElement(deviceEnabled);
          return;
        }
        console.warn(`Element with deep link id ${settingId} not focusable.`);
      });
      // Stop deep link attempt since we completed it manually.
      return false;
    }
    return true;
  }

  /**
   * RouteObserverBehavior
   * @param {!Route} newRoute
   * @param {!Route=} oldRoute
   * @protected
   */
  currentRouteChanged(newRoute, oldRoute) {
    if (newRoute !== routes.INTERNET_NETWORKS) {
      this.stopScanning_();
      return;
    }
    this.init();
    RouteOriginBehaviorImpl.currentRouteChanged.call(this, newRoute, oldRoute);

    this.attemptDeepLink().then(result => {
      if (!result.deepLinkShown && result.pendingSettingId) {
        // Store any deep link settingId that wasn't shown so we can try again
        // in getNetworkStateList_.
        this.pendingSettingId_ = result.pendingSettingId;
      }
    });
  }

  init() {
    // Clear any stale data.
    this.networkStateList_ = [];
    this.thirdPartyVpns_ = {};
    this.hasCompletedScanSinceLastEnabled_ = false;
    this.showSpinner = false;

    // Request the list of networks and start scanning if necessary.
    this.getNetworkStateList_();
    this.updateScanning_();

    // Get always-on VPN configuration.
    this.updateAlwaysOnVpnPreferences_();
  }

  /**
   * NetworkListenerBehavior override
   * @param {!Array<OncMojo.NetworkStateProperties>} networks
   */
  onActiveNetworksChanged(networks) {
    this.getNetworkStateList_();
  }

  /** NetworkListenerBehavior override */
  onNetworkStateListChanged() {
    this.getNetworkStateList_();
    this.updateAlwaysOnVpnPreferences_();
  }

  /** NetworkListenerBehavior override */
  onVpnProvidersChanged() {
    if (this.deviceState.type !== NetworkType.kVPN) {
      return;
    }
    this.getNetworkStateList_();
  }

  /** @private */
  deviceStateChanged_() {
    if (this.deviceState !== undefined) {
      // Set |vpnIsEnabled_| to be used for VPN special cases.
      if (this.deviceState.type === NetworkType.kVPN) {
        this.vpnIsEnabled_ =
            this.deviceState.deviceState === DeviceStateType.kEnabled;
      }

      // A scan has completed if the spinner was active (i.e., scanning was
      // active) and the device is no longer scanning.
      this.hasCompletedScanSinceLastEnabled_ = this.showSpinner &&
          !this.deviceState.scanning &&
          this.deviceState.deviceState === DeviceStateType.kEnabled;

      // If the cellular network list is showing and currently inhibited, there
      // is a separate spinner that shows in the CellularNetworkList.
      if (this.shouldShowCellularNetworkList_() && this.isDeviceInhibited_()) {
        this.showSpinner = false;
      } else {
        this.showSpinner = !!this.deviceState.scanning;
      }
    }

    // Scans should only be triggered by the "networks" subpage.
    if (Router.getInstance().getCurrentRoute() !== routes.INTERNET_NETWORKS) {
      this.stopScanning_();
      return;
    }

    this.getNetworkStateList_();
    this.updateScanning_();
  }

  /** @private */
  updateScanning_() {
    if (!this.deviceState) {
      return;
    }

    if (this.shouldStartScan_()) {
      this.startScanning_();
      return;
    }
  }

  /**
   * @return {boolean}
   * @private
   */
  shouldStartScan_() {
    // Scans should be kicked off from the Wi-Fi networks subpage.
    if (this.deviceState.type === NetworkType.kWiFi) {
      return true;
    }

    // Scans should be kicked off from the Mobile data subpage, as long as it
    // includes Tether networks.
    if (this.deviceState.type === NetworkType.kTether ||
        (this.deviceState.type === NetworkType.kCellular &&
         this.tetherDeviceState)) {
      return true;
    }

    return false;
  }

  /** @private */
  startScanning_() {
    if (this.scanIntervalId_ != null) {
      return;
    }
    const INTERVAL_MS = 10 * 1000;
    let type = this.deviceState.type;
    if (type === NetworkType.kCellular && this.tetherDeviceState) {
      // Only request tether scan. Cellular scan is disruptive and should
      // only be triggered by explicit user action.
      type = NetworkType.kTether;
    }
    this.networkConfig_.requestNetworkScan(type);
    this.scanIntervalId_ = window.setInterval(() => {
      this.networkConfig_.requestNetworkScan(type);
    }, INTERVAL_MS);
  }

  /** @private */
  stopScanning_() {
    if (this.scanIntervalId_ == null) {
      return;
    }
    window.clearInterval(this.scanIntervalId_);
    this.scanIntervalId_ = null;
  }

  /** @private */
  getNetworkStateList_() {
    if (!this.deviceState) {
      return;
    }
    const filter = {
      filter: FilterType.kVisible,
      limit: NO_LIMIT,
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
  }

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
    if (this.deviceState.type === NetworkType.kCellular &&
        this.tetherDeviceState) {
      const filter = {
        filter: FilterType.kVisible,
        limit: NO_LIMIT,
        networkType: NetworkType.kTether,
      };
      this.networkConfig_.getNetworkStateList(filter).then(response => {
        this.set('networkStateList_', networkStates.concat(response.result));
      });
      return;
    }

    // For VPNs, separate out third party (Extension + Arc) VPNs.
    if (this.deviceState.type === NetworkType.kVPN) {
      const builtinNetworkStates = [];
      const thirdPartyVpns = {};
      networkStates.forEach(state => {
        assert(state.type === NetworkType.kVPN);
        switch (state.typeState.vpn.type) {
          case VpnType.kIKEv2:
          case VpnType.kL2TPIPsec:
          case VpnType.kOpenVPN:
          case VpnType.kWireGuard:
            builtinNetworkStates.push(state);
            break;
          case VpnType.kArc:
            // Only show connected Arc VPNs.
            if (!OncMojo.connectionStateIsConnected(state.connectionState)) {
              break;
            }
          // Otherwise Arc VPNs are treated the same as Extension VPNs.
          case VpnType.kExtension:
            const providerId = state.typeState.vpn.providerId;
            thirdPartyVpns[providerId] = thirdPartyVpns[providerId] || [];
            thirdPartyVpns[providerId].push(state);
            break;
        }
      });
      networkStates = builtinNetworkStates;
      this.thirdPartyVpns_ = thirdPartyVpns;
    }

    this.set('networkStateList_', networkStates);
  }

  /**
   * Returns an ordered list of VPN providers for all third party VPNs and any
   * other known providers.
   * @param {!Array<!VpnProvider>} vpnProviders
   * @param {!Object<!Array<!OncMojo.NetworkStateProperties>>} thirdPartyVpns
   * @return {!Array<!VpnProvider>}
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
        lastLaunchTime: {internalValue: 0},
      };
      configuredProviders.push(provider);
    }
    // Next update or append known third party providers.
    const unconfiguredProviders = [];
    for (const provider of vpnProviders) {
      const idx = configuredProviders.findIndex(
          p => p.providerId === provider.providerId);
      if (idx >= 0) {
        configuredProviders[idx] = provider;
      } else {
        unconfiguredProviders.push(provider);
      }
    }
    return configuredProviders.concat(unconfiguredProviders);
  }

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
        (deviceState.type === NetworkType.kVPN ||
         deviceState.deviceState === DeviceStateType.kEnabled);
  }

  /**
   * @param {!OncMojo.DeviceStateProperties|undefined} deviceState
   * @param {string} onstr
   * @param {string} offstr
   * @return {string}
   * @private
   */
  getOffOnString_(deviceState, onstr, offstr) {
    return this.deviceIsEnabled_(deviceState) ? onstr : offstr;
  }

  /**
   * @param {!OncMojo.DeviceStateProperties|undefined} deviceState
   * @return {boolean}
   * @private
   */
  enableToggleIsVisible_(deviceState) {
    return !!deviceState && deviceState.type !== NetworkType.kEthernet &&
        deviceState.type !== NetworkType.kVPN;
  }

  /**
   * @param {!OncMojo.DeviceStateProperties|undefined} deviceState
   * @return {boolean}
   * @private
   */
  enableToggleIsEnabled_(deviceState) {
    if (!deviceState) {
      return false;
    }
    if (deviceState.deviceState === DeviceStateType.kProhibited) {
      return false;
    }
    if (OncMojo.deviceStateIsIntermediate(deviceState.deviceState)) {
      return false;
    }
    return !this.isDeviceInhibited_();
  }

  /**
   * @return {boolean}
   * @private
   */
  isDeviceInhibited_() {
    if (!this.deviceState) {
      return false;
    }
    return OncMojo.deviceIsInhibited(this.deviceState);
  }

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
      case NetworkType.kTether:
      case NetworkType.kCellular:
        return this.i18n('internetToggleMobileA11yLabel');
      case NetworkType.kWiFi:
        return this.i18n('internetToggleWiFiA11yLabel');
    }
    assertNotReached();
    return '';
  }

  /**
   * @param {!VpnProvider} provider
   * @return {string}
   * @private
   */
  getAddThirdPartyVpnA11yString_(provider) {
    return this.i18n('internetAddThirdPartyVPN', provider.providerName || '');
  }

  /**
   * @param {!OncMojo.DeviceStateProperties|undefined} deviceState
   * @param {!GlobalPolicy} globalPolicy
   * @return {boolean}
   * @private
   */
  allowAddConnection_(deviceState, globalPolicy) {
    if (!this.deviceIsEnabled_(deviceState)) {
      return false;
    }
    return globalPolicy && !globalPolicy.allowOnlyPolicyWifiNetworksToConnect;
  }

  /**
   * @param {!OncMojo.DeviceStateProperties|undefined} deviceState
   * @param {!GlobalPolicy} globalPolicy
   * @return {boolean}
   * @private
   */
  showAddWifiButton_(deviceState, globalPolicy) {
    if (!deviceState || deviceState.type !== NetworkType.kWiFi) {
      return false;
    }
    return this.allowAddConnection_(deviceState, globalPolicy);
  }

  /**
   * @private
   * @param {!string} type
   */
  dispatchShowConfigEvent_(type) {
    const event = new CustomEvent('show-config', {
      bubbles: true,
      composed: true,
      detail: {type},
    });
    this.dispatchEvent(event);
  }

  /** @private */
  onAddWifiButtonTap_() {
    assert(this.deviceState, 'Device state is falsey - Wifi expected.');
    const type = this.deviceState.type;
    assert(type === NetworkType.kWiFi, 'Wifi type expected.');
    this.dispatchShowConfigEvent_(OncMojo.getNetworkTypeString(type));
  }

  /** @private */
  onAddVpnButtonTap_() {
    assert(this.deviceState, 'Device state is falsey - VPN expected.');
    const type = this.deviceState.type;
    assert(type === NetworkType.kVPN, 'VPN type expected.');
    this.dispatchShowConfigEvent_(OncMojo.getNetworkTypeString(type));
  }

  /**
   * @param {!{model: !{item: !VpnProvider}}} event
   * @private
   */
  onAddThirdPartyVpnTap_(event) {
    const provider = event.model.item;
    this.browserProxy_.addThirdPartyVpn(provider.appId);
    recordSettingChange();
  }

  /**
   * @param {!OncMojo.DeviceStateProperties|undefined} deviceState
   * @return {boolean}
   * @private
   */
  knownNetworksIsVisible_(deviceState) {
    return !!deviceState && deviceState.type === NetworkType.kWiFi;
  }

  /**
   * Event triggered when the known networks button is clicked.
   * @private
   */
  onKnownNetworksTap_() {
    assert(this.deviceState.type === NetworkType.kWiFi);
    const showKnownNetworksEvent = new CustomEvent('show-known-networks', {
      bubbles: true,
      composed: true,
      detail: this.deviceState.type,
    });
    this.dispatchEvent(showKnownNetworksEvent);
  }

  /**
   * Event triggered when the enable button is toggled.
   * @param {!Event} event
   * @private
   */
  onDeviceEnabledChange_(event) {
    assert(this.deviceState);
    const deviceEnabledToggledEvent =
        new CustomEvent('device-enabled-toggled', {
          bubbles: true,
          composed: true,
          detail: {
            enabled: !this.deviceIsEnabled_(this.deviceState),
            type: this.deviceState.type,
          },
        });
    this.dispatchEvent(deviceEnabledToggledEvent);
  }

  /**
   * @param {!Object<!Array<!OncMojo.NetworkStateProperties>>} thirdPartyVpns
   * @param {!VpnProvider} provider
   * @return {!Array<!OncMojo.NetworkStateProperties>}
   * @private
   */
  getThirdPartyVpnNetworks_(thirdPartyVpns, provider) {
    return thirdPartyVpns[provider.providerId] || [];
  }

  /**
   * @param {!Object<!Array<!OncMojo.NetworkStateProperties>>} thirdPartyVpns
   * @param {!VpnProvider} provider
   * @return {boolean}
   * @private
   */
  haveThirdPartyVpnNetwork_(thirdPartyVpns, provider) {
    const list = this.getThirdPartyVpnNetworks_(thirdPartyVpns, provider);
    return !!list.length;
  }

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
      const networkConnectEvent = new CustomEvent('network-connect', {
        bubbles: true,
        composed: true,
        detail: {networkState},
      });
      this.dispatchEvent(networkConnectEvent);
      recordSettingChange();
      return;
    }

    const showDetailEvent = new CustomEvent('show-detail', {
      bubbles: true,
      composed: true,
      detail: networkState,
    });
    this.dispatchEvent(showDetailEvent);
  }

  /**
   * @param {!OncMojo.NetworkStateProperties} state The network state.
   * @return {boolean}
   * @private
   */
  isBlockedByPolicy_(state) {
    if (state.type !== NetworkType.kWiFi &&
        state.type !== NetworkType.kCellular) {
      return false;
    }
    if (this.isPolicySource(state.source) || !this.globalPolicy) {
      return false;
    }

    if (state.type === NetworkType.kCellular) {
      return !!this.globalPolicy.allowOnlyPolicyCellularNetworks;
    }

    return !!this.globalPolicy.allowOnlyPolicyWifiNetworksToConnect ||
        (!!this.globalPolicy.allowOnlyPolicyWifiNetworksToConnectIfAvailable &&
         !!this.deviceState && !!this.deviceState.managedNetworkAvailable) ||
        (!!this.globalPolicy.blockedHexSsids &&
         this.globalPolicy.blockedHexSsids.includes(
             state.typeState.wifi.hexSsid));
  }

  /**
   * Determines whether or not it is possible to attempt a connection to the
   * provided network (e.g., whether it's possible to connect or configure the
   * network for connection).
   * @param {!OncMojo.NetworkStateProperties} state The network state.
   * @private
   */
  canAttemptConnection_(state) {
    if (state.connectionState !== ConnectionStateType.kNotConnected) {
      return false;
    }

    if (this.isBlockedByPolicy_(state)) {
      return false;
    }

    // VPNs can only be connected if there is an existing network connection to
    // use with the VPN.
    if (state.type === NetworkType.kVPN &&
        (!this.defaultNetwork ||
         !OncMojo.connectionStateIsConnected(
             this.defaultNetwork.connectionState))) {
      return false;
    }

    // Locked SIM profiles must be unlocked before a connection can occur.
    if (state.type === NetworkType.kCellular &&
        state.typeState.cellular.simLocked) {
      return false;
    }

    return true;
  }

  /**
   * @param {string} typeString
   * @param {OncMojo.DeviceStateProperties} device
   * @return {boolean}
   * @private
   */
  matchesType_(typeString, device) {
    return !!device &&
        device.type === OncMojo.getNetworkTypeFromString(typeString);
  }

  /**
   * @param {!Array<!OncMojo.NetworkStateProperties>}
   *     networkStateList
   * @return {boolean}
   * @private
   */
  shouldShowNetworkList_(networkStateList) {
    if (this.shouldShowCellularNetworkList_()) {
      return false;
    }

    if (!!this.deviceState && this.deviceState.type === NetworkType.kVPN) {
      return this.shouldShowVpnList_();
    }
    return this.networkStateList_.length > 0;
  }

  /**
   * @return {boolean} True if native VPN is not disabled by policy and there
   *     are more than one VPN network configured.
   * @private
   */
  shouldShowVpnList_() {
    return this.vpnIsEnabled_ && this.networkStateList_.length > 0;
  }

  /**
   * @return {boolean}
   * @private
   */
  shouldShowCellularNetworkList_() {
    // Only shown if the currently-active subpage is for Cellular networks.
    return !!this.deviceState &&
        this.deviceState.type === NetworkType.kCellular;
  }

  /**
   * @param {!Array<!OncMojo.NetworkStateProperties>}
   *     networkStateList
   * @return {boolean}
   * @private
   */
  hideNoNetworksMessage_(networkStateList) {
    return this.shouldShowCellularNetworkList_() ||
        this.shouldShowNetworkList_(networkStateList);
  }

  /**
   * @param {!OncMojo.DeviceStateProperties|undefined} deviceState
   * @param {!OncMojo.DeviceStateProperties|undefined} tetherDeviceState
   * @return {string}
   * @private
   */
  getNoNetworksInnerHtml_(deviceState, tetherDeviceState) {
    const type = deviceState.type;
    if (type === NetworkType.kTether ||
        (type === NetworkType.kCellular && this.tetherDeviceState)) {
      return this.i18nAdvanced('internetNoNetworksMobileData');
    }

    if (type === NetworkType.kVPN) {
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
  }

  /**
   * @param {!Array<string>} notificationsDisabledDeviceNames
   * @return {boolean}
   * @private
   */
  showGmsCoreNotificationsSection_(notificationsDisabledDeviceNames) {
    return notificationsDisabledDeviceNames.length > 0;
  }

  /**
   * @param {!Array<string>} notificationsDisabledDeviceNames
   * @return {string}
   * @private
   */
  getGmsCoreNotificationsDevicesString_(notificationsDisabledDeviceNames) {
    if (notificationsDisabledDeviceNames.length === 1) {
      return this.i18n(
          'gmscoreNotificationsOneDeviceSubtitle',
          notificationsDisabledDeviceNames[0]);
    }

    if (notificationsDisabledDeviceNames.length === 2) {
      return this.i18n(
          'gmscoreNotificationsTwoDevicesSubtitle',
          notificationsDisabledDeviceNames[0],
          notificationsDisabledDeviceNames[1]);
    }

    return this.i18n('gmscoreNotificationsManyDevicesSubtitle');
  }

  /**
   * @return {boolean}
   * @private
   */
  computeIsShowingVpn_() {
    if (!this.deviceState) {
      return false;
    }
    return this.matchesType_(
        OncMojo.getNetworkTypeString(NetworkType.kVPN), this.deviceState);
  }

  /**
   * Tells when VPN preferences section should be displayed. It is
   * displayed when the preferences are applicable to the current device.
   * @return {boolean}
   * @private
   */
  shouldShowVpnPreferences_() {
    if (!this.deviceState) {
      return false;
    }
    // For now the section only contain always-on VPN settings. It should not be
    // displayed on managed devices while the legacy always-on VPN based on ARC
    // is not replaced/extended by the new implementation.
    return !this.isManaged_ && this.isShowingVpn_;
  }

  /**
   * Generates the list of VPN services available for always-on. It keeps from
   * the network list only the supported technologies.
   * @return {!Array<!OncMojo.NetworkStateProperties>}
   * @private
   */
  getAlwaysOnVpnNetworks_() {
    if (!this.deviceState || this.deviceState.type !== NetworkType.kVPN) {
      return [];
    }

    /** @type {!Array<!OncMojo.NetworkStateProperties>} */
    const alwaysOnVpnList = this.networkStateList_.slice();
    for (const vpnList of Object.values(this.thirdPartyVpns_)) {
      assert(vpnList.length > 0);
      // Exclude incompatible VPN technologies:
      // - TODO(b/188864779): ARC VPNs are not supported yet,
      // - Chrome VPN apps are deprecated and incompatible with lockdown mode
      //   (see b/206910855).
      if (vpnList[0].typeState.vpn.type === VpnType.kArc ||
          vpnList[0].typeState.vpn.type === VpnType.kExtension) {
        continue;
      }
      alwaysOnVpnList.push(...vpnList);
    }

    return alwaysOnVpnList;
  }

  /**
   * Fetches the always-on VPN configuration from network config.
   * @private
   */
  updateAlwaysOnVpnPreferences_() {
    if (!this.deviceState || this.deviceState.type !== NetworkType.kVPN) {
      return;
    }

    this.networkConfig_.getAlwaysOnVpn().then(result => {
      this.alwaysOnVpnMode_ = result.properties.mode;
      this.alwaysOnVpnService_ = result.properties.serviceGuid;
    });
  }

  /**
   * Handles a change in |alwaysOnVpnMode_| or |alwaysOnVpnService_|
   * triggered via the observer.
   * @private
   */
  onAlwaysOnVpnChanged_() {
    if (this.alwaysOnVpnMode_ === undefined ||
        this.alwaysOnVpnService_ === undefined) {
      return;
    }

    /** @type {!AlwaysOnVpnProperties} */
    const properties = {
      mode: this.alwaysOnVpnMode_,
      serviceGuid: this.alwaysOnVpnService_,
    };
    this.networkConfig_.setAlwaysOnVpn(properties);
  }
}

customElements.define(
    SettingsInternetSubpageElement.is, SettingsInternetSubpageElement);
