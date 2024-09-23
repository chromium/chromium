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
import {CrosHotspotConfigInterface, CrosHotspotConfigObserverReceiver, HotspotAllowStatus, HotspotInfo} from 'chrome://resources/ash/common/hotspot/cros_hotspot_config.mojom-webui.js';
import {MojoInterfaceProviderImpl} from 'chrome://resources/ash/common/network/mojo_interface_provider.js';
import {NetworkListenerBehavior, NetworkListenerBehaviorInterface} from 'chrome://resources/ash/common/network/network_listener_behavior.js';
import {OncMojo} from 'chrome://resources/ash/common/network/onc_mojo.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {CrosNetworkConfigInterface, FilterType, GlobalPolicy, NO_LIMIT} from 'chrome://resources/mojo/chromeos/services/network_config/public/mojom/cros_network_config.mojom-webui.js';
import {DeviceStateType, NetworkType, OncSource} from 'chrome://resources/mojo/chromeos/services/network_config/public/mojom/network_types.mojom-webui.js';
import {mixinBehaviors, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {castExists} from '../assert_extras.js';
import {Constructor} from '../common/types.js';

import {getTemplate} from './network_summary.html.js';
import {NetworkSummaryItemElement} from './network_summary_item.js';

const NetworkSummaryElementBase =
    mixinBehaviors([NetworkListenerBehavior], PolymerElement) as
    Constructor<PolymerElement&NetworkListenerBehaviorInterface>;

export class NetworkSummaryElement extends NetworkSummaryElementBase {
  static get is() {
    return 'network-summary' as const;
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      /**
       * Highest priority connected network or null. Set here to update
       * internet-page which updates internet-subpage and internet-detail-page.
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
       */
      deviceStates: {
        type: Object,
        value() {
          return {
            [NetworkType.kWiFi]: {
              deviceState: DeviceStateType.kDisabled,
              type: NetworkType.kWiFi,
            },
          };
        },
        notify: true,
      },

      /**
       * Hotspot information including state, active connected client count,
       * allow status and hotspot configuration. Set here to update
       * internet-page which updates hotspot-subpage.
       */
      hotspotInfo: {
        type: Object,
        notify: true,
      },

      /**
       * Array of active network states, one per device type. Initialized to
       * include a default WiFi state (see deviceStates comment).
       */
      activeNetworkStates_: {
        type: Array,
        value() {
          return [OncMojo.getDefaultNetworkState(NetworkType.kWiFi)];
        },
      },

      /**
       * List of network state data for each network type.
       */
      networkStateLists_: {
        type: Object,
        value() {
          return {
            [NetworkType.kWiFi]: [],
          };
        },
      },

      globalPolicy_: Object,

      /**
       * Return true if instant hotspot rebrand feature flag is enabled
       */
      isInstantHotspotRebrandEnabled_: {
        type: Boolean,
        value() {
          return loadTimeData.valueExists('isInstantHotspotRebrandEnabled') &&
              loadTimeData.getBoolean('isInstantHotspotRebrandEnabled');
        },
      },
    };
  }

  defaultNetwork: OncMojo.NetworkStateProperties|null;
  hotspotInfo: HotspotInfo|undefined;
  deviceStates: Record<NetworkType, OncMojo.DeviceStateProperties>;
  private activeNetworkIds_: Set<string>|null;
  private activeNetworkStates_: OncMojo.NetworkStateProperties[];
  private crosHotspotConfig_: CrosHotspotConfigInterface;
  private crosHotspotConfigObserverReceiver_: CrosHotspotConfigObserverReceiver;
  private globalPolicy_: GlobalPolicy|undefined;
  private isInstantHotspotRebrandEnabled_: boolean;
  private networkConfig_: CrosNetworkConfigInterface;
  private networkStateLists_:
      Record<NetworkType, OncMojo.NetworkStateProperties[]>;

  constructor() {
    super();

    /**
     * Set of GUIDs identifying active networks, one for each type.
     */
    this.activeNetworkIds_ = null;

    this.networkConfig_ =
        MojoInterfaceProviderImpl.getInstance().getMojoServiceRemote();
    this.crosHotspotConfig_ = getHotspotConfig();
    this.crosHotspotConfigObserverReceiver_ =
        new CrosHotspotConfigObserverReceiver(this);
  }

  override ready(): void {
    super.ready();

    this.crosHotspotConfig_.addObserver(
        this.crosHotspotConfigObserverReceiver_.$.bindNewPipeAndPassRemote());
  }

  override connectedCallback(): void {
    super.connectedCallback();

    this.getNetworkLists_();

    // Fetch global policies.
    this.onPoliciesApplied(/*userhash=*/ '');
    this.onHotspotInfoChanged();
  }

  async onHotspotInfoChanged(): Promise<void> {
    const response = await this.crosHotspotConfig_.getHotspotInfo();
    this.hotspotInfo = response.hotspotInfo;
  }

  /**
   * CrosNetworkConfigObserver impl
   */
  override async onPoliciesApplied(_userhash: string): Promise<void> {
    const response = await this.networkConfig_.getGlobalPolicy();
    this.globalPolicy_ = response.result;
  }

  /**
   * CrosNetworkConfigObserver impl
   * Updates any matching existing active networks. Note: newly active networks
   * will trigger onNetworkStateListChanged which triggers getNetworkLists_.
   */
  override onActiveNetworksChanged(networks: OncMojo.NetworkStateProperties[]):
      void {
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
  override onNetworkStateListChanged(): void {
    this.getNetworkLists_();
  }

  /** CrosNetworkConfigObserver impl */
  override onDeviceStateListChanged(): void {
    this.getNetworkLists_();
  }

  /**
   * Returns the network-summary-item element corresponding to the
   * |networkType|.
   */
  getNetworkRow(networkType: NetworkType): NetworkSummaryItemElement|null {
    const networkTypeString = OncMojo.getNetworkTypeString(networkType);
    return this.shadowRoot!.querySelector<NetworkSummaryItemElement>(
        `#${networkTypeString}`);
  }

  /**
   * Requests the list of device states and network states from Chrome.
   * Updates deviceStates, activeNetworkStates, and networkStateLists once the
   * results are returned from Chrome.
   */
  private async getNetworkLists_(): Promise<void> {
    // First get the device states.
    const response = await this.networkConfig_.getDeviceStateList();
    // Second get the network states.
    this.getNetworkStates_(response.result);
  }

  /**
   * Requests the list of network states from Chrome. Updates
   * activeNetworkStates and networkStateLists once the results are returned
   * from Chrome.
   */
  private async getNetworkStates_(
      deviceStateList: OncMojo.DeviceStateProperties[]): Promise<void> {
    const filter = {
      filter: FilterType.kVisible,
      limit: NO_LIMIT,
      networkType: NetworkType.kAll,
    };
    const response = await this.networkConfig_.getNetworkStateList(filter);
    this.updateNetworkStates_(response.result, deviceStateList);
  }

  /**
   * Called after network states are received from getNetworks.
   */
  private updateNetworkStates_(
      networkStates: OncMojo.NetworkStateProperties[],
      deviceStateList: OncMojo.DeviceStateProperties[]): void {
    const newDeviceStates: Record<string, OncMojo.DeviceStateProperties> = {};
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
        new Map<NetworkType, OncMojo.NetworkStateProperties>();

    // Complete list of states by type.
    const newNetworkStateLists:
        Record<string, OncMojo.NetworkStateProperties[]> = {};
    for (const type of orderedNetworkTypes) {
      newNetworkStateLists[type] = [];
    }

    let firstConnectedNetwork: OncMojo.NetworkStateProperties|null = null;
    networkStates.forEach((networkState) => {
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
    const newActiveNetworkStates: OncMojo.NetworkStateProperties[] = [];
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
          newDeviceStates[NetworkType.kCellular] &&
          !this.isInstantHotspotRebrandEnabled_) {
        newNetworkStateLists[NetworkType.kCellular] =
            newNetworkStateLists[NetworkType.kCellular].concat(
                newNetworkStateLists[NetworkType.kTether]);
        continue;
      }

      // Note: The active state for 'Cellular' may be a Tether network if both
      // types are enabled but no Cellular network exists (edge case).
      const networkState = castExists(
          this.getActiveStateForType_(activeNetworkStatesByType, type));
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
   */
  private getActiveStateForType_(
      activeStatesByType: Map<NetworkType, OncMojo.NetworkStateProperties>,
      type: NetworkType): OncMojo.NetworkStateProperties|undefined {
    let activeState = activeStatesByType.get(type);
    if (!activeState && type === NetworkType.kCellular &&
        !this.isInstantHotspotRebrandEnabled_) {
      activeState = activeStatesByType.get(NetworkType.kTether);
    }
    return activeState || OncMojo.getDefaultNetworkState(type);
  }

  /**
   * Provides an id string for summary items. Used in tests.
   */
  private getTypeString_(network: OncMojo.NetworkStateProperties): string {
    return OncMojo.getNetworkTypeString(network.type);
  }

  private getTetherDeviceState_(
      deviceStates: Record<NetworkType, OncMojo.DeviceStateProperties>):
      OncMojo.DeviceStateProperties|undefined {
    return deviceStates[NetworkType.kTether];
  }

  /**
   * Return whether hotspot row should be shown in network summary.
   */
  private shouldShowHotspotSummary_(): boolean {
    if (!this.hotspotInfo) {
      return false;
    }
    // Hide the hotspot summary row if the device doesn't support hotspot.
    return this.hotspotInfo.allowStatus !==
        HotspotAllowStatus.kDisallowedNoCellularUpstream &&
        this.hotspotInfo.allowStatus !==
        HotspotAllowStatus.kDisallowedNoWiFiDownstream &&
        this.hotspotInfo.allowStatus !==
        HotspotAllowStatus.kDisallowedNoWiFiSecurityModes;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [NetworkSummaryElement.is]: NetworkSummaryElement;
  }
}

customElements.define(NetworkSummaryElement.is, NetworkSummaryElement);
