// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from 'chrome://resources/js/assert_ts.js';

import {fakeAcceleratorConfig, fakeLayoutInfo} from './fake_data.js';
import {FakeShortcutProvider} from './fake_shortcut_provider.js';
import {ShortcutProviderInterface} from './shortcut_types.js';

/**
 * @fileoverview
 * Provides singleton access to mojo interfaces with the ability
 * to override them with test/fake implementations.
 */

let shortcutProvider: ShortcutProviderInterface|null = null;

export function setShortcutProviderForTesting(
    testProvider: ShortcutProviderInterface) {
  shortcutProvider = testProvider;
}

/**
 * Sets up a FakeShortcutProvider to be used at runtime.
 * TODO(zentaro): Remove once mojo bindings are implemented.
 */
function setupFakeShortcutProvider() {
  // Create provider.
  const provider = new FakeShortcutProvider();

  // Setup accelerator config.
  provider.setFakeAcceleratorConfig(fakeAcceleratorConfig);

  // Setup accelerator layout info.
  provider.setFakeLayoutInfo(fakeLayoutInfo);

  // Set the fake provider.
  setShortcutProviderForTesting(provider);
}

export function getShortcutProvider(): ShortcutProviderInterface {
  if (!shortcutProvider) {
    // TODO(zentaro): Instantiate a real mojo interface here.
    setupFakeShortcutProvider();
  }

  assert(!!shortcutProvider);
  return shortcutProvider;
}
