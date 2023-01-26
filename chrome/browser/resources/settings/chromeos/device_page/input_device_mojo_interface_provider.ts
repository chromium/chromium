// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from 'chrome://resources/js/assert_ts.js';

import {fakeKeyboards, fakeMice, fakePointingSticks, fakeTouchpads} from './fake_input_device_data.js';
import {FakeInputDeviceSettingsProvider} from './fake_input_device_settings_provider.js';

/**
 * @fileoverview
 * Provides singleton access to mojo interfaces with the ability
 * to override them with test/fake implementations.
 */

let inputDeviceSettingsProvider: FakeInputDeviceSettingsProvider|null;

/**
 * Create a FakeInputDeviceSettingsProvider with reasonable fake data.
 */
export function setupFakeInputDeviceSettingsProvider(): void {
  inputDeviceSettingsProvider = new FakeInputDeviceSettingsProvider();
  inputDeviceSettingsProvider.setFakeKeyboards(fakeKeyboards);
  inputDeviceSettingsProvider.setFakeTouchpads(fakeTouchpads);
  inputDeviceSettingsProvider.setFakeMice(fakeMice);
  inputDeviceSettingsProvider.setFakePointingSticks(fakePointingSticks);
}

export function getInputDeviceSettingsProvider():
    FakeInputDeviceSettingsProvider {
  // TODO(yyhyyh@): After mojo bindings implemented, update situations for
  // getting the provider from remote and create a fake provider for testing.
  if (!inputDeviceSettingsProvider) {
    setupFakeInputDeviceSettingsProvider();
  }

  assert(!!inputDeviceSettingsProvider);
  return inputDeviceSettingsProvider;
}
