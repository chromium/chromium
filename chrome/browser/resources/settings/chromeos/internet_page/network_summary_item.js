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
import 'chrome://resources/ash/common/network/network_siminfo.js';
import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';
import 'chrome://resources/cr_elements/cr_toggle/cr_toggle.js';
import 'chrome://resources/cr_elements/cr_shared_vars.css.js';
import 'chrome://resources/polymer/v3_0/iron-flex-layout/iron-flex-layout-classes.js';

import {getSimSlotCount} from 'chrome://resources/ash/common/network/cellular_utils.js';
import {CrPolicyNetworkBehaviorMojo, CrPolicyNetworkBehaviorMojoInterface} from 'chrome://resources/ash/common/network/cr_policy_network_behavior_mojo.js';
import {OncMojo} from 'chrome://resources/ash/common/network/onc_mojo.js';
import {I18nBehavior, I18nBehaviorInterface} from 'chrome://resources/ash/common/i18n_behavior.js';
import {CrPolicyIndicatorType} from 'chrome://resources/ash/common/cr_policy_indicator_behavior.js';
import {assert, assertNotReached} from 'chrome://resources/ash/common/assert.js';
import {loadTimeData} from 'chrome://resources/ash/common/load_time_data.m.js';
import {GlobalPolicy, VpnType} from 'chrome://resources/mojo/chromeos/services/network_config/public/mojom/cros_network_config.mojom-webui.js';
import {ConnectionStateType, DeviceStateType, NetworkType, OncSource, PortalState} from 'chrome://resources/mojo/chromeos/services/network_config/public/mojom/network_types.mojom-webui.js';
import {html, mixinBehaviors, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {InternetPageBrowserProxy, InternetPageBrowserProxyImpl} from './internet_page_browser_proxy.js';

/**
 * @constructor
 * @extends {PolymerElement}
 * @implements {CrPolicyNetworkBehaviorMojoInterface}
 * @implements {I18nBehaviorInterface}
 */
const NetworkSummaryItemElementBase =
    mixinBehaviors([CrPolicyNetworkBehaviorMojo, I18nBehavior], PolymerElement);

/** @polymer */
export class NetworkSummaryItemElement extends NetworkSummaryItemElementBase {
  static get is() {
    return 'network-summary-item';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
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
        },
      },

      /**
       * Return true if captivePortalUI2022 feature flag is enabled.
       * @private
       */
      isCaptivePortalUI2022Enabled_: {
        type: Boolean,
        value() {
          return loadTimeData.valueExists('captivePortalUI2022') &&
              loadTimeData.getBoolean('captivePortalUI2022');
        },
      },

      /** @private {!GlobalPolicy|undefined} */
      globalPolicy: Object,
    };
  }

  constructor() {
    super();

    /** @private  {!InternetPageBrowserProxy} */
    this.browserProxy_ = InternetPageBrowserProxyImpl.getInstance();
  }

  /*
   * Returns the device enabled toggle element.
   * @return {?CrToggleElement}
   */
  getDeviceEnabledToggle() {
    return this.shadowRoot.querySelector('#deviceEnabledButton');
  }

  /**
   * @return {string}
   * @private
   */
  getNetworkStateText_() {
    // If SIM Locked, show warning message instead of connection state.
    if (this.shouldShowLockedWarningMessage_(this.deviceState)) {
      return this.i18n('networkSimLockedSubtitle');
    }
    if (OncMojo.deviceIsInhibited(this.deviceState)) {
      return this.i18n('internetDeviceBusy');
    }

    if (this.isCaptivePortalUI2022Enabled_ &&
        this.isPortalState_(this.activeNetworkState.portalState)) {
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
        return this.i18n('internetDeviceEnabling');
      }
    }
    // No device or unknown device state, use 'off'.
    return this.i18n('deviceOff');
  }

  /**
   * @param {!OncMojo.NetworkStateProperties|undefined} networkState
   * @return {string}
   * @private
   */
  getConnectionStateText_(networkState) {
    if (!networkState || !networkState.guid) {
      return '';
    }
    const connectionState = networkState.connectionState;
    const name = OncMojo.getNetworkStateDisplayName(networkState);
    if (OncMojo.connectionStateIsConnected(connectionState)) {
      // Ethernet networks always have the display name 'Ethernet' so we use the
      // state text 'Connected' to avoid repeating the label in the sublabel.
      // See http://crbug.com/989907 for details.
      return networkState.type === NetworkType.kEthernet ?
          this.i18n('networkListItemConnected') :
          name;
    }
    if (connectionState === ConnectionStateType.kConnecting) {
      return name ? this.i18n('networkListItemConnectingTo', name) :
                    this.i18n('networkListItemConnecting');
    }
    return this.i18n('networkListItemNotConnected');
  }

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
  }

  /**
   * @param {!OncMojo.NetworkStateProperties} activeNetworkState
   * @return {!CrPolicyIndicatorType} Device policy indicator for VPN when
   *     disabled by policy and an indicator corresponding to the source of the
   *     active network state otherwise.
   * @private
   */
  getPolicyIndicatorType_(activeNetworkState) {
    if (this.isProhibitedVpn_()) {
      return this.getIndicatorTypeForSource(OncSource.kDevicePolicy);
    }
    return this.getIndicatorTypeForSource(activeNetworkState.source);
  }

  /**
   * @param {!OncMojo.DeviceStateProperties|undefined} deviceState
   * @return {boolean}
   * @private
   */
  showSimInfo_(deviceState) {
    if (!deviceState || deviceState.type !== NetworkType.kCellular) {
      return false;
    }

    const {pSimSlots, eSimSlots} = getSimSlotCount(deviceState);
    if (eSimSlots > 0) {
      // Do not show simInfo if we are using an eSIM enabled device.
      return false;
    }
    return this.simLocked_(deviceState);
  }

  /**
   * @param {!OncMojo.NetworkStateProperties|undefined} activeNetworkState
   * @param {!OncMojo.DeviceStateProperties|undefined} deviceState
   * @return {string}
   * @private
   */
  getNetworkStateClass_(activeNetworkState, deviceState) {
    if ((this.isCaptivePortalUI2022Enabled_ &&
         this.isPortalState_(activeNetworkState.portalState)) ||
        this.shouldShowLockedWarningMessage_(deviceState)) {
      return 'warning-message';
    }
    return 'network-state';
  }

  /**
   * @param {!OncMojo.DeviceStateProperties|undefined} deviceState
   * @return {boolean}
   * @private
   */
  shouldShowLockedWarningMessage_(deviceState) {
    if (!deviceState || deviceState.type !== NetworkType.kCellular ||
        !deviceState.simLockStatus) {
      return false;
    }

    // If the device is eSIM capable, never show message.
    const {eSimSlots} = getSimSlotCount(deviceState);
    if (eSimSlots > 0) {
      return false;
    }

    return !!deviceState.simLockStatus.lockType;
  }

  /**
   * @param {!OncMojo.DeviceStateProperties|undefined} deviceState
   * @return {boolean}
   * @private
   */
  simLocked_(deviceState) {
    if (!deviceState) {
      return false;
    }
    if (!deviceState.simLockStatus) {
      return false;
    }
    const simLockType = deviceState.simLockStatus.lockType;
    return simLockType === 'sim-pin' || simLockType === 'sim-puk';
  }

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
        (deviceState.type === NetworkType.kVPN ||
         deviceState.deviceState === DeviceStateType.kEnabled ||
         OncMojo.deviceIsInhibited(deviceState));
  }

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
      case NetworkType.kEthernet:
      case NetworkType.kVPN:
        return false;

      case NetworkType.kTether:
        return true;

      case NetworkType.kWiFi:
        return deviceState.deviceState !== DeviceStateType.kUninitialized;

      case NetworkType.kCellular:
        if (deviceState.deviceState === DeviceStateType.kUninitialized) {
          return false;
        }

        // Toggle should be shown as long as we are not also showing the UI for
        // unlocking the SIM.
        return !this.showSimInfo_(deviceState);
    }
    assertNotReached();
    return false;
  }

  /**
   * @param {!OncMojo.DeviceStateProperties|undefined} deviceState
   * @return {boolean}
   * @private
   */
  enableToggleIsEnabled_(deviceState) {
    return this.enableToggleIsVisible_(deviceState) &&
        deviceState.deviceState !== DeviceStateType.kProhibited &&
        !OncMojo.deviceIsInhibited(deviceState) &&
        !OncMojo.deviceStateIsIntermediate(deviceState.deviceState);
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
   * @param {!OncMojo.DeviceStateProperties|undefined} deviceState
   * @return {string}
   * @private
   */
  getToggleA11yDescribedBy_(deviceState) {
    // Use network state text to describe toggle for uninitialized tether
    // device. This announces details about enabling bluetooth.
    if (this.enableToggleIsVisible_(deviceState) &&
        deviceState.type === NetworkType.kTether &&
        deviceState.deviceState === DeviceStateType.kUninitialized) {
      return 'networkState';
    }
    return '';
  }

  /**
   * @return {boolean} True if VPNs are disabled by policy and the current
   *     device is VPN.
   * @private
   */
  isProhibitedVpn_() {
    return !!this.deviceState && this.deviceState.type === NetworkType.kVPN &&
        this.builtInVpnProhibited_(this.deviceState);
  }

  /**
   * @param {!VpnType} vpnType
   * @return {boolean}
   * @private
   */
  isBuiltInVpnType_(vpnType) {
    return vpnType === VpnType.kL2TPIPsec || vpnType === VpnType.kOpenVPN;
  }

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
  }

  /**
   * @param {!OncMojo.DeviceStateProperties|undefined} deviceState
   * @return {boolean} True if the built-in VPNs are disabled by policy.
   * @private
   */
  builtInVpnProhibited_(deviceState) {
    return !!deviceState &&
        deviceState.deviceState === DeviceStateType.kProhibited;
  }

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
  }

  /**
   * @param {!OncMojo.NetworkStateProperties|undefined} activeNetworkState
   * @param {!OncMojo.DeviceStateProperties|undefined} deviceState
   * @param {!Array<!OncMojo.NetworkStateProperties>} networkStateList
   * @return {boolean}
   * @private
   */
  shouldShowDetails_(activeNetworkState, deviceState, networkStateList) {
    if (!!deviceState && deviceState.type === NetworkType.kVPN) {
      return this.anyVpnExists_(deviceState, networkStateList);
    }

    return this.deviceIsEnabled_(deviceState) &&
        (!!activeNetworkState.guid || networkStateList.length > 0);
  }

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

    let minlen;
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
   * @param {!Event} event The enable button event.
   * @private
   */
  onShowDetailsTap_(event) {
    if (!this.deviceIsEnabled_(this.deviceState)) {
      if (this.enableToggleIsEnabled_(this.deviceState)) {
        const type = this.deviceState.type;
        const deviceEnabledToggledEvent =
            new CustomEvent('device-enabled-toggled', {
              bubbles: true,
              composed: true,
              detail: {enabled: true, type: type},
            });
        this.dispatchEvent(deviceEnabledToggledEvent);
      }
    } else if (
        this.isCaptivePortalUI2022Enabled_ &&
        this.isPortalState_(this.activeNetworkState.portalState)) {
      this.browserProxy_.showPortalSignin(this.activeNetworkState.guid);
    } else if (this.shouldShowSubpage_(
                   this.deviceState, this.networkStateList)) {
      const showNetworksEvent = new CustomEvent('show-networks', {
        bubbles: true,
        composed: true,
        detail: this.deviceState.type,
      });
      this.dispatchEvent(showNetworksEvent);
    } else if (this.shouldShowDetails_(
                   this.activeNetworkState, this.deviceState,
                   this.networkStateList)) {
      if (this.activeNetworkState.guid) {
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
   * function and `onShowDetailsTap_`.
   * @param {!Event} event The enable button event.
   * @private
   */
  onShowDetailsArrowTap_(event) {
    if (this.shouldShowSubpage_(this.deviceState, this.networkStateList)) {
      const showNetworksEvent = new CustomEvent('show-networks', {
        bubbles: true,
        composed: true,
        detail: this.deviceState.type,
      });
      this.dispatchEvent(showNetworksEvent);
    } else if (this.shouldShowDetails_(
                   this.activeNetworkState, this.deviceState,
                   this.networkStateList)) {
      if (this.activeNetworkState.guid) {
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

    // Item is actionable if tapping should show the user to the portal signin.
    if (this.isCaptivePortalUI2022Enabled_ &&
        this.isPortalState_(this.activeNetworkState.portalState)) {
      return true;
    }

    // Item is actionable if tapping should show either networks subpage or the
    // network details page.
    return this.shouldShowSubpage_(this.deviceState, this.networkStateList) ||
        this.shouldShowDetails_(
            activeNetworkState, deviceState, networkStateList);
  }

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
  }

  /**
   * Event triggered when the enable button is toggled.
   * @param {!Event} event
   * @private
   */
  onDeviceEnabledChange_(event) {
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

  /**
   * @return {string}
   * @private
   */
  getTitleText_() {
    if (this.networkTitleText) {
      return this.networkTitleText;
    }
    if (this.isCaptivePortalUI2022Enabled_ &&
        this.isPortalState_(this.activeNetworkState.portalState)) {
      const stateText = this.getConnectionStateText_(this.activeNetworkState);
      if (stateText) {
        return stateText;
      }
    }
    return this.getNetworkTypeString_(this.activeNetworkState.type);
  }

  /**
   * Make sure events in embedded components do not propagate to onDetailsTap_.
   * @param {!Event} event
   * @private
   */
  doNothing_(event) {
    event.stopPropagation();
  }

  /**
   * @param {!NetworkType} type
   * @return {string}
   * @private
   */
  getNetworkTypeString_(type) {
    // The shared Cellular/Tether subpage is referred to as "Mobile".
    // TODO(khorimoto): Remove once Cellular/Tether are split into their own
    // sections.
    if (type === NetworkType.kCellular || type === NetworkType.kTether) {
      type = NetworkType.kMobile;
    }
    return this.i18n('OncType' + OncMojo.getNetworkTypeString(type));
  }

  /**
   * Return true if portalState is either kPortal or kProxyAuthRequired.
   * @param {!PortalState} portalState
   * @return {boolean}
   * @private
   */
  isPortalState_(portalState) {
    return portalState === PortalState.kPortal ||
        portalState === PortalState.kProxyAuthRequired;
  }
}

customElements.define(NetworkSummaryItemElement.is, NetworkSummaryItemElement);
