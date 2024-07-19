// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Polymer element for displaying the network state for a specific
 * type and a list of networks for that type. NOTE: It both Cellular and Tether
 * technologies are available, they are combined into a single 'Mobile data'
 * section. See crbug.com/726380.
 */

import 'chrome://resources/ash/common/network/network_icon.js';
import 'chrome://resources/ash/common/cr_elements/cr_icon_button/cr_icon_button.js';
import 'chrome://resources/ash/common/cr_elements/cr_toggle/cr_toggle.js';
import 'chrome://resources/ash/common/cr_elements/cr_shared_vars.css.js';
import 'chrome://resources/polymer/v3_0/iron-flex-layout/iron-flex-layout-classes.js';

import {getSimSlotCount} from 'chrome://resources/ash/common/network/cellular_utils.js';
import {CrPolicyNetworkBehaviorMojo, CrPolicyNetworkBehaviorMojoInterface} from 'chrome://resources/ash/common/network/cr_policy_network_behavior_mojo.js';
import {OncMojo} from 'chrome://resources/ash/common/network/onc_mojo.js';
import {CrToggleElement} from 'chrome://resources/ash/common/cr_elements/cr_toggle/cr_toggle.js';
import {I18nMixin, I18nMixinInterface} from 'chrome://resources/ash/common/cr_elements/i18n_mixin.js';
import {CrPolicyIndicatorType} from 'chrome://resources/ash/common/cr_elements/policy/cr_policy_indicator_mixin.js';
import {assert, assertNotReached} from 'chrome://resources/js/assert.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {GlobalPolicy, VpnType} from 'chrome://resources/mojo/chromeos/services/network_config/public/mojom/cros_network_config.mojom-webui.js';
import {ConnectionStateType, DeviceStateType, NetworkType, OncSource, PortalState} from 'chrome://resources/mojo/chromeos/services/network_config/public/mojom/network_types.mojom-webui.js';
import {mixinBehaviors, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {Constructor} from '../common/types.js';

import {InternetPageBrowserProxy, InternetPageBrowserProxyImpl} from './internet_page_browser_proxy.js';
import {getTemplate} from './network_summary_item.html.js';

const NetworkSummaryItemElementBase =
    mixinBehaviors([CrPolicyNetworkBehaviorMojo], I18nMixin(PolymerElement)) as
    Constructor<PolymerElement&I18nMixinInterface&
                CrPolicyNetworkBehaviorMojoInterface>;

export class NetworkSummaryItemElement extends NetworkSummaryItemElementBase {
  static get is() {
    return 'network-summary-item' as const;
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      /**
       * Device state for the network type. This might briefly be undefined if
       * a device becomes unavailable.
       */
      deviceState: {
        type: Object,
        notify: true,
      },

      /**
       * If both Cellular and Tether technologies exist, we combine the
       * sections and set this to the device state for Tether.
       */
      tetherDeviceState: Object,

      /**
       * Network state for the active network.
       */
      activeNetworkState: Object,

      /**
       * List of all network state data for the network type.
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
       */
      networkTitleText: String,

      /**
       * Whether to show technology badge on mobile network icon.
       */
      showTechnologyBadge_: {
        type: Boolean,
        value() {
          return loadTimeData.valueExists('showTechnologyBadge') &&
              loadTimeData.getBoolean('showTechnologyBadge');
        },
      },

      globalPolicy: Object,
    };
  }

  activeNetworkState: OncMojo.NetworkStateProperties|undefined;
  deviceState: OncMojo.DeviceStateProperties|undefined;
  globalPolicy: GlobalPolicy|undefined;
  networkStateList: OncMojo.NetworkStateProperties[];
  networkTitleText: string|undefined;
  tetherDeviceState: OncMojo.DeviceStateProperties|undefined;
  private browserProxy_: InternetPageBrowserProxy;
  private showTechnologyBadge_: boolean;

  constructor() {
    super();

    this.browserProxy_ = InternetPageBrowserProxyImpl.getInstance();
  }

  getDeviceEnabledToggle(): CrToggleElement|null {
    return this.shadowRoot!.querySelector<CrToggleElement>(
        '#deviceEnabledButton');
  }

  private getNetworkStateText_(): string {
    if (OncMojo.deviceIsInhibited(this.deviceState)) {
      return this.i18n('internetDeviceBusy');
    }

    if (OncMojo.deviceIsFlashing(this.deviceState)) {
      return this.i18n('internetDeviceFlashing');
    }

    if (this.isPortalState_(this.activeNetworkState!.portalState)) {
      if (this.deviceState && this.deviceState.type === NetworkType.kCellular) {
        return this.i18n('networkListItemCellularSignIn');
      }
      return this.i18n('networkListItemSignIn');
    }

    const stateText = this.getConnectionStateText_(this.activeNetworkState);
    if (stateText) {
      return stateText;
    }
    // No network state, use device state.
    const deviceState = this.deviceState;
    if (deviceState) {
      if (deviceState.type === NetworkType.kTether) {
        if (deviceState.deviceState === DeviceStateType.kUninitialized) {
          return this.i18n('tetherEnableBluetooth');
        }
      }

      // Enabled or enabling states.
      if (deviceState.deviceState === DeviceStateType.kEnabled) {
        return this.networkStateList.length > 0 ?
            this.i18n('networkListItemNotConnected') :
            this.i18n('networkListItemNoNetwork');
      }

      if (deviceState.deviceState === DeviceStateType.kEnabling) {
        return this.i18n('networkDeviceTurningOn');
      }
    }
    // No device or unknown device state, use 'off'.
    return this.i18n('deviceOff');
  }

  private getConnectionStateText_(networkState: OncMojo.NetworkStateProperties|
                                  undefined): string {
    if (!networkState || !networkState.guid) {
      return '';
    }
    const connectionState = networkState.connectionState;
    const name = OncMojo.getNetworkStateDisplayNameUnsafe(networkState);
    if (OncMojo.connectionStateIsConnected(connectionState)) {
      // Ethernet networks always have the display name 'Ethernet' so we use the
      // state text 'Connected' to avoid repeating the label in the sublabel.
      // See http://crbug.com/989907 for details.
      return networkState.type === NetworkType.kEthernet ?
          this.i18n('networkListItemConnected') :
          name;
    }
    if (connectionState === ConnectionStateType.kConnecting) {
      return name ?
          loadTimeData.getStringF('networkListItemConnectingTo', name) :
          this.i18n('networkListItemConnecting');
    }
    return this.i18n('networkListItemNotConnected');
  }

  private showPolicyIndicator_(activeNetworkState:
                                   OncMojo.NetworkStateProperties): boolean {
    return (activeNetworkState !== undefined &&
            OncMojo.connectionStateIsConnected(
                activeNetworkState.connectionState)) ||
        this.isPolicySource(activeNetworkState.source) ||
        this.isProhibitedVpn_();
  }

  /**
   * @return Device policy indicator for VPN when
   *     disabled by policy and an indicator corresponding to the source of the
   *     active network state otherwise.
   */
  private getPolicyIndicatorType_(activeNetworkState:
                                      OncMojo.NetworkStateProperties):
      CrPolicyIndicatorType {
    if (this.isProhibitedVpn_()) {
      return this.getIndicatorTypeForSource(OncSource.kDevicePolicy);
    }
    return this.getIndicatorTypeForSource(activeNetworkState.source);
  }

  private getNetworkStateClass_(activeNetworkState:
                                    OncMojo.NetworkStateProperties|
                                undefined): string {
    if ((this.isPortalState_(activeNetworkState!.portalState))) {
      return 'warning-message';
    }
    return 'network-state';
  }

  /**
   * @return True if the device is enabled or if it is a VPN or if
   *     we are in the state of inhibited. Note:
   *     This function will always return true for VPNs because VPNs can be
   *     disabled by policy only for built-in VPNs (OpenVPN & L2TP), but always
   *     enabled for other VPN providers. To know whether built-in VPNs are
   *     disabled, use builtInVpnProhibited_() instead.
   */
  private deviceIsEnabled_(deviceState: OncMojo.DeviceStateProperties|
                           undefined): boolean {
    if (!deviceState) {
      return false;
    }

    if (this.isInstantHotspotRebrandEnabled_() &&
        deviceState.type === NetworkType.kTether) {
      return true;
    }
    if (deviceState.type === NetworkType.kVPN) {
      return true;
    }
    if (deviceState.deviceState === DeviceStateType.kEnabled) {
      return true;
    }
    if (OncMojo.deviceIsFlashing(deviceState)) {
      return false;
    }

    return OncMojo.deviceIsInhibited(deviceState);
  }

  /**
   * @return True if the device state is enabling.
   */
  private deviceIsEnabling_(deviceState: OncMojo.DeviceStateProperties|
                            undefined): boolean {
    return !!deviceState &&
        deviceState.deviceState === DeviceStateType.kEnabling;
  }

  private deviceIsEnabledOrEnabling_(deviceState: OncMojo.DeviceStateProperties|
                                     undefined): boolean {
    return this.deviceIsEnabled_(deviceState) ||
        this.deviceIsEnabling_(deviceState);
  }

  private enableToggleIsVisible_(deviceState: OncMojo.DeviceStateProperties|
                                 undefined): boolean {
    if (!deviceState) {
      return false;
    }
    switch (deviceState.type) {
      case NetworkType.kEthernet:
      case NetworkType.kVPN:
        return false;
      case NetworkType.kTether:
        return !this.isInstantHotspotRebrandEnabled_();
      case NetworkType.kWiFi:
      case NetworkType.kCellular:
        return deviceState.deviceState !== DeviceStateType.kUninitialized;
    }
    assertNotReached();
  }

  private enableToggleIsEnabled_(deviceState: OncMojo.DeviceStateProperties|
                                 undefined): boolean {
    return this.enableToggleIsVisible_(deviceState) &&
        deviceState!.deviceState !== DeviceStateType.kProhibited &&
        !OncMojo.deviceIsInhibited(deviceState) &&
        !OncMojo.deviceStateIsIntermediate(deviceState!.deviceState) &&
        !OncMojo.deviceIsFlashing(deviceState);
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

  private getToggleA11yDescribedBy_(deviceState: OncMojo.DeviceStateProperties|
                                    undefined): string {
    // Use network state text to describe toggle for uninitialized tether
    // device. This announces details about enabling bluetooth.
    if (this.enableToggleIsVisible_(deviceState) &&
        deviceState!.type === NetworkType.kTether &&
        deviceState!.deviceState === DeviceStateType.kUninitialized) {
      return 'networkState';
    }
    return '';
  }

  /**
   * @return True if instant hotspot rebrand feature flag is enabled.
   */
  private isInstantHotspotRebrandEnabled_(): boolean {
    return loadTimeData.valueExists('isInstantHotspotRebrandEnabled') &&
        loadTimeData.getBoolean('isInstantHotspotRebrandEnabled');
  }

  /**
   * @return True if VPNs are disabled by policy and the current device is VPN.
   */
  private isProhibitedVpn_(): boolean {
    return !!this.deviceState && this.deviceState.type === NetworkType.kVPN &&
        this.builtInVpnProhibited_(this.deviceState);
  }

  private isBuiltInVpnType_(vpnType: VpnType): boolean {
    return vpnType === VpnType.kL2TPIPsec || vpnType === VpnType.kOpenVPN;
  }

  /**
   * @return True if at least one non-native VPN is configured.
   */
  private hasNonBuiltInVpn_(networkStateList: OncMojo.NetworkStateProperties[]):
      boolean {
    const nonBuiltInVpnIndex = networkStateList.findIndex((networkState) => {
      return !this.isBuiltInVpnType_(networkState.typeState.vpn!.type);
    });
    return nonBuiltInVpnIndex !== -1;
  }

  /**
   * @return True if the built-in VPNs are disabled by policy.
   */
  private builtInVpnProhibited_(deviceState: OncMojo.DeviceStateProperties|
                                undefined): boolean {
    return !!deviceState &&
        deviceState.deviceState === DeviceStateType.kProhibited;
  }

  /**
   * @return True if there is any configured VPN for a non-disabled
   *     VPN provider. Note: Only built-in VPN providers can be disabled by
   *     policy at the moment.
   */
  private anyVpnExists_(
      deviceState: OncMojo.DeviceStateProperties|undefined,
      networkStateList: OncMojo.NetworkStateProperties[]): boolean {
    return this.hasNonBuiltInVpn_(networkStateList) ||
        (!this.builtInVpnProhibited_(deviceState) &&
         networkStateList.length > 0);
  }

  private shouldShowDetails_(
      activeNetworkState: OncMojo.NetworkStateProperties|undefined,
      deviceState: OncMojo.DeviceStateProperties|undefined,
      networkStateList: OncMojo.NetworkStateProperties[]): boolean {
    if (!!deviceState && deviceState.type === NetworkType.kVPN) {
      return this.anyVpnExists_(deviceState, networkStateList);
    }

    return this.deviceIsEnabled_(deviceState) &&
        (!!activeNetworkState!.guid || networkStateList.length > 0);
  }

  private shouldShowSubpage_(
      deviceState: OncMojo.DeviceStateProperties|undefined,
      networkStateList: OncMojo.NetworkStateProperties[]): boolean {
    if (!deviceState) {
      return false;
    }
    const type = deviceState.type;

    if (type === NetworkType.kTether ||
        (type === NetworkType.kCellular && this.tetherDeviceState)) {
      // The "Mobile data" subpage should always be shown if Tether is
      // available, even if there are currently no associated networks.
      return true;
    }

    if (type === NetworkType.kCellular) {
      if (OncMojo.deviceIsInhibited(deviceState)) {
        // The "Mobile data" subpage should be shown if the device state is
        // inhibited.
        return true;
      }
      // When network type is Cellular, always show "Mobile data" subpage, when
      // at least one eSIM or pSIM slot is available
      const {pSimSlots, eSimSlots} = getSimSlotCount(deviceState);
      if (eSimSlots > 0 || pSimSlots > 0) {
        return true;
      }
    }

    if (type === NetworkType.kVPN) {
      return this.anyVpnExists_(deviceState, networkStateList);
    }

    let minlen: number;
    if (type === NetworkType.kWiFi) {
      // WiFi subpage includes 'Known Networks' so always show, even if the
      // technology is still enabling / scanning, or none are visible.
      minlen = 0;
    } else {
      // By default, only show the subpage if there are 2+ networks
      minlen = 2;
    }
    return networkStateList.length >= minlen;
  }

  /**
   * This handles clicking the network summary item row. Clicking this row can
   * lead to toggling device enablement or showing the corresponding networks
   * list or showing details about a network or doing nothing based on the
   * device and networks states.
   */
  private onShowDetailsClick_(event: Event): void {
    if (!this.deviceIsEnabled_(this.deviceState)) {
      if (this.enableToggleIsEnabled_(this.deviceState)) {
        const type = this.deviceState!.type;
        const deviceEnabledToggledEvent =
            new CustomEvent('device-enabled-toggled', {
              bubbles: true,
              composed: true,
              detail: {enabled: true, type: type},
            });
        this.dispatchEvent(deviceEnabledToggledEvent);
      }
    } else if (this.isPortalState_(this.activeNetworkState!.portalState)) {
      this.browserProxy_.showPortalSignin(this.activeNetworkState!.guid);
    } else if (this.shouldShowSubpage_(
                   this.deviceState, this.networkStateList)) {
      const showNetworksEvent = new CustomEvent('show-networks', {
        bubbles: true,
        composed: true,
        detail: this.deviceState!.type,
      });
      this.dispatchEvent(showNetworksEvent);
    } else if (this.shouldShowDetails_(
                   this.activeNetworkState, this.deviceState,
                   this.networkStateList)) {
      if (this.activeNetworkState!.guid) {
        const showDetailEvent = new CustomEvent('show-detail', {
          bubbles: true,
          composed: true,
          detail: this.activeNetworkState,
        });
        this.dispatchEvent(showDetailEvent);
      } else if (this.networkStateList.length > 0) {
        const showDetailEvent = new CustomEvent('show-detail', {
          bubbles: true,
          composed: true,
          detail: this.networkStateList[0],
        });
        this.dispatchEvent(showDetailEvent);
      }
    }
    event.stopPropagation();
  }

  /**
   * This handles clicking the subpage arrow. Clicking this icon can lead
   * to showing the corresponding networks list or showing details about
   * a network or doing nothing based on the device and networks states.
   * TODO(b/253326370) Cleanup duplicate functionality between this
   * function and `onShowDetailsClick_`.
   */
  private onShowDetailsArrowClick_(event: Event): void {
    if (this.shouldShowSubpage_(this.deviceState, this.networkStateList)) {
      const showNetworksEvent = new CustomEvent('show-networks', {
        bubbles: true,
        composed: true,
        detail: this.deviceState!.type,
      });
      this.dispatchEvent(showNetworksEvent);
    } else if (this.shouldShowDetails_(
                   this.activeNetworkState, this.deviceState,
                   this.networkStateList)) {
      if (this.activeNetworkState!.guid) {
        const showDetailEvent = new CustomEvent('show-detail', {
          bubbles: true,
          composed: true,
          detail: this.activeNetworkState,
        });
        this.dispatchEvent(showDetailEvent);
      } else if (this.networkStateList.length > 0) {
        const showDetailEvent = new CustomEvent('show-detail', {
          bubbles: true,
          composed: true,
          detail: this.networkStateList[0],
        });
        this.dispatchEvent(showDetailEvent);
      }
    }
    event.stopPropagation();
  }

  private isItemActionable_(
      activeNetworkState: OncMojo.NetworkStateProperties,
      deviceState: OncMojo.DeviceStateProperties|undefined,
      networkStateList: OncMojo.NetworkStateProperties[]): boolean {
    // The boolean logic here matches onShowDetailsClick_ method that handles the
    // item click event.

    if (!this.deviceIsEnabled_(this.deviceState)) {
      // When device is disabled, tapping the item flips the enable toggle. So
      // the item is actionable only when the toggle is enabled.
      return this.enableToggleIsEnabled_(this.deviceState);
    }

    // Item is actionable if tapping should show the user to the portal signin.
    if (this.isPortalState_(this.activeNetworkState!.portalState)) {
      return true;
    }

    // Item is actionable if tapping should show either networks subpage or the
    // network details page.
    return this.shouldShowSubpage_(this.deviceState, this.networkStateList) ||
        this.shouldShowDetails_(
            activeNetworkState, deviceState, networkStateList);
  }

  private showArrowButton_(
      activeNetworkState: OncMojo.NetworkStateProperties,
      deviceState: OncMojo.DeviceStateProperties|undefined,
      networkStateList: OncMojo.NetworkStateProperties[]): boolean {
    if (!this.deviceIsEnabled_(deviceState)) {
      return false;
    }
    return this.shouldShowSubpage_(deviceState, networkStateList) ||
        this.shouldShowDetails_(
            activeNetworkState, deviceState, networkStateList);
  }

  /**
   * Event triggered when the enable button is toggled.
   */
  private onDeviceEnabledChange_(): void {
    assert(this.deviceState);
    const deviceIsEnabled = this.deviceIsEnabled_(this.deviceState);
    const deviceEnabledToggledEvent =
        new CustomEvent('device-enabled-toggled', {
          bubbles: true,
          composed: true,
          detail: {enabled: !deviceIsEnabled, type: this.deviceState.type},
        });
    this.dispatchEvent(deviceEnabledToggledEvent);

    // Set the device state to enabling or disabling until updated.
    this.deviceState.deviceState = deviceIsEnabled ?
        DeviceStateType.kDisabling :
        DeviceStateType.kEnabling;
  }

  private getTitleText_(): string {
    if (this.networkTitleText) {
      return this.networkTitleText;
    }
    if (this.isPortalState_(this.activeNetworkState!.portalState)) {
      const stateText = this.getConnectionStateText_(this.activeNetworkState);
      if (stateText) {
        return stateText;
      }
    }
    return this.getNetworkTypeString_(this.activeNetworkState!.type);
  }

  /**
   * Make sure events in embedded components do not propagate to onDetailsClick_.
   */
  private doNothing_(event: Event): void {
    event.stopPropagation();
  }

  private getNetworkTypeString_(type: NetworkType): string {
    // The shared Cellular/Tether subpage is referred to as "Mobile".
    // TODO(khorimoto): Remove once Cellular/Tether are split into their own
    // sections.
    if (type === NetworkType.kCellular ||
        (type === NetworkType.kTether &&
         !this.isInstantHotspotRebrandEnabled_())) {
      type = NetworkType.kMobile;
    }
    return this.i18n('OncType' + OncMojo.getNetworkTypeString(type));
  }

  private isPortalState_(portalState: PortalState): boolean {
    return portalState === PortalState.kPortal ||
        portalState === PortalState.kPortalSuspected;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [NetworkSummaryItemElement.is]: NetworkSummaryItemElement;
  }
}

customElements.define(NetworkSummaryItemElement.is, NetworkSummaryItemElement);
