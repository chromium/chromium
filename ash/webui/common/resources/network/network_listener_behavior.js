// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Polymer behavior for observing CrosNetworkConfigObserver
 * events.
 */

import {CrosNetworkConfigObserver, CrosNetworkConfigObserverReceiver, NetworkStateProperties} from '//resources/mojo/chromeos/services/network_config/public/mojom/cros_network_config.mojom-webui.js';

import {MojoInterfaceProviderImpl} from './mojo_interface_provider.js';

/** @polymerBehavior */
export const NetworkListenerBehavior = {
  /** @private {?CrosNetworkConfigObserver} */
  observer_: null,

  /** @override */
  attached() {
    this.observer_ = new CrosNetworkConfigObserverReceiver(this);
    MojoInterfaceProviderImpl.getInstance().getMojoServiceRemote().addObserver(
        this.observer_.$.bindNewPipeAndPassRemote());
  },

  // CrosNetworkConfigObserver methods. Override these in the implementation.

  /**
   * @param {!Array<NetworkStateProperties>}
   *     activeNetworks
   */
  onActiveNetworksChanged(activeNetworks) {},

  /** @param {!NetworkStateProperties} network */
  onNetworkStateChanged(network) {},

  onNetworkStateListChanged() {},

  onDeviceStateListChanged() {},

  onVpnProvidersChanged() {},

  onNetworkCertificatesChanged() {},

  /** @param {string} userhash */
  onPoliciesApplied(userhash) {},
};

/** @interface */
export class NetworkListenerBehaviorInterface {
  constructor() {
    /** @private {?CrosNetworkConfigObserver} */
    this.observer_;
  }

  attached() {}

  /**
   * @param {!Array<NetworkStateProperties>}
   *     activeNetworks
   */
  onActiveNetworksChanged(activeNetworks) {}

  /** @param {!NetworkStateProperties} network */
  onNetworkStateChanged(network) {}

  onNetworkStateListChanged() {}

  onDeviceStateListChanged() {}

  onVpnProvidersChanged() {}

  onNetworkCertificatesChanged() {}

  /** @param {!string} userhash */
  onPoliciesApplied(userhash) {}
}
