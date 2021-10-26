// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from 'chrome://resources/js/assert.m.js';
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
 * @return {!UpdateProviderInterface}
 */
export function getUpdateProvider() {
  // TODO(michaelcheco): Instantiate a real mojo interface here.
  assert(!!updateProvider);

  return updateProvider;
}
