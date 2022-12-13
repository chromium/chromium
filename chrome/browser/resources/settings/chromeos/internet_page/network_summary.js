// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Polymer element for displaying a summary of network states
 * by type: Ethernet, WiFi, Cellular, and VPN.
 */

import './hotspot_summary_item.js';
import './network_summary_item.js';

import {getHotspotConfig} from 'chrome://resources/ash/common/hotspot/cros_hotspot_config.js';
import {MojoInterfaceProvider, MojoInterfaceProviderImpl} from 'chrome://resources/ash/common/network/mojo_interface_provider.js';
import {NetworkListenerBehavior, NetworkListenerBehaviorInterface} from 'chrome://resources/ash/common/network/network_listener_behavior.js';
import {OncMojo} from 'chrome://resources/ash/common/network/onc_mojo.js';
import {loadTimeData} from 'chrome://resources/ash/common/load_time_data.m.js';
import {CrosHotspotConfigInterface, CrosHotspotConfigObserverInterface, CrosHotspotConfigObserverReceiver, HotspotAllowStatus, HotspotInfo, HotspotState} from 'chrome://resources/mojo/chromeos/ash/services/hotspot_config/public/mojom/cros_hotspot_config.mojom-webui.js';
import {CrosNetworkConfigRemote, FilterType, GlobalPolicy, NO_LIMIT} from 'chrome://resources/mojo/chromeos/services/network_config/public/mojom/cros_network_config.mojom-webui.js';
import {DeviceStateType, NetworkType, OncSource} from 'chrome://resources/mojo/chromeos/services/network_config/public/mojom/network_types.mojom-webui.js';
import {html, mixinBehaviors, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {NetworkSummaryItemElement} from './network_summary_item.js';

/**
 * @constructor
 * @extends {PolymerElement}
 * @implements {NetworkListenerBehaviorInterface}
 */
const NetworkSummaryElementBase =
    mixinBehaviors([NetworkListenerBehavior], PolymerElement);

/** @polymer */
class NetworkSummaryElement extends NetworkSummaryElementBase {
  static get is() {
    return 'network-summary';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      /**
       * Highest priority connected network or null. Set here to update
       * internet-page which updates internet-subpage and internet-detail-page.
       * @type {?OncMojo.NetworkStateProperties}
       */
      defaultNetwork: {
        type: Object,
        value: null,
        notify: true,
      },

      /**
       * The device state for each network device type. We initialize this to
       * include a disabled WiFi type since WiFi is always present. This reduces
       * the amount of visual change on first load.
       * @private {!Object<!OncMojo.DeviceStateProperties>}
       */
      deviceStates: {
        type: Object,
        value() {
          const result = {};
          result[NetworkType.kWiFi] = {
            deviceState: DeviceStateType.kDisabled,
            type: NetworkType.kWiFi,
          };
          return result;
        },
        notify: true,
      },

      /**
       * Array of active network states, one per device type. Initialized to
       * include a default WiFi state (see deviceStates comment).
       * @private {!Array<!OncMojo.NetworkStateProperties>}
       */
      activeNetworkStates_: {
        type: Array,
        value() {
          return [OncMojo.getDefaultNetworkState(NetworkType.kWiFi)];
        },
      },

      /**
       * List of network state data for each network type.
       * @private {!Object<!Array<!OncMojo.NetworkStateProperties>>}
       */
      networkStateLists_: {
        type: Object,
        value() {
          const result = {};
          result[NetworkType.kWiFi] = [];
          return result;
        },
      },

      /** @private {!GlobalPolicy|undefined} */
      globalPolicy_: Object,

      /** @private {!HotspotInfo} */
      hotspotInfo_: Object,

      /**
       * Return true if hotspot feature flag is enabled.
       * @private
       */
      isHotspotFeatureEnabled_: {
        type: Boolean,
        value() {
          return loadTimeData.valueExists('isHotspotEnabled') &&
              loadTimeData.getBoolean('isHotspotEnabled');
        },
      },
    };
  }

  /** @override */
  constructor() {
    super();

    /**
     * Set of GUIDs identifying active networks, one for each type.
     * @private {?Set<string>}
     */
    this.activeNetworkIds_ = null;

    /** @private {!CrosNetworkConfigRemote} */
    this.networkConfig_ =
        MojoInterfaceProviderImpl.getInstance().getMojoServiceRemote();

    if (this.isHotspotFeatureEnabled_) {
      /** @private {!CrosHotspotConfigInterface} */
      this.crosHotspotConfig_ = getHotspotConfig();

      /**
       * @private {!CrosHotspotConfigObserverReceiver}
       */
      this.crosHotspotConfigObserverReceiver_ =
          new CrosHotspotConfigObserverReceiver(
              /**
               * @type {!CrosHotspotConfigObserverInterface}
               */
              (this));
    }
  }

  /** @override */
  ready() {
    super.ready();
    if (this.isHotspotFeatureEnabled_) {
      this.crosHotspotConfig_.addObserver(
          this.crosHotspotConfigObserverReceiver_.$.bindNewPipeAndPassRemote());
    }
  }

  /** @override */
  connectedCallback() {
    super.connectedCallback();

    this.getNetworkLists_();

    // Fetch global policies.
    this.onPoliciesApplied(/*userhash=*/ '');

    if (this.isHotspotFeatureEnabled_) {
      this.onHotspotInfoChanged();
    }
  }

  /** override */
  onHotspotInfoChanged() {
    this.crosHotspotConfig_.getHotspotInfo().then(response => {
      this.hotspotInfo_ = response.hotspotInfo;
    });
  }

  /**
   * CrosNetworkConfigObserver impl
   * @param {!string} userhash
   */
  onPoliciesApplied(userhash) {
    this.networkConfig_.getGlobalPolicy().then(response => {
      this.globalPolicy_ = response.result;
    });
  }

  /**
   * CrosNetworkConfigObserver impl
   * Updates any matching existing active networks. Note: newly active networks
   * will trigger onNetworkStateListChanged which triggers getNetworkLists_.
   * @param {!Array<OncMojo.NetworkStateProperties>} networks
   */
  onActiveNetworksChanged(networks) {
    if (!this.activeNetworkIds_) {
      // Initial list of networks not received yet.
      return;
    }
    networks.forEach(network => {
      const index = this.activeNetworkStates_.findIndex(
          state => state.guid === network.guid);
      if (index !== -1) {
        this.set(['activeNetworkStates_', index], network);
      }
    });
  }

  /** CrosNetworkConfigObserver impl */
  onNetworkStateListChanged() {
    this.getNetworkLists_();
  }

  /** CrosNetworkConfigObserver impl */
  onDeviceStateListChanged() {
    this.getNetworkLists_();
  }

  /**
   * Returns the network-summary-item element corresponding to the
   * |networkType|.
   * @param {!NetworkType} networkType
   * @return {?NetworkSummaryItemElement}
   */
  getNetworkRow(networkType) {
    const networkTypeString = OncMojo.getNetworkTypeString(networkType);
    return /** @type {NetworkSummaryItemElement} */ (
        this.shadowRoot.querySelector(`#${networkTypeString}`));
  }

  /**
   * Requests the list of device states and network states from Chrome.
   * Updates deviceStates, activeNetworkStates, and networkStateLists once the
   * results are returned from Chrome.
   * @private
   */
  getNetworkLists_() {
    // First get the device states.
    this.networkConfig_.getDeviceStateList().then(response => {
      // Second get the network states.
      this.getNetworkStates_(response.result);
    });
  }

  /**
   * Requests the list of network states from Chrome. Updates
   * activeNetworkStates and networkStateLists once the results are returned
   * from Chrome.
   * @param {!Array<!OncMojo.DeviceStateProperties>} deviceStateList
   * @private
   */
  getNetworkStates_(deviceStateList) {
    const filter = {
      filter: FilterType.kVisible,
      limit: NO_LIMIT,
      networkType: NetworkType.kAll,
    };
    this.networkConfig_.getNetworkStateList(filter).then(response => {
      this.updateNetworkStates_(response.result, deviceStateList);
    });
  }

  /**
   * Called after network states are received from getNetworks.
   * @param {!Array<!OncMojo.NetworkStateProperties>} networkStates The state
   *     properties for all visible networks.
   * @param {!Array<!OncMojo.DeviceStateProperties>} deviceStateList
   * @private
   */
  updateNetworkStates_(networkStates, deviceStateList) {
    const newDeviceStates = {};
    for (const device of deviceStateList) {
      newDeviceStates[device.type] = device;
    }

    const orderedNetworkTypes = [
      NetworkType.kEthernet,
      NetworkType.kWiFi,
      NetworkType.kCellular,
      NetworkType.kTether,
      NetworkType.kVPN,
    ];

    // Clear any current networks.
    const activeNetworkStatesByType =
        /** @type {!Map<NetworkType, !OncMojo.NetworkStateProperties>} */
        (new Map());

    // Complete list of states by type.
    const newNetworkStateLists = {};
    for (const type of orderedNetworkTypes) {
      newNetworkStateLists[type] = [];
    }

    let firstConnectedNetwork = null;
    networkStates.forEach(function(networkState) {
      const type = networkState.type;
      if (!activeNetworkStatesByType.has(type)) {
        activeNetworkStatesByType.set(type, networkState);
        if (!firstConnectedNetwork && networkState.type !== NetworkType.kVPN &&
            OncMojo.connectionStateIsConnected(networkState.connectionState)) {
          firstConnectedNetwork = networkState;
        }
      }
      newNetworkStateLists[type].push(networkState);
    }, this);

    this.defaultNetwork = firstConnectedNetwork;


    // Push the active networks onto newActiveNetworkStates in order based on
    // device priority, creating an empty state for devices with no networks.
    const newActiveNetworkStates = [];
    this.activeNetworkIds_ = new Set();
    for (const type of orderedNetworkTypes) {
      const device = newDeviceStates[type];
      if (!device) {
        continue;  // The technology for this device type is unavailable.
      }

      // A VPN device state will always exist in |deviceStateList| even if there
      // is no active VPN. This check is to add the VPN network summary item
      // only if there is at least one active VPN.
      if (device.type === NetworkType.kVPN &&
          !activeNetworkStatesByType.has(device.type)) {
        continue;
      }

      // If both 'Tether' and 'Cellular' technologies exist, merge the network
      // lists and do not add an active network for 'Tether' so that there is
      // only one 'Mobile data' section / subpage.
      if (type === NetworkType.kTether &&
          newDeviceStates[NetworkType.kCellular]) {
        newNetworkStateLists[NetworkType.kCellular] =
            newNetworkStateLists[NetworkType.kCellular].concat(
                newNetworkStateLists[NetworkType.kTether]);
        continue;
      }

      // Note: The active state for 'Cellular' may be a Tether network if both
      // types are enabled but no Cellular network exists (edge case).
      const networkState =
          this.getActiveStateForType_(activeNetworkStatesByType, type);
      if (networkState.source === OncSource.kNone &&
          device.deviceState === DeviceStateType.kProhibited) {
        // Prohibited technologies are enforced by the device policy.
        networkState.source = OncSource.kDevicePolicy;
      }
      newActiveNetworkStates.push(networkState);
      this.activeNetworkIds_.add(networkState.guid);
    }

    this.deviceStates = newDeviceStates;
    this.networkStateLists_ = newNetworkStateLists;
    // Set activeNetworkStates last to rebuild the dom-repeat.
    this.activeNetworkStates_ = newActiveNetworkStates;
    const activeNetworksUpdatedEvent = new CustomEvent(
        'active-networks-updated', {bubbles: true, composed: true});
    this.dispatchEvent(activeNetworksUpdatedEvent);
  }

  /**
   * Returns the active network state for |type| or a default network state.
   * If there is no 'Cellular' network, return the active 'Tether' network if
   * any since the two types are represented by the same section / subpage.
   * @param {!Map<NetworkType, !OncMojo.NetworkStateProperties>}
   *     activeStatesByType
   * @param {!NetworkType} type
   * @return {!OncMojo.NetworkStateProperties|undefined}
   * @private
   */
  getActiveStateForType_(activeStatesByType, type) {
    let activeState = activeStatesByType.get(type);
    if (!activeState && type === NetworkType.kCellular) {
      activeState = activeStatesByType.get(NetworkType.kTether);
    }
    return activeState || OncMojo.getDefaultNetworkState(type);
  }

  /**
   * Provides an id string for summary items. Used in tests.
   * @param {!OncMojo.NetworkStateProperties} network
   * @return {string}
   * @private
   */
  getTypeString_(network) {
    return OncMojo.getNetworkTypeString(network.type);
  }

  /**
   * @param {!Object<!OncMojo.DeviceStateProperties>} deviceStates
   * @return {!OncMojo.DeviceStateProperties|undefined}
   * @private
   */
  getTetherDeviceState_(deviceStates) {
    return this.deviceStates[NetworkType.kTether];
  }

  /**
   * Return whether hotspot row should be shown in network summary.
   *
   * @return {boolean}
   * @private
   */
  shouldShowHotspotSummary_() {
    if (!this.isHotspotFeatureEnabled_ || !this.hotspotInfo_) {
      return false;
    }
    // Hide the hotspot summary row if the device doesn't support hotspot.
    return this.hotspotInfo_.allowStatus !==
        HotspotAllowStatus.kDisallowedNoCellularUpstream &&
        this.hotspotInfo_.allowStatus !==
        HotspotAllowStatus.kDisallowedNoWiFiDownstream;
  }
}

customElements.define(NetworkSummaryElement.is, NetworkSummaryElement);
