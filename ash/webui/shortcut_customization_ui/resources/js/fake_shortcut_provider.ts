// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {FakeMethodResolver} from 'chrome://resources/ash/common/fake_method_resolver.js';

import {AcceleratorConfigResult, AcceleratorSource, MojoAcceleratorConfig, MojoLayoutInfo, ShortcutProviderInterface} from './shortcut_types.js';


/**
 * @fileoverview
 * Implements a fake version of the FakeShortcutProvider mojo interface.
 */

export class FakeShortcutProvider implements ShortcutProviderInterface {
  private methods_: FakeMethodResolver;

  constructor() {
    this.methods_ = new FakeMethodResolver();

    // Setup method resolvers.
    this.methods_.register('getAccelerators');
    this.methods_.register('getAcceleratorLayoutInfos');
    this.methods_.register('isMutable');
    this.methods_.register('addUserAccelerator');
    this.methods_.register('replaceAccelerator');
    this.methods_.register('removeAccelerator');
    this.methods_.register('restoreAllDefaults');
    this.methods_.register('restoreActionDefaults');
  }

  getAcceleratorLayoutInfos(): Promise<{layoutInfos: MojoLayoutInfo[]}> {
    return this.methods_.resolveMethod('getAcceleratorLayoutInfos');
  }

  getAccelerators(): Promise<{config: MojoAcceleratorConfig}> {
    return this.methods_.resolveMethod('getAccelerators');
  }

  isMutable(source: AcceleratorSource): Promise<{isMutable: boolean}> {
    this.methods_.setResult(
        'isMutable', {isMutable: source !== AcceleratorSource.kBrowser});
    return this.methods_.resolveMethod('isMutable');
  }

  // Return nothing because this method has a void return type.
  addObserver(): void {}

  addUserAccelerator(): Promise<AcceleratorConfigResult> {
    // Always return kSuccess in this fake.
    this.methods_.setResult(
        'addUserAccelerator', AcceleratorConfigResult.SUCCESS);
    return this.methods_.resolveMethod('addUserAccelerator');
  }

  replaceAccelerator(): Promise<AcceleratorConfigResult> {
    // Always return kSuccess in this fake.
    this.methods_.setResult(
        'replaceAccelerator', AcceleratorConfigResult.SUCCESS);
    return this.methods_.resolveMethod('replaceAccelerator');
  }

  removeAccelerator(): Promise<AcceleratorConfigResult> {
    // Always return kSuccess in this fake.
    this.methods_.setResult(
        'removeAccelerator', AcceleratorConfigResult.SUCCESS);
    return this.methods_.resolveMethod('removeAccelerator');
  }

  restoreAllDefaults(): Promise<AcceleratorConfigResult> {
    // Always return kSuccess in this fake.
    this.methods_.setResult(
        'restoreAllDefaults', AcceleratorConfigResult.SUCCESS);
    return this.methods_.resolveMethod('restoreAllDefaults');
  }

  restoreActionDefaults(): Promise<AcceleratorConfigResult> {
    // Always return kSuccess in this fake.
    this.methods_.setResult(
        'restoreActionDefaults', AcceleratorConfigResult.SUCCESS);
    return this.methods_.resolveMethod('restoreActionDefaults');
  }

  /**
   * Sets the value that will be returned when calling
   * getAccelerators().
   */
  setFakeAcceleratorConfig(config: MojoAcceleratorConfig) {
    this.methods_.setResult('getAccelerators', {config});
  }

  /**
   * Sets the value that will be returned when calling
   * getAcceleratorLayoutInfos().
   */
  setFakeAcceleratorLayoutInfos(layoutInfos: MojoLayoutInfo[]) {
    this.methods_.setResult('getAcceleratorLayoutInfos', {layoutInfos});
  }
}
