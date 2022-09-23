// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/ash/common/network/onc_mojo.js';

import {MojoInterfaceProviderImpl} from 'chrome://resources/ash/common/network/mojo_interface_provider.js';
import {DeviceStateProperties, FilterType, NetworkStateProperties, NO_LIMIT} from 'chrome://resources/mojo/chromeos/services/network_config/public/mojom/cros_network_config.mojom-webui.js';
import {ConnectionStateType, NetworkType} from 'chrome://resources/mojo/chromeos/services/network_config/public/mojom/network_types.mojom-webui.js';

/**
 * Checks if the device has a cellular network with connectionState not
 * kNotConnected.
 * @return {!Promise<boolean>}
 */
export function hasActiveCellularNetwork() {
  const networkConfig =
      MojoInterfaceProviderImpl.getInstance().getMojoServiceRemote();
  return networkConfig
      .getNetworkStateList({
        filter: FilterType.kActive,
        networkType: NetworkType.kCellular,
        limit: NO_LIMIT,
      })
      .then((response) => {
        return response.result.some(network => {
          return network.connectionState !== ConnectionStateType.kNotConnected;
        });
      });
}

/**
 * Returns number of phyical SIM and eSIM slots on the current device
 * @param {!DeviceStateProperties|undefined}
 *     deviceState
 * @return {!{pSimSlots: number, eSimSlots: number}}
 */
export function getSimSlotCount(deviceState) {
  let pSimSlots = 0;
  let eSimSlots = 0;

  if (!deviceState || !deviceState.simInfos) {
    return {pSimSlots, eSimSlots};
  }

  for (const simInfo of deviceState.simInfos) {
    if (simInfo.eid) {
      eSimSlots++;
      continue;
    }
    pSimSlots++;
  }

  return {pSimSlots, eSimSlots};
}

/**
 * Checks if the device is currently connected to a WiFi, Ethernet or Tether
 * network.
 * @return {!Promise<boolean>}
 */
export function isConnectedToNonCellularNetwork() {
  const networkConfig =
      MojoInterfaceProviderImpl.getInstance().getMojoServiceRemote();
  return networkConfig
      .getNetworkStateList({
        filter: FilterType.kActive,
        networkType: NetworkType.kAll,
        limit: NO_LIMIT,
      })
      .then((response) => {
        // Filter for connected non-cellular networks.
        return response.result.some(network => {
          return network.connectionState === ConnectionStateType.kOnline &&
              network.type !== NetworkType.kCellular;
        });
      });
}

/**
 * Determines if the current network is on the active sim slot.
 * @param {?NetworkStateProperties} networkState
 * @param {?DeviceStateProperties} deviceState
 */
export function isActiveSim(networkState, deviceState) {
  if (!networkState || networkState.type !== NetworkType.kCellular) {
    return false;
  }

  const iccid = networkState.typeState.cellular.iccid;
  if (!iccid || !deviceState || !deviceState.simInfos) {
    return false;
  }
  const isActiveSim = deviceState.simInfos.find(simInfo => {
    return simInfo.iccid === iccid && simInfo.isPrimary;
  });
  return !!isActiveSim;
}
