// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from 'chrome://resources/ash/common/assert.js';

import {fakeAllNetworksAvailable, fakeBatteryChargeStatus, fakeBatteryHealth, fakeBatteryInfo, fakeCellularNetwork, fakeCpuUsage, fakeEthernetNetwork, fakeKeyboards, fakeMemoryUsage, fakeSystemInfo, fakeTouchDevices, fakeWifiNetwork} from './fake_data.js';
import {FakeInputDataProvider} from './fake_input_data_provider.js';
import {FakeNetworkHealthProvider} from './fake_network_health_provider.js';
import {FakeSystemDataProvider} from './fake_system_data_provider.js';
import {FakeSystemRoutineController} from './fake_system_routine_controller.js';
import {InputDataProvider, InputDataProviderInterface} from './input_data_provider.mojom-webui.js';
import {NetworkHealthProvider, NetworkHealthProviderInterface} from './network_health_provider.mojom-webui.js';
import {SystemDataProvider, SystemDataProviderInterface} from './system_data_provider.mojom-webui.js';
import {SystemRoutineController, SystemRoutineControllerInterface} from './system_routine_controller.mojom-webui.js';

/**
 * @fileoverview
 * Provides singleton access to mojo interfaces with the ability
 * to override them with test/fake implementations.
 */

/**
 * If true this will replace all providers with fakes.
 * @type {boolean}
 */
const useFakeProviders = false;

/**
 * @type {?SystemDataProviderInterface}
 */
let systemDataProvider = null;

/**
 * @type {?SystemRoutineControllerInterface}
 */
let systemRoutineController = null;

/**
 * @type {?NetworkHealthProviderInterface}
 */
let networkHealthProvider = null;

/**
 * @type {?InputDataProviderInterface}
 */
let inputDataProvider = null;

/**
 * @param {!SystemDataProviderInterface} testProvider
 */
export function setSystemDataProviderForTesting(testProvider) {
  systemDataProvider = testProvider;
}

/**
 * Create a FakeSystemDataProvider with reasonable fake data.
 */
function setupFakeSystemDataProvider() {
  systemDataProvider = new FakeSystemDataProvider();
  systemDataProvider.setFakeBatteryChargeStatus(fakeBatteryChargeStatus);
  systemDataProvider.setFakeBatteryHealth(fakeBatteryHealth);
  systemDataProvider.setFakeBatteryInfo(fakeBatteryInfo);
  systemDataProvider.setFakeCpuUsage(fakeCpuUsage);
  systemDataProvider.setFakeMemoryUsage(fakeMemoryUsage);
  systemDataProvider.setFakeSystemInfo(fakeSystemInfo);
}

/**
 * @return {!SystemDataProviderInterface}
 */
export function getSystemDataProvider() {
  if (!systemDataProvider) {
    if (useFakeProviders) {
      setupFakeSystemDataProvider();
    } else {
      systemDataProvider = SystemDataProvider.getRemote();
    }
  }

  assert(!!systemDataProvider);
  return systemDataProvider;
}

/**
 * @param {!SystemRoutineControllerInterface} testController
 */
export function setSystemRoutineControllerForTesting(testController) {
  systemRoutineController = testController;
}

/**
 * Create a FakeSystemRoutineController with reasonable fake data.
 */
function setupFakeSystemRoutineController() {
  systemRoutineController = new FakeSystemRoutineController();
  systemRoutineController.setDelayTimeInMillisecondsForTesting(-1);

  // Enable all routines by default.
  systemRoutineController.setFakeSupportedRoutines(
      systemRoutineController.getAllRoutines());
}

/**
 * @return {!SystemRoutineControllerInterface}
 */
export function getSystemRoutineController() {
  if (!systemRoutineController) {
    if (useFakeProviders) {
      setupFakeSystemRoutineController();
    } else {
      systemRoutineController = SystemRoutineController.getRemote();
    }
  }

  assert(!!systemRoutineController);
  return systemRoutineController;
}

/**
 * @param {!NetworkHealthProviderInterface} testProvider
 */
export function setNetworkHealthProviderForTesting(testProvider) {
  networkHealthProvider = testProvider;
}

/**
 * Create a FakeNetworkHealthProvider with reasonable fake data.
 */
function setupFakeNetworkHealthProvider() {
  const provider = new FakeNetworkHealthProvider();
  // The fake provides a stable state with all networks connected.
  provider.setFakeNetworkGuidInfo([fakeAllNetworksAvailable]);
  provider.setFakeNetworkState('ethernetGuid', [fakeEthernetNetwork]);
  provider.setFakeNetworkState('wifiGuid', [fakeWifiNetwork]);
  provider.setFakeNetworkState('cellularGuid', [fakeCellularNetwork]);

  setNetworkHealthProviderForTesting(provider);
}

/**
 * @return {!NetworkHealthProviderInterface}
 */
export function getNetworkHealthProvider() {
  if (!networkHealthProvider) {
    if (useFakeProviders) {
      setupFakeNetworkHealthProvider();
    } else {
      networkHealthProvider = NetworkHealthProvider.getRemote();
    }
  }

  assert(!!networkHealthProvider);
  return networkHealthProvider;
}

// Creates a FakeInputDataProvider with fake devices setup.
function setupFakeInputDataProvider() {
  const provider = new FakeInputDataProvider();
  provider.setFakeConnectedDevices(fakeKeyboards, fakeTouchDevices);
  setInputDataProviderForTesting(provider);
}

/**
 * @param {!InputDataProviderInterface} testProvider
 */
export function setInputDataProviderForTesting(testProvider) {
  inputDataProvider = testProvider;
}

/**
 * @return {!InputDataProviderInterface}
 */
export function getInputDataProvider() {
  if (!inputDataProvider) {
    if (useFakeProviders) {
      setupFakeInputDataProvider();
    } else {
      inputDataProvider = InputDataProvider.getRemote();
    }
  }

  assert(!!inputDataProvider);
  return inputDataProvider;
}
