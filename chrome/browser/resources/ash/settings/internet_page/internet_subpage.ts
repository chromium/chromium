// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Polymer element for displaying information about WiFi,
 * Cellular, or virtual networks.
 */

import 'chrome://resources/ash/common/network/network_list.js';
import 'chrome://resources/ash/common/cr_elements/localized_link/localized_link.js';
import 'chrome://resources/ash/common/cr_elements/cr_icon_button/cr_icon_button.js';
import 'chrome://resources/ash/common/cr_elements/cr_link_row/cr_link_row.js';
import 'chrome://resources/ash/common/cr_elements/cr_shared_style.css.js';
import 'chrome://resources/ash/common/cr_elements/cr_shared_vars.css.js';
import 'chrome://resources/ash/common/cr_elements/cr_toggle/cr_toggle.js';
import 'chrome://resources/ash/common/cr_elements/md_select.css.js';
import 'chrome://resources/ash/common/cr_elements/policy/cr_policy_indicator.js';
import 'chrome://resources/polymer/v3_0/iron-flex-layout/iron-flex-layout-classes.js';
import 'chrome://resources/polymer/v3_0/iron-icon/iron-icon.js';
import '../settings_shared.css.js';
import '../os_settings_icons.css.js';
import './cellular_networks_list.js';
import './network_always_on_vpn.js';
import './internet_subpage_menu.js';
import '/shared/settings/prefs/prefs.js';

