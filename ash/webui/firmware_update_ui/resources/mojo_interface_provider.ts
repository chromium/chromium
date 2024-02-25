// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from 'chrome://resources/js/assert.js';

import {fakeFirmwareUpdates} from './fake_data.js';
import {FakeUpdateController} from './fake_update_controller.js';
import {FakeUpdateProvider} from './fake_update_provider.js';
import {InstallController, InstallControllerInterface, UpdateProvider, UpdateProviderInterface} from './firmware_update.mojom-webui.js';

/**
 * @fileoverview
 * Provides singleton access to mojo interfaces with the ability
 * to override them with test/fake implementations.
 */

/**
 * If true this will replace UpdateProvider with a fake.
 */
let useFakeProviders = false;

let updateProvider: UpdateProviderInterface|null = null;

let updateController: InstallControllerInterface|null = null;

export function setUseFakeProviders(value: boolean): void {
  useFakeProviders = value;
}

export function setUpdateProviderForTesting(
    testProvider: UpdateProviderInterface): void {
  updateProvider = testProvider;
}

export function setUpdateControllerForTesting(
    testUpdateController: InstallControllerInterface): void {
  updateController = testUpdateController;
}

/**
 * Sets up a FakeUpdateProvider to be used at runtime.
 */
function setupFakeUpdateProvider(): void {
  const provider = new FakeUpdateProvider();

  provider.setFakeFirmwareUpdates(fakeFirmwareUpdates);
  setUpdateProviderForTesting(provider);
}

/**
 * Sets up a FakeUpdateController to be used at runtime.
 */
function setupFakeUpdateController(): void {
  setUpdateControllerForTesting(new FakeUpdateController());
}

export function getUpdateProvider(): UpdateProviderInterface {
  if (!updateProvider) {
    if (useFakeProviders) {
      setupFakeUpdateProvider();
    } else {
      updateProvider = UpdateProvider.getRemote();
    }
  }

  assert(!!updateProvider);
  return updateProvider;
}

export function getUpdateController(): InstallControllerInterface {
  if (!updateController) {
    if (useFakeProviders) {
      setupFakeUpdateController();
    } else {
      updateController = InstallController.getRemote();
    }
  }

  assert(!!updateController);
  return updateController;
}
