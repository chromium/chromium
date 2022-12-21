// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from 'chrome://resources/js/assert_ts.js';

import {AcceleratorConfigurationProvider, AcceleratorConfigurationProviderRemote, AcceleratorsUpdatedObserverRemote} from '../mojom-webui/ash/webui/shortcut_customization_ui/mojom/shortcut_customization.mojom-webui.js';

import {fakeAcceleratorConfig, fakeLayoutInfo} from './fake_data.js';
import {FakeShortcutProvider} from './fake_shortcut_provider.js';
import {Accelerator, AcceleratorConfigResult, AcceleratorSource, MojoAcceleratorConfig, MojoLayoutInfo, ShortcutProviderInterface} from './shortcut_types.js';



/**
 * @fileoverview
 * Provides singleton access to mojo interfaces with the ability
 * to override them with test/fake implementations.
 */

let shortcutProvider: ShortcutProviderInterface|null = null;

/**
 * When true, this variable forces the app to use the fake provider.
 * This variable is intended to be manually set by developers for the
 * purposes of debugging.
 */
const useFakeProvider: boolean = false;

export function setShortcutProviderForTesting(
    testProvider: ShortcutProviderInterface): void {
  shortcutProvider = testProvider;
}

/**
 * Sets up a FakeShortcutProvider to be used at runtime.
 * TODO(zentaro): Remove once mojo bindings are implemented.
 */
export function setupFakeShortcutProvider(): ShortcutProviderInterface {
  // Create provider.
  const provider = new FakeShortcutProvider();

  // Setup accelerator config.
  provider.setFakeAcceleratorConfig(fakeAcceleratorConfig);

  // Setup accelerator layout info.
  provider.setFakeAcceleratorLayoutInfos(fakeLayoutInfo);

  // Set the fake provider.
  setShortcutProviderForTesting(provider);

  return provider;
}

/**
 * This wrapper is used to bridge the gap from the fake provider to the
 * real provider until all methods are implemented.
 * TODO(cambickel): Remove once all mojo bindings are implemented.
 */
export class ShortcutProviderWrapper implements ShortcutProviderInterface {
  private remote: AcceleratorConfigurationProviderRemote;
  private fakeProvider: ShortcutProviderInterface;

  constructor(fakeProvider: ShortcutProviderInterface) {
    this.remote = AcceleratorConfigurationProvider.getRemote();
    this.fakeProvider = fakeProvider;
  }

  getAcceleratorLayoutInfos(): Promise<{layoutInfos: MojoLayoutInfo[]}> {
    return this.remote.getAcceleratorLayoutInfos();
  }

  getAccelerators(): Promise<{config: MojoAcceleratorConfig}> {
    return this.remote.getAccelerators();
  }

  isMutable(source: AcceleratorSource): Promise<{isMutable: boolean}> {
    return this.remote.isMutable(source);
  }

  removeAccelerator(
      source: AcceleratorSource, action: number,
      accelerator: Accelerator): Promise<AcceleratorConfigResult> {
    // TODO(cambickel) Replace with real mojo method.
    return this.fakeProvider.removeAccelerator(source, action, accelerator);
  }

  replaceAccelerator(
      source: AcceleratorSource, action: number, oldAccelerator: Accelerator,
      newAccelerator: Accelerator): Promise<AcceleratorConfigResult> {
    // TODO(cambickel) Replace with real mojo method.
    return this.fakeProvider.replaceAccelerator(
        source, action, oldAccelerator, newAccelerator);
  }

  addUserAccelerator(
      source: AcceleratorSource, action: number,
      accelerator: Accelerator): Promise<AcceleratorConfigResult> {
    // TODO(cambickel) Replace with real mojo method.
    return this.fakeProvider.addUserAccelerator(source, action, accelerator);
  }

  addObserver(observer: AcceleratorsUpdatedObserverRemote): void {
    return this.remote.addObserver(observer);
  }
}

export function getShortcutProvider(): ShortcutProviderInterface {
  if (!shortcutProvider) {
    const fakeProvider: ShortcutProviderInterface = setupFakeShortcutProvider();
    if (useFakeProvider) {
      setShortcutProviderForTesting(fakeProvider);
    } else {
      shortcutProvider = new ShortcutProviderWrapper(fakeProvider);
    }
  }

  assert(!!shortcutProvider);
  return shortcutProvider;
}