import {PrefsMixin, PrefsMixinInterface} from '/shared/settings/prefs/prefs_mixin.js';
import {I18nMixin, I18nMixinInterface} from 'chrome://resources/ash/common/cr_elements/i18n_mixin.js';
import {CrPolicyNetworkBehaviorMojo, CrPolicyNetworkBehaviorMojoInterface} from 'chrome://resources/ash/common/network/cr_policy_network_behavior_mojo.js';
import {MojoInterfaceProviderImpl} from 'chrome://resources/ash/common/network/mojo_interface_provider.js';
import {NetworkListenerBehavior, NetworkListenerBehaviorInterface} from 'chrome://resources/ash/common/network/network_listener_behavior.js';
import {OncMojo} from 'chrome://resources/ash/common/network/onc_mojo.js';
import {assert, assertNotReached} from 'chrome://resources/js/assert.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {AlwaysOnVpnMode, AlwaysOnVpnProperties, CrosNetworkConfigInterface, FilterType, GlobalPolicy, NO_LIMIT, VpnProvider, VpnType} from 'chrome://resources/mojo/chromeos/services/network_config/public/mojom/cros_network_config.mojom-webui.js';
import {ConnectionStateType, DeviceStateType, NetworkType} from 'chrome://resources/mojo/chromeos/services/network_config/public/mojom/network_types.mojom-webui.js';
import {afterNextRender, DomRepeatEvent, mixinBehaviors, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {castExists} from '../assert_extras.js';
import {DeepLinkingMixin, DeepLinkingMixinInterface} from '../common/deep_linking_mixin.js';
import {RouteOriginMixin, RouteOriginMixinInterface} from '../common/route_origin_mixin.js';
import {Setting} from '../mojom-webui/setting.mojom-webui.js';
import {Route, Router, routes} from '../router.js';

import {InternetPageBrowserProxy, InternetPageBrowserProxyImpl} from './internet_page_browser_proxy.js';
import {getTemplate} from './internet_subpage.html.js';

const SettingsInternetSubpageElementBase =
    mixinBehaviors(
        [
          NetworkListenerBehavior,
          CrPolicyNetworkBehaviorMojo,
        ],
        DeepLinkingMixin(
            PrefsMixin(RouteOriginMixin(I18nMixin(PolymerElement))))) as {
      new (): PolymerElement & I18nMixinInterface & RouteOriginMixinInterface &
          PrefsMixinInterface & DeepLinkingMixinInterface &
          NetworkListenerBehaviorInterface &
          CrPolicyNetworkBehaviorMojoInterface,
    };

export class SettingsInternetSubpageElement extends
    SettingsInternetSubpageElementBase {
  static get is() {
    return 'settings-internet-subpage' as const;
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      /**
       * Highest priority connected network or null. Provided by
       * settings-internet-page (but set in network-summary).
       */
      defaultNetwork: Object,

      /**
       * Device state for the network type. Note: when both Cellular and Tether
       * are available this will always be set to the Cellular device state and
       * |tetherDeviceState| will be set to the Tether device state.
       */
      deviceState: Object,

      /**
       * If both Cellular and Tether technologies exist, we combine the subpages
       * and set this to the device state for Tether.
       */
      tetherDeviceState: Object,

      globalPolicy: Object,

      /**
       * List of third party (Extension + Arc) VPN providers.
       */
      vpnProviders: Array,

      isBuiltInVpnManagementBlocked: {
        type: Boolean,
        value: false,
      },

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
       */
      networkStateList_: {
        type: Array,
        value() {
          return [];
        },
      },

      /**
       * Dictionary of lists of network states for third party VPNs.
       */
      thirdPartyVpns_: {
        type: Object,
        value() {
          return {};
        },
      },

      /**
       * Return true if instant hotspot rebrand feature flag is enabled.
       */
      isInstantHotspotRebrandEnabled_: {
        type: Boolean,
        value() {
          return loadTimeData.valueExists('isInstantHotspotRebrandEnabled') &&
              loadTimeData.getBoolean('isInstantHotspotRebrandEnabled');
        },
      },

      isShowingVpn_: {
        type: Boolean,
        computed: 'computeIsShowingVpn_(deviceState)',
        reflectToAttribute: true,
      },

      isShowingTether_: {
        type: Boolean,
        computed: 'computeIsShowingTether_(deviceState)',
        reflectToAttribute: true,
      },

      /**
       * Whether the browser/ChromeOS is managed by their organization
       * through enterprise policies.
       */
      isManaged_: {
        type: Boolean,
        value() {
          return loadTimeData.getBoolean('isManaged');
        },
      },

      /**
       * Always-on VPN operating mode.
       */
      alwaysOnVpnMode_: Number,

      /**
       * Always-on VPN service automatically started on login.
       */
      alwaysOnVpnService_: String,

      /**
       * List of potential Tether hosts whose "Google Play Services"
       * notifications are disabled (these notifications are required to use
       * Instant Tethering).
       */
      notificationsDisabledDeviceNames_: {
        type: Array,
        value() {
          return [];
        },
      },

      /**
       * Whether to show technology badge on mobile network icons.
       */
      showTechnologyBadge_: {
        type: Boolean,
        value() {
          return loadTimeData.valueExists('showTechnologyBadge') &&
              loadTimeData.getBoolean('showTechnologyBadge');
        },
      },

      hasCompletedScanSinceLastEnabled_: {
        type: Boolean,
        value: false,
      },

      /**
       * False if VPN is disabled by policy.
       */
      vpnIsEnabled_: {
        type: Boolean,
        value: false,
      },

      /**
       * Contains the settingId of any deep link that wasn't able to be shown,
       * null otherwise.
       */
      pendingSettingId_: {
        type: Number,
        value: null,
      },

      /**
       * Used by DeepLinkingMixin to focus this page's deep links.
       */
      supportedSettingIds: {
        type: Object,
        value: () => new Set<Setting>([
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

  defaultNetwork: OncMojo.NetworkStateProperties|null|undefined;
  deviceState: OncMojo.DeviceStateProperties|undefined;
  globalPolicy: GlobalPolicy|undefined;
  isBuiltInVpnManagementBlocked: boolean;
  isCellularSetupActive: boolean;
  isConnectedToNonCellularNetwork: boolean;
  showSpinner: boolean;
  tetherDeviceState: OncMojo.DeviceStateProperties|undefined;
  vpnProviders: VpnProvider[];
  private alwaysOnVpnMode_: AlwaysOnVpnMode|undefined;
  private alwaysOnVpnService_: string|undefined;
  private browserProxy_: InternetPageBrowserProxy;
  private hasCompletedScanSinceLastEnabled_: boolean;
  private isInstantHotspotRebrandEnabled_: boolean;
  private isManaged_: boolean;
  private isShowingTether_: boolean;
  private isShowingVpn_: boolean;
  private networkConfig_: CrosNetworkConfigInterface;
  private networkStateList_: OncMojo.NetworkStateProperties[];
  private notificationsDisabledDeviceNames_: string[];
  private pendingSettingId_: Setting|null;
  private scanIntervalId_: number|null;
  private showTechnologyBadge_: boolean;
  private thirdPartyVpns_: Record<string, OncMojo.NetworkStateProperties[]>;
  private vpnIsEnabled_: boolean;

  constructor() {
    super();

    /** RouteOriginMixin override */
    this.route = routes.INTERNET_NETWORKS;

    this.scanIntervalId_ = null;
    this.browserProxy_ = InternetPageBrowserProxyImpl.getInstance();
    this.networkConfig_ =
        MojoInterfaceProviderImpl.getInstance().getMojoServiceRemote();
  }

  override ready(): void {
    super.ready();

    this.browserProxy_.setGmsCoreNotificationsDisabledDeviceNamesCallback(
        (notificationsDisabledDeviceNames) => {
          this.notificationsDisabledDeviceNames_ =
              notificationsDisabledDeviceNames;
        });
    this.browserProxy_.requestGmsCoreNotificationsDisabledDeviceNames();

    this.addFocusConfig(routes.KNOWN_NETWORKS, '#knownNetworksSubpageButton');
  }

  override disconnectedCallback(): void {
    super.disconnectedCallback();

    this.stopScanning_();
  }

  /**
   * Overridden from DeepLinkingMixin.
   */
  override beforeDeepLinkAttempt(settingId: Setting): boolean {
    if (settingId === Setting.kAddESimNetwork) {
      afterNextRender(this, () => {
        const deepLinkElement =
            this.shadowRoot!.querySelector(
                                'cellular-networks-list')!.getAddEsimButton();
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
            this.shadowRoot!.querySelector<HTMLElement>('#tetherEnabledButton');
        if (tetherEnabled) {
          this.showDeepLinkElement(tetherEnabled);
          return;
        }
        // Otherwise, the device does not support Cellular and Instant Tethering
        // on/off is controlled by the top-level "Mobile data" toggle instead.
        const deviceEnabled =
            this.shadowRoot!.querySelector<HTMLElement>('#deviceEnabledButton');
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
   * RouteObserverMixin override
   */
  override currentRouteChanged(newRoute: Route, oldRoute?: Route): void {
    if (newRoute !== this.route) {
      this.stopScanning_();
      return;
    }
    this.init();
    super.currentRouteChanged(newRoute, oldRoute);

    this.attemptDeepLink().then(result => {
      if (!result.deepLinkShown && result.pendingSettingId) {
        // Store any deep link settingId that wasn't shown so we can try again
        // in getNetworkStateList_.
        this.pendingSettingId_ = result.pendingSettingId;
      }
    });
  }

  init(): void {
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
   */
  override onActiveNetworksChanged(): void {
    this.getNetworkStateList_();
  }

  /** NetworkListenerBehavior override */
  override onNetworkStateListChanged(): void {
    this.getNetworkStateList_();
    this.updateAlwaysOnVpnPreferences_();
  }

  /** NetworkListenerBehavior override */
  override onVpnProvidersChanged(): void {
    if (this.deviceState!.type !== NetworkType.kVPN) {
      return;
    }
    this.getNetworkStateList_();
  }

  private deviceStateChanged_(): void {
    if (this.deviceState) {
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
    if (Router.getInstance().currentRoute !== routes.INTERNET_NETWORKS) {
      this.stopScanning_();
      return;
    }

    this.getNetworkStateList_();
    this.updateScanning_();
  }

  private updateScanning_(): void {
    if (!this.deviceState) {
      return;
    }

    if (this.shouldStartScan_()) {
      this.startScanning_();
      return;
    }
  }

  private shouldStartScan_(): boolean {
    assert(this.deviceState);
    // Scans should be kicked off from the Wi-Fi networks subpage.
    if (this.deviceState.type === NetworkType.kWiFi) {
      return true;
    }

    // Scans should be kicked off from the new Instant Hotspot page.
    return this.deviceState.type === NetworkType.kTether ||
        (this.deviceState.type === NetworkType.kCellular &&
         !!this.tetherDeviceState && !this.isInstantHotspotRebrandEnabled_);
  }

  private startScanning_(): void {
    if (this.scanIntervalId_ !== null) {
      return;
    }
    const INTERVAL_MS = 10 * 1000;
    let type = this.deviceState!.type;
    if (!this.isInstantHotspotRebrandEnabled_ &&
        type === NetworkType.kCellular && this.tetherDeviceState) {
      // Only request tether scan. Cellular scan is disruptive and should
      // only be triggered by explicit user action.
      type = NetworkType.kTether;
    }
    this.networkConfig_.requestNetworkScan(type);
    this.scanIntervalId_ = window.setInterval(() => {
      this.networkConfig_.requestNetworkScan(type);
    }, INTERVAL_MS);
  }

  private stopScanning_(): void {
    if (this.scanIntervalId_ === null) {
      return;
    }
    window.clearInterval(this.scanIntervalId_);
    this.scanIntervalId_ = null;
  }

  private async getNetworkStateList_(): Promise<void> {
    if (!this.deviceState) {
      return;
    }
    const filter = {
      filter: FilterType.kVisible,
      limit: NO_LIMIT,
      networkType: this.deviceState.type,
    };
    const response = await this.networkConfig_.getNetworkStateList(filter);
    await this.onGetNetworks_(response.result);

    // Check if we have yet to focus a deep-linked element.
    if (!this.pendingSettingId_) {
      return;
    }

    const result = await this.showDeepLink(this.pendingSettingId_);
    if (result.deepLinkShown) {
      this.pendingSettingId_ = null;
    }
  }

  private async onGetNetworks_(networkStates: OncMojo.NetworkStateProperties[]):
      Promise<void> {
    if (!this.deviceState) {
      // Edge case when device states change before this callback.
      return;
    }

    // For the Cellular/Mobile subpage, also request Tether networks.
    if (!this.isInstantHotspotRebrandEnabled_ &&
        this.deviceState.type === NetworkType.kCellular &&
        this.tetherDeviceState) {
      const filter = {
        filter: FilterType.kVisible,
        limit: NO_LIMIT,
        networkType: NetworkType.kTether,
      };
      const response = await this.networkConfig_.getNetworkStateList(filter);
      this.set('networkStateList_', networkStates.concat(response.result));
      return;
    }

    // For VPNs, separate out third party (Extension + Arc) VPNs.
    if (this.deviceState.type === NetworkType.kVPN) {
      const builtinNetworkStates: OncMojo.NetworkStateProperties[] = [];
      const thirdPartyVpns:
          Record<string, OncMojo.NetworkStateProperties[]> = {};
      networkStates.forEach(state => {
        assert(state.type === NetworkType.kVPN && state.typeState.vpn);
        switch (state.typeState.vpn.type) {
          case VpnType.kIKEv2:
          case VpnType.kL2TPIPsec:
          case VpnType.kOpenVPN:
          case VpnType.kWireGuard:
            builtinNetworkStates.push(state);
            break;
          // @ts-expect-error Fallthrough case in switch
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
   */
  private getVpnProviders_(
      vpnProviders: VpnProvider[],
      thirdPartyVpns: Record<string, OncMojo.NetworkStateProperties[]>):
      VpnProvider[] {
    // First add providers for configured thirdPartyVpns. This list will
    // generally be empty or small.
    const configuredProviders = [];
    for (const vpnList of Object.values(thirdPartyVpns)) {
      assert(vpnList.length > 0);
      // All vpns in the list will have the same type and provider id.
      const vpn = castExists(vpnList[0].typeState.vpn);
      const provider = {
        type: vpn.type,
        providerId: vpn.providerId,
        providerName: vpn.providerName || vpn.providerId,
        appId: '',
        lastLaunchTime: {internalValue: BigInt(0)},
      };
      configuredProviders.push(provider);
    }
    // Next update or append known third party providers.
    const unconfiguredProviders = [];
    for (const provider of vpnProviders) {
      const idx: number = configuredProviders.findIndex(
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
   * @return True if the device is enabled or if it is a VPN.
   *     Note: This function will always return true for VPN because VPNs can be
   *     disabled by policy only for built-in VPNs (OpenVPN & L2TP). So even
   *     when VPNs are disabled by policy; the VPN network summary item should
   *     still be visible and actionable to show details for other VPN
   *     providers.
   */
  private deviceIsEnabled_(deviceState: OncMojo.DeviceStateProperties|
                           undefined): boolean {
    if (OncMojo.deviceIsFlashing(deviceState)) {
      return false;
    }

    return !!deviceState &&
        (deviceState.type === NetworkType.kVPN ||
         deviceState.deviceState === DeviceStateType.kEnabled);
  }

  private getOffOnString_(
      deviceState: OncMojo.DeviceStateProperties|undefined, onstr: string,
      offstr: string): string {
    return this.deviceIsEnabled_(deviceState) ? onstr : offstr;
  }

  private enableToggleIsVisible_(deviceState: OncMojo.DeviceStateProperties|
                                 undefined): boolean {
    return !!deviceState && deviceState.type !== NetworkType.kEthernet &&
        deviceState.type !== NetworkType.kVPN &&
        (!this.isInstantHotspotRebrandEnabled_ ||
         deviceState.type !== NetworkType.kTether);
  }

  private enableToggleIsEnabled_(deviceState: OncMojo.DeviceStateProperties|
                                 undefined): boolean {
    if (!deviceState) {
      return false;
    }
    if (deviceState.deviceState === DeviceStateType.kProhibited) {
      return false;
    }
    if (OncMojo.deviceStateIsIntermediate(deviceState.deviceState)) {
      return false;
    }
    if (OncMojo.deviceIsFlashing(deviceState)) {
      return false;
    }
    return !this.isDeviceInhibited_();
  }

  private isDeviceInhibited_(): boolean {
    if (!this.deviceState) {
      return false;
    }
    return OncMojo.deviceIsInhibited(this.deviceState);
  }

  private getToggleA11yString_(deviceState: OncMojo.DeviceStateProperties|
                               undefined): string {
    if (!this.enableToggleIsVisible_(deviceState)) {
      return '';
    }
    switch (deviceState!.type) {
      case NetworkType.kTether:
        return this.i18n('internetToggleTetherA11yLabel');
      case NetworkType.kCellular:
        return this.i18n('internetToggleMobileA11yLabel');
      case NetworkType.kWiFi:
        return this.i18n('internetToggleWiFiA11yLabel');
    }
    assertNotReached();
  }

  private getAddThirdPartyVpnA11yString_(provider: VpnProvider): string {
    return this.i18n('internetAddThirdPartyVPN', provider.providerName || '');
  }

  private allowAddConnection_(
      deviceState: OncMojo.DeviceStateProperties|undefined,
      globalPolicy: GlobalPolicy): boolean {
    if (!this.deviceIsEnabled_(deviceState)) {
      return false;
    }
    return globalPolicy && !globalPolicy.allowOnlyPolicyWifiNetworksToConnect;
  }

  private showAddWifiButton_(
      deviceState: OncMojo.DeviceStateProperties|undefined,
      globalPolicy: GlobalPolicy): boolean {
    if (!deviceState || deviceState.type !== NetworkType.kWiFi) {
      return false;
    }
    return this.allowAddConnection_(deviceState, globalPolicy);
  }

  private dispatchShowConfigEvent_(type: string): void {
    const event = new CustomEvent('show-config', {
      bubbles: true,
      composed: true,
      detail: {type},
    });
    this.dispatchEvent(event);
  }

  private onAddWifiButtonClick_(): void {
    assert(this.deviceState, 'Device state is falsey - Wifi expected.');
    const type = this.deviceState.type;
    assert(type === NetworkType.kWiFi, 'Wifi type expected.');
    this.dispatchShowConfigEvent_(OncMojo.getNetworkTypeString(type));
  }

  private onAddVpnButtonClick_(): void {
    assert(this.deviceState, 'Device state is falsey - VPN expected.');
    const type = this.deviceState.type;
    assert(type === NetworkType.kVPN, 'VPN type expected.');
    this.dispatchShowConfigEvent_(OncMojo.getNetworkTypeString(type));
  }

  private onAddThirdPartyVpnClick_(event: DomRepeatEvent<VpnProvider>): void {
    const provider = event.model.item;
    this.browserProxy_.addThirdPartyVpn(provider.appId);
    // TODO(b/282233232) recordSettingChange() for adding third party VPN.
  }

  private knownNetworksIsVisible_(deviceState: OncMojo.DeviceStateProperties|
                                  undefined): boolean {
    return !!deviceState && deviceState.type === NetworkType.kWiFi;
  }

  /**
   * Event triggered when the known networks button is clicked.
   */
  private onKnownNetworksClick_(): void {
    assert(this.deviceState?.type === NetworkType.kWiFi);
    const showKnownNetworksEvent = new CustomEvent('show-known-networks', {
      bubbles: true,
      composed: true,
      detail: this.deviceState.type,
    });
    this.dispatchEvent(showKnownNetworksEvent);
  }

  /**
   * Event triggered when the enable button is toggled.
   */
  private onDeviceEnabledChange_(): void {
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

  private getThirdPartyVpnNetworks_(
      thirdPartyVpns: Record<string, OncMojo.NetworkStateProperties[]>,
      provider: VpnProvider): OncMojo.NetworkStateProperties[] {
    return thirdPartyVpns[provider.providerId] || [];
  }

  private haveThirdPartyVpnNetwork_(
      thirdPartyVpns: Record<string, OncMojo.NetworkStateProperties[]>,
      provider: VpnProvider): boolean {
    const list = this.getThirdPartyVpnNetworks_(thirdPartyVpns, provider);
    return !!list.length;
  }

  /**
   * Event triggered when a network list item is selected.
   */
  private onNetworkSelected_(e: CustomEvent<OncMojo.NetworkStateProperties>):
      void {
    assert(this.globalPolicy);
    assert(this.defaultNetwork !== undefined);
    const networkState = e.detail;
    (e.target as HTMLElement).blur();
    if (this.canAttemptConnection_(networkState)) {
      const networkConnectEvent = new CustomEvent('network-connect', {
        bubbles: true,
        composed: true,
        detail: {networkState},
      });
      this.dispatchEvent(networkConnectEvent);
      // TODO(b/282233232) recordSettingChange() for connecting to network.
      return;
    }

    const showDetailEvent = new CustomEvent('show-detail', {
      bubbles: true,
      composed: true,
      detail: networkState,
    });
    this.dispatchEvent(showDetailEvent);
  }

  private isBlockedByPolicy_(state: OncMojo.NetworkStateProperties): boolean {
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
             state.typeState.wifi!.hexSsid));
  }

  /**
   * Determines whether or not it is possible to attempt a connection to the
   * provided network (e.g., whether it's possible to connect or configure the
   * network for connection).
   */
  private canAttemptConnection_(state: OncMojo.NetworkStateProperties):
      boolean {
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
        state.typeState.cellular!.simLocked) {
      return false;
    }

    return true;
  }

  private matchesType_(
      typeString: string, device: OncMojo.DeviceStateProperties): boolean {
    return !!device &&
        device.type === OncMojo.getNetworkTypeFromString(typeString);
  }

  private shouldShowNetworkList_(
      networkStateList: OncMojo.NetworkStateProperties[]): boolean {
    if (this.shouldShowCellularNetworkList_()) {
      return false;
    }

    if (!!this.deviceState && this.deviceState.type === NetworkType.kVPN) {
      return this.shouldShowVpnList_();
    }
    return networkStateList.length > 0;
  }

  /**
   * @return True if native VPN is not disabled by policy and there
   *     are more than one VPN network configured.
   */
  private shouldShowVpnList_(): boolean {
    return this.vpnIsEnabled_ && this.networkStateList_.length > 0;
  }

  private shouldShowCellularNetworkList_(): boolean {
    // Only shown if the currently-active subpage is for Cellular networks.
    return !!this.deviceState &&
        this.deviceState.type === NetworkType.kCellular;
  }

  private shouldShowBluetoothDisabledTetherErrorMessage_(
      deviceState: OncMojo.DeviceStateProperties|undefined): boolean {
    return this.isInstantHotspotRebrandEnabled_ && !!deviceState &&
        deviceState.type === NetworkType.kTether &&
        deviceState.deviceState === DeviceStateType.kUninitialized;
  }

  private hideNoNetworksMessage_(
      networkStateList: OncMojo.NetworkStateProperties[]): boolean {
    return this.shouldShowCellularNetworkList_() ||
        this.shouldShowNetworkList_(networkStateList);
  }

  private getNoNetworksInnerHtml_(
      deviceState: OncMojo.DeviceStateProperties,
      _tetherDeviceState: OncMojo.DeviceStateProperties|undefined): string {
    const type = deviceState.type;
    if (type === NetworkType.kTether && this.isInstantHotspotRebrandEnabled_) {
      return this.i18n('internetNoTetherHosts');
    }

    if (!this.isInstantHotspotRebrandEnabled_ &&
        (type === NetworkType.kCellular && this.tetherDeviceState ||
         type === NetworkType.kTether)) {
      return this.i18nAdvanced('internetNoNetworksMobileData').toString();
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

  private getBluetoothDisabledErrorMessageForTether_(): string {
    return this.i18n('tetherEnableBluetooth');
  }

  private showGmsCoreNotificationsSection_(notificationsDisabledDeviceNames:
                                               string[]): boolean {
    return notificationsDisabledDeviceNames.length > 0;
  }

  private getGmsCoreNotificationsDevicesString_(
      notificationsDisabledDeviceNames: string[]): string {
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

  private computeIsShowingVpn_(): boolean {
    if (!this.deviceState) {
      return false;
    }
    return this.matchesType_(
        OncMojo.getNetworkTypeString(NetworkType.kVPN), this.deviceState);
  }

  private computeIsShowingTether_(): boolean {
    return !!this.deviceState &&
        this.matchesType_(
            OncMojo.getNetworkTypeString(NetworkType.kTether),
            this.deviceState);
  }

  /**
   * Tells when VPN preferences section should be displayed. It is
   * displayed when the preferences are applicable to the current device.
   */
  private shouldShowVpnPreferences_(): boolean {
    if (!this.deviceState) {
      return false;
    }
    // For now the section only contain always-on VPN settings. It should not be
    // displayed on managed devices while the legacy always-on VPN based on ARC
    // is not replaced/extended by the new implementation.
    return !this.isManaged_ && this.isShowingVpn_;
  }

  /**
   * Tells whether the Tether notification control should be displayed. It is
   * displayed when instant-hotspot-rebrand is enabled and there are Tether
   * networks.
   */
  private shouldShowTetherNotificationControl_(
      deviceState: OncMojo.DeviceStateProperties|undefined): boolean {
    return !!deviceState && deviceState.type === NetworkType.kTether &&
        this.isInstantHotspotRebrandEnabled_;
  }

  /*
   * Says whether header for the Tether network list should be displayed.
   * Returns true if the rebrand is enabled and the device state is Tether
   */
  private shouldShowTetherDeviceListHeader_(deviceState:
                                                OncMojo.DeviceStateProperties|
                                            undefined): boolean {
    return !!deviceState && deviceState.type === NetworkType.kTether &&
        this.isInstantHotspotRebrandEnabled_;
  }

  /**
   * Generates the list of VPN services available for always-on. It keeps from
   * the network list only the supported technologies.
   */
  private getAlwaysOnVpnNetworks_(): OncMojo.NetworkStateProperties[] {
    if (!this.deviceState || this.deviceState.type !== NetworkType.kVPN) {
      return [];
    }

    const alwaysOnVpnList = this.networkStateList_.slice();
    for (const vpnList of Object.values(this.thirdPartyVpns_)) {
      assert(vpnList.length > 0);
      // Exclude incompatible VPN technologies:
      // - TODO(b/188864779): ARC VPNs are not supported yet,
      // - Chrome VPN apps are deprecated and incompatible with lockdown mode
      //   (see b/206910855).
      if (vpnList[0].typeState.vpn!.type === VpnType.kArc ||
          vpnList[0].typeState.vpn!.type === VpnType.kExtension) {
        continue;
      }
      alwaysOnVpnList.push(...vpnList);
    }

    return alwaysOnVpnList;
  }

  /**
   * Fetches the always-on VPN configuration from network config.
   */
  private async updateAlwaysOnVpnPreferences_(): Promise<void> {
    if (!this.deviceState || this.deviceState.type !== NetworkType.kVPN) {
      return;
    }

    const result = await this.networkConfig_.getAlwaysOnVpn();
    this.alwaysOnVpnMode_ = result.properties.mode;
    this.alwaysOnVpnService_ = result.properties.serviceGuid;
  }

  /**
   * Handles a change in |alwaysOnVpnMode_| or |alwaysOnVpnService_|
   * triggered via the observer.
   */
  private onAlwaysOnVpnChanged_(): void {
    if (this.alwaysOnVpnMode_ === undefined ||
        this.alwaysOnVpnService_ === undefined) {
      return;
    }

    const properties: AlwaysOnVpnProperties = {
      mode: this.alwaysOnVpnMode_,
      serviceGuid: this.alwaysOnVpnService_,
    };
    this.networkConfig_.setAlwaysOnVpn(properties);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [SettingsInternetSubpageElement.is]: SettingsInternetSubpageElement;
  }
}

customElements.define(
    SettingsInternetSubpageElement.is, SettingsInternetSubpageElement);
