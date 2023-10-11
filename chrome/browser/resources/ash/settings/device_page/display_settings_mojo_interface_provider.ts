// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from 'chrome://resources/js/assert.js';

import {DisplaySettingsProvider, DisplaySettingsProviderInterface} from '../mojom-webui/display_settings_provider.mojom-webui.js';

/**
 * @fileoverview
 * Provides singleton access to mojo interfaces with the ability
 * to override them with test/fake implementations.
 */

let displaySettingsProvider: DisplaySettingsProviderInterface|null;

export function getDisplaySettingsProvider(): DisplaySettingsProviderInterface {
  if (!displaySettingsProvider) {
    displaySettingsProvider = DisplaySettingsProvider.getRemote();
  }

  assert(displaySettingsProvider);
  return displaySettingsProvider;
}

export function setDisplaySettingsProviderForTesting(
    testProvider: DisplaySettingsProviderInterface): void {
  displaySettingsProvider = testProvider;
}
