// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from 'chrome://resources/js/assert.js';

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
 */
const useFakeProviders = false;

let systemDataProvider: SystemDataProviderInterface|null = null;

let systemRoutineController: SystemRoutineControllerInterface|null = null;

let networkHealthProvider: NetworkHealthProviderInterface|null = null;

let inputDataProvider: InputDataProviderInterface|null = null;

export function setSystemDataProviderForTesting(
    testProvider: SystemDataProviderInterface): void {
  systemDataProvider = testProvider;
}

/**
 * Create a FakeSystemDataProvider with reasonable fake data.
 */
function setupFakeSystemDataProvider(): void {
  const provider = new FakeSystemDataProvider();
  provider.setFakeBatteryChargeStatus(fakeBatteryChargeStatus);
  provider.setFakeBatteryHealth(fakeBatteryHealth);
  provider.setFakeBatteryInfo(fakeBatteryInfo);
  provider.setFakeCpuUsage(fakeCpuUsage);
  provider.setFakeMemoryUsage(fakeMemoryUsage);
  provider.setFakeSystemInfo(fakeSystemInfo);
  setSystemDataProviderForTesting(provider);
}

export function getSystemDataProvider(): SystemDataProviderInterface {
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

export function setSystemRoutineControllerForTesting(
    testController: SystemRoutineControllerInterface): void {
  systemRoutineController = testController;
}

/**
 * Create a FakeSystemRoutineController with reasonable fake data.
 */
function setupFakeSystemRoutineController(): void {
  const controller = new FakeSystemRoutineController();
  // Enable all routines by default.
  controller.setFakeSupportedRoutines(controller.getAllRoutines());
  setSystemRoutineControllerForTesting(controller);
}

export function getSystemRoutineController(): SystemRoutineControllerInterface {
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

export function setNetworkHealthProviderForTesting(
    testProvider: NetworkHealthProviderInterface): void {
  networkHealthProvider = testProvider;
}

/**
 * Create a FakeNetworkHealthProvider with reasonable fake data.
 */
function setupFakeNetworkHealthProvider(): void {
  const provider = new FakeNetworkHealthProvider();
  // The fake provides a stable state with all networks connected.
  provider.setFakeNetworkGuidInfo([fakeAllNetworksAvailable]);
  provider.setFakeNetworkState('ethernetGuid', [fakeEthernetNetwork]);
  provider.setFakeNetworkState('wifiGuid', [fakeWifiNetwork]);
  provider.setFakeNetworkState('cellularGuid', [fakeCellularNetwork]);

  setNetworkHealthProviderForTesting(provider);
}

export function getNetworkHealthProvider(): NetworkHealthProviderInterface {
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
function setupFakeInputDataProvider(): void {
  const provider = new FakeInputDataProvider();
  provider.setFakeConnectedDevices(fakeKeyboards, fakeTouchDevices);
  setInputDataProviderForTesting(provider);
}

export function setInputDataProviderForTesting(
    testProvider: InputDataProviderInterface): void {
  inputDataProvider = testProvider;
}

export function getInputDataProvider(): InputDataProviderInterface {
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
