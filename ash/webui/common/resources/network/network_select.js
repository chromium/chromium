// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Polymer element wrapping network-list including the
 * networkConfig mojo API calls to populate it.
 */

import '//resources/ash/common/cr_elements/cr_shared_vars.css.js';
import '//resources/polymer/v3_0/paper-progress/paper-progress.js';
import './network_list.js';

import {assert} from '//resources/ash/common/assert.js';
import {CrosNetworkConfigInterface, FilterType, GlobalPolicy, NO_LIMIT} from '//resources/mojo/chromeos/services/network_config/public/mojom/cros_network_config.mojom-webui.js';
import {ConnectionStateType, NetworkType} from '//resources/mojo/chromeos/services/network_config/public/mojom/network_types.mojom-webui.js';
import {mixinBehaviors, PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {MojoInterfaceProvider, MojoInterfaceProviderImpl} from './mojo_interface_provider.js';
import {NetworkList} from './network_list_types.js';
import {NetworkListenerBehavior, NetworkListenerBehaviorInterface} from './network_listener_behavior.js';
import {getTemplate} from './network_select.html.js';
import {OncMojo} from './onc_mojo.js';

/**
 * @constructor
 * @extends {PolymerElement}
 * @implements {NetworkListenerBehaviorInterface}
 */
const NetworkSelectElementBase =
    mixinBehaviors([NetworkListenerBehavior], PolymerElement);

/** @polymer */
class NetworkSelectElement extends NetworkSelectElementBase {
  static get is() {
    return 'network-select';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      /**
       * Show all buttons in list items.
       */
      showButtons: {
        type: Boolean,
        value: false,
        reflectToAttribute: true,
      },

      /**
       * The list of custom items to display after the list of networks.
       * See NetworkList for details.
       * @type {!Array<NetworkList.CustomItemState>}
       */
      customItems: {
        type: Array,
        value() {
          return [];
        },
      },

      /** Whether to show technology badges on mobile network icons. */
      showTechnologyBadge: {
        type: Boolean,
        value: true,
      },

      /**
       * Whether this element should trigger periodic Wi-Fi scans to update the
       * list of networks. If true, a background scan is performed every 10
       * seconds.
       */
      enableWifiScans: {
        type: Boolean,
        value: true,
        observer: 'onEnableWifiScansChanged_',
      },

      /**
       * Whether to show a progress indicator at the top of the network list
       * while a scan (e.g., for nearby Wi-Fi networks) is in progress.
       */
      showScanProgress: {
        type: Boolean,
        value: false,
      },

      /** Whether cellular activation is unavailable in the current context. */
      activationUnavailable: Boolean,

      /**
       * List of all network state data for all visible networks.
       * @private {!Array<!OncMojo.NetworkStateProperties>}
       */
      networkStateList_: {
        type: Array,
        value() {
          return [];
        },
      },

      /**
       * Whether a network scan is currently in progress.
       * @private
       */
      isScanOngoing_: {
        type: Boolean,
        value: false,
      },

      /** @private {!GlobalPolicy|undefined} */
      globalPolicy_: Object,
    };
  }

  /** @override */
  constructor() {
    super();

    /** @type {!OncMojo.NetworkStateProperties|undefined} */
    this.defaultNetworkState_ = undefined;

    /** @private {number|null} */
    this.scanIntervalId_ = null;

    /** @private {?CrosNetworkConfigInterface} */
    this.networkConfig_ =
        MojoInterfaceProviderImpl.getInstance().getMojoServiceRemote();
  }

  /** @override */
  connectedCallback() {
    super.connectedCallback();

    this.refreshNetworks();
    this.onEnableWifiScansChanged_();
  }

  /** @override */
  disconnectedCallback() {
    super.disconnectedCallback();

    this.clearScheduledScans_();
  }

  /**
   * Returns network list object for testing.
   */
  getNetworkListForTest() {
    return this.$.networkList.shadowRoot.querySelector('#networkList');
  }

  /**
   * Returns network list item object for testing.
   */
  getNetworkListItemByNameForTest(name) {
    const networkList =
        this.$.networkList.shadowRoot.querySelector('#networkList');
    assert(networkList);
    for (const network of networkList.children) {
      if (network.is === 'network-list-item' &&
          network.shadowRoot.querySelector('#divText').children[0].innerText ===
              name) {
        return network.shadowRoot.getElementById('divOuter');
      }
    }
    return null;
  }

  focus() {
    this.$.networkList.focus();
  }

  /** CrosNetworkConfigObserver impl */
  onNetworkStateListChanged() {
    this.refreshNetworks();
  }

  /** CrosNetworkConfigObserver impl */
  onDeviceStateListChanged() {
    this.refreshNetworks();
  }

  /**
   * Requests the device and network states. May be called externally to force a
   * refresh and list update (e.g. when the element is shown).
   */
  refreshNetworks() {
    this.networkConfig_.getDeviceStateList().then(response => {
      this.onGetDeviceStates_(response.result);
    });
    this.networkConfig_.getGlobalPolicy().then(response => {
      this.globalPolicy_ = response.result;
    });
  }

  /**
   * Returns default network if it is present.
   * @return {!OncMojo.NetworkStateProperties|undefined}
   */
  getDefaultNetwork() {
    let defaultNetwork;
    for (let i = 0; i < this.networkStateList_.length; ++i) {
      const state = this.networkStateList_[i];
      if (OncMojo.connectionStateIsConnected(state.connectionState)) {
        defaultNetwork = state;
        break;
      }
      if (state.connectionState === ConnectionStateType.kConnecting &&
          !defaultNetwork) {
        defaultNetwork = state;
        // Do not break here in case a non WiFi network is connecting but a
        // WiFi network is connected.
      } else if (state.type === NetworkType.kWiFi) {
        break;  // Non connecting or connected WiFI networks are always last.
      }
    }
    return defaultNetwork;
  }

  /**
   * Returns network with specified GUID if it is available.
   * @param {string} guid of network.
   * @return {!OncMojo.NetworkStateProperties|undefined}
   */
  getNetwork(guid) {
    return this.networkStateList_.find(function(network) {
      return network.guid === guid;
    });
  }

  /**
   * Handler for changes to |enableWifiScans| which either schedules upcoming
   * scans or clears already-scheduled scans.
   * @private
   */
  onEnableWifiScansChanged_() {
    // Clear any scans which are already scheduled.
    this.clearScheduledScans_();

    // If Scans are disabled, return early.
    if (!this.enableWifiScans) {
      return;
    }

    const INTERVAL_MS = 10 * 1000;
    // Request only WiFi network scans. Tether and Cellular scans are not useful
    // here. Cellular scans are disruptive and should only be triggered by
    // explicit user action.
    const kWiFi = NetworkType.kWiFi;
    this.networkConfig_.requestNetworkScan(kWiFi);
    this.scanIntervalId_ = window.setInterval(function() {
      this.networkConfig_.requestNetworkScan(kWiFi);
    }.bind(this), INTERVAL_MS);
  }

  /**
   * Clears any scheduled Wi-FI scans; no-op if there were no scans scheduled.
   * @private
   */
  clearScheduledScans_() {
    if (this.scanIntervalId_ !== null) {
      window.clearInterval(this.scanIntervalId_);
      this.scanIntervalId_ = null;
    }
  }

  /**
   * @param {!Array<!OncMojo.DeviceStateProperties>} deviceStates
   * @private
   */
  onGetDeviceStates_(deviceStates) {
    this.isScanOngoing_ =
        deviceStates.some((deviceState) => !!deviceState.scanning);

    const filter = {
      filter: FilterType.kVisible,
      networkType: NetworkType.kAll,
      limit: NO_LIMIT,
    };
    this.networkConfig_.getNetworkStateList(filter).then(response => {
      this.onGetNetworkStateList_(deviceStates, response.result);
    });
  }

  /**
   * @param {!Array<!OncMojo.DeviceStateProperties>} deviceStates
   * @param {!Array<!OncMojo.NetworkStateProperties>} networkStates
   * @private
   */
  onGetNetworkStateList_(deviceStates, networkStates) {
    this.networkStateList_ = networkStates;
    this.dispatchEvent(
        new CustomEvent('network-list-changed', {detail: networkStates}));

    const defaultNetwork = this.getDefaultNetwork();

    if ((!defaultNetwork && !this.defaultNetworkState_) ||
        (defaultNetwork && this.defaultNetworkState_ &&
         defaultNetwork.guid === this.defaultNetworkState_.guid &&
         defaultNetwork.connectionState ===
             this.defaultNetworkState_.connectionState)) {
      return;  // No change to network or ConnectionState
    }
    this.defaultNetworkState_ = defaultNetwork ?
        /** @type {!OncMojo.NetworkStateProperties|undefined} */ (
            Object.assign({}, defaultNetwork)) :
        undefined;
    // Note: event.detail will be {} if defaultNetwork is undefined.
    this.dispatchEvent(
        new CustomEvent('default-network-changed', {detail: defaultNetwork}));
  }

  /**
   * Event triggered when a network-list-item is selected.
   * @param {!{target: HTMLElement, detail: !OncMojo.NetworkStateProperties}} e
   * @private
   */
  onNetworkListItemSelected_(e) {
    const state = e.detail;
    e.target.blur();

    this.dispatchEvent(
        new CustomEvent('network-item-selected', {detail: state}));
  }
}

customElements.define(NetworkSelectElement.is, NetworkSelectElement);
