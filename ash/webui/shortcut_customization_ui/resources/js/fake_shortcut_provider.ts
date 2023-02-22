// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {FakeMethodResolver} from 'chrome://resources/ash/common/fake_method_resolver.js';
import {FakeObservables} from 'chrome://resources/ash/common/fake_observables.js';
import {assert} from 'chrome://resources/js/assert_ts.js';

import {AcceleratorsUpdatedObserverRemote} from '../mojom-webui/ash/webui/shortcut_customization_ui/mojom/shortcut_customization.mojom-webui.js';

import {AcceleratorConfigResult, AcceleratorSource, MojoAcceleratorConfig, MojoLayoutInfo, ShortcutProviderInterface} from './shortcut_types.js';


/**
 * @fileoverview
 * Implements a fake version of the FakeShortcutProvider mojo interface.
 */

// Method names.
const ON_ACCELERATORS_UPDATED_METHOD_NAME =
    'AcceleratorsUpdatedObserver_OnAcceleratorsUpdated';

export class FakeShortcutProvider implements ShortcutProviderInterface {
  private methods: FakeMethodResolver;
  private observables: FakeObservables = new FakeObservables();
  private acceleratorsUpdatedRemote: AcceleratorsUpdatedObserverRemote|null =
      null;
  private acceleratorsUpdatedPromise: Promise<void>|null = null;

  constructor() {
    this.methods = new FakeMethodResolver();

    // Setup method resolvers.
    this.methods.register('getAccelerators');
    this.methods.register('getAcceleratorLayoutInfos');
    this.methods.register('isMutable');
    this.methods.register('addUserAccelerator');
    this.methods.register('replaceAccelerator');
    this.methods.register('removeAccelerator');
    this.methods.register('restoreAllDefaults');
    this.methods.register('restoreActionDefaults');
    this.methods.register('addObserver');
    this.registerObservables();
  }

  registerObservables(): void {
    this.observables.register(ON_ACCELERATORS_UPDATED_METHOD_NAME);
  }

  // Disable all observers and reset provider to initial state.
  reset(): void {
    this.observables = new FakeObservables();
    this.registerObservables();
  }

  getAcceleratorLayoutInfos(): Promise<{layoutInfos: MojoLayoutInfo[]}> {
    return this.methods.resolveMethod('getAcceleratorLayoutInfos');
  }

  getAccelerators(): Promise<{config: MojoAcceleratorConfig}> {
    return this.methods.resolveMethod('getAccelerators');
  }

  isMutable(source: AcceleratorSource): Promise<{isMutable: boolean}> {
    this.methods.setResult(
        'isMutable', {isMutable: source !== AcceleratorSource.kBrowser});
    return this.methods.resolveMethod('isMutable');
  }

  addObserver(observer: AcceleratorsUpdatedObserverRemote): void {
    this.acceleratorsUpdatedPromise = this.observe(
        ON_ACCELERATORS_UPDATED_METHOD_NAME,
        (config: MojoAcceleratorConfig) => {
          observer.onAcceleratorsUpdated(config);
        });
  }

  getAcceleratorsUpdatedPromiseForTesting(): Promise<void> {
    assert(this.acceleratorsUpdatedPromise);
    return this.acceleratorsUpdatedPromise;
  }

  // Set the value that will be retuned when `onAcceleratorsUpdated()` is
  // called.
  setFakeAcceleratorsUpdated(config: MojoAcceleratorConfig[]): void {
    this.observables.setObservableData(
        ON_ACCELERATORS_UPDATED_METHOD_NAME, config);
  }

  addUserAccelerator(): Promise<AcceleratorConfigResult> {
    // Always return kSuccess in this fake.
    this.methods.setResult(
        'addUserAccelerator', AcceleratorConfigResult.SUCCESS);
    return this.methods.resolveMethod('addUserAccelerator');
  }

  replaceAccelerator(): Promise<AcceleratorConfigResult> {
    // Always return kSuccess in this fake.
    this.methods.setResult(
        'replaceAccelerator', AcceleratorConfigResult.SUCCESS);
    return this.methods.resolveMethod('replaceAccelerator');
  }

  removeAccelerator(): Promise<AcceleratorConfigResult> {
    // Always return kSuccess in this fake.
    this.methods.setResult(
        'removeAccelerator', AcceleratorConfigResult.SUCCESS);
    return this.methods.resolveMethod('removeAccelerator');
  }

  restoreAllDefaults(): Promise<AcceleratorConfigResult> {
    // Always return kSuccess in this fake.
    this.methods.setResult(
        'restoreAllDefaults', AcceleratorConfigResult.SUCCESS);
    return this.methods.resolveMethod('restoreAllDefaults');
  }

  restoreActionDefaults(): Promise<AcceleratorConfigResult> {
    // Always return kSuccess in this fake.
    this.methods.setResult(
        'restoreActionDefaults', AcceleratorConfigResult.SUCCESS);
    return this.methods.resolveMethod('restoreActionDefaults');
  }

  /**
   * Sets the value that will be returned when calling
   * getAccelerators().
   */
  setFakeAcceleratorConfig(config: MojoAcceleratorConfig): void {
    this.methods.setResult('getAccelerators', {config});
  }

  /**
   * Sets the value that will be returned when calling
   * getAcceleratorLayoutInfos().
   */
  setFakeAcceleratorLayoutInfos(layoutInfos: MojoLayoutInfo[]): void {
    this.methods.setResult('getAcceleratorLayoutInfos', {layoutInfos});
  }

  // Sets up an observer for methodName.
  private observe(methodName: string, callback: (T: any) => void):
      Promise<void> {
    return new Promise((resolve) => {
      this.observables.observe(methodName, callback);
      resolve();
    });
  }
}
