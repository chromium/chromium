// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from 'chrome://resources/js/assert.js';

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
 * These variables are bound to real mojo services when running
 * in a live environment and set to fake services when testing.
 */
let systemDataProvider: SystemDataProviderInterface|null = null;
let systemRoutineController: SystemRoutineControllerInterface|null = null;
let networkHealthProvider: NetworkHealthProviderInterface|null = null;
let inputDataProvider: InputDataProviderInterface|null = null;

export function setSystemDataProviderForTesting(
    testProvider: SystemDataProviderInterface): void {
  systemDataProvider = testProvider;
}

export function getSystemDataProvider(): SystemDataProviderInterface {
  if (!systemDataProvider) {
    systemDataProvider = SystemDataProvider.getRemote();
  }

  assert(!!systemDataProvider);
  return systemDataProvider;
}

export function setSystemRoutineControllerForTesting(
    testController: SystemRoutineControllerInterface): void {
  systemRoutineController = testController;
}

export function getSystemRoutineController(): SystemRoutineControllerInterface {
  if (!systemRoutineController) {
    systemRoutineController = SystemRoutineController.getRemote();
  }

  assert(!!systemRoutineController);
  return systemRoutineController;
}

export function setNetworkHealthProviderForTesting(
    testProvider: NetworkHealthProviderInterface): void {
  networkHealthProvider = testProvider;
}

export function getNetworkHealthProvider(): NetworkHealthProviderInterface {
  if (!networkHealthProvider) {
    networkHealthProvider = NetworkHealthProvider.getRemote();
  }

  assert(!!networkHealthProvider);
  return networkHealthProvider;
}

export function setInputDataProviderForTesting(
    testProvider: InputDataProviderInterface): void {
  inputDataProvider = testProvider;
}

export function getInputDataProvider(): InputDataProviderInterface {
  if (!inputDataProvider) {
    inputDataProvider = InputDataProvider.getRemote();
  }

  assert(!!inputDataProvider);
  return inputDataProvider;
}
