// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from 'chrome://resources/js/assert.m.js';
import {fakeFirmwareUpdates} from './fake_data.js';
import {FakeUpdateProvider} from './fake_update_provider.js';
import {UpdateProviderInterface} from './firmware_update_types.js';

/**
 * @fileoverview
 * Provides singleton access to mojo interfaces with the ability
 * to override them with test/fake implementations.
 */

/**
 * @type {?UpdateProviderInterface}
 */
let updateProvider = null;

/**
 * @param {!UpdateProviderInterface} testProvider
 */
export function setUpdateProviderForTesting(testProvider) {
  updateProvider = testProvider;
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
 * @return {!UpdateProviderInterface}
 */
export function getUpdateProvider() {
  if (!updateProvider) {
    // TODO(michaelcheco): Instantiate a real mojo interface here.
    setupFakeUpdateProvider();
  }

  assert(!!updateProvider);
  return updateProvider;
}
