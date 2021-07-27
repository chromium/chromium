// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from 'chrome://resources/js/assert.m.js';
import {ShortcutProviderInterface} from './shortcut_types.js'

/**
 * @fileoverview
 * Provides singleton access to mojo interfaces with the ability
 * to override them with test/fake implementations.
 */

/**
 * @type {?ShortcutProviderInterface}
 */
let shortcutProvider = null;

/**
 * @param {!ShortcutProviderInterface} testProvider
 */
export function setShortcutProviderForTesting(testProvider) {
  shortcutProvider = testProvider;
}

/**
 * @return {!ShortcutProviderInterface}
 */
export function getShortcutProvider() {
  // TODO(zentaro): Instantiate a real mojo interface here.
  assert(!!shortcutProvider);

  return shortcutProvider;
}
