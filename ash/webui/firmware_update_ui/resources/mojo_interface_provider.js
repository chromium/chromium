// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from 'chrome://resources/ash/common/assert.js';
import {fakeFirmwareUpdates} from './fake_data.js';
import {FakeUpdateController} from './fake_update_controller.js';
import {FakeUpdateProvider} from './fake_update_provider.js';
import {InstallController, InstallControllerInterface, UpdateProvider, UpdateProviderInterface} from './firmware_update_types.js';
/**
 * @fileoverview
 * Provides singleton access to mojo interfaces with the ability
 * to override them with test/fake implementations.
 */

/**
 * If true this will replace UpdateProvider with a fake.
 * @type {boolean}
 */
let useFakeProviders = false;

/**
 * @type {?UpdateProviderInterface}
 */
let updateProvider = null;

/**
 * @type {?InstallControllerInterface}
 */
let updateController = null;

/**
 * @param {boolean} value
 */
export function setUseFakeProviders(value) {
  useFakeProviders = value;
}

/**
 * @param {!UpdateProviderInterface} testProvider
 */
export function setUpdateProviderForTesting(testProvider) {
  updateProvider = testProvider;
}

/**
 * @param {!InstallControllerInterface} testUpdateController
 */
export function setUpdateControllerForTesting(testUpdateController) {
  updateController = testUpdateController;
}

/**
 * Sets up a FakeUpdateProvider to be used at runtime.
 */
function setupFakeUpdateProvider() {
  const provider = new FakeUpdateProvider();

  provider.setFakeFirmwareUpdates(fakeFirmwareUpdates);
  setUpdateProviderForTesting(provider);
}

/**
 * Sets up a FakeUpdateController to be used at runtime.
 */
function setupFakeUpdateController() {
  setUpdateControllerForTesting(new FakeUpdateController());
}

/**
 * @return {!UpdateProviderInterface}
 */
export function getUpdateProvider() {
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

/**
 * @return {!InstallControllerInterface}
 */
export function getUpdateController() {
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