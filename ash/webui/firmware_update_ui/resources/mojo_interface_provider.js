// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from 'chrome://resources/js/assert.m.js';
import {fakeFirmwareUpdates} from './fake_data.js';
import {FakeUpdateController} from './fake_update_controller.js';
import {FakeUpdateProvider} from './fake_update_provider.js';
import {UpdateControllerInterface, UpdateProvider, UpdateProviderInterface} from './firmware_update_types.js';

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
 * @type {?UpdateControllerInterface}
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
 * @param {!UpdateControllerInterface} testUpdateController
 */
export function setUpdateControllerForTesting(testUpdateController) {
  updateController = testUpdateController;
}

/**
 * Sets up a FakeUpdateProvider to be used at runtime.
 * TODO(michaelcheco): Remove once mojo bindings are implemented.
 */
function setupFakeUpdateProvider() {
  const provider = new FakeUpdateProvider();

  provider.setFakeFirmwareUpdates(fakeFirmwareUpdates);
  setUpdateProviderForTesting(provider);
}

/**
 * Sets up a FakeUpdateController to be used at runtime.
 * TODO(michaelcheco): Remove once mojo bindings are implemented.
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

/** @return {!UpdateControllerInterface} */
export function getUpdateController() {
  if (!updateController) {
    // TODO(michaelcheco): Instantiate a real mojo interface here.
    setupFakeUpdateController();
  }

  assert(!!updateController);
  return updateController;
}
