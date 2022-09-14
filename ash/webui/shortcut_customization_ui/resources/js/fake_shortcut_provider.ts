// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {FakeMethodResolver} from 'chrome://resources/ash/common/fake_method_resolver.js';

import {AcceleratorConfig, AcceleratorConfigResult, AcceleratorSource, LayoutInfoList, ShortcutProviderInterface} from './shortcut_types.js';

/**
 * @fileoverview
 * Implements a fake version of the FakeShortcutProvider mojo interface.
 */

export class FakeShortcutProvider implements ShortcutProviderInterface {
  private methods_: FakeMethodResolver;

  constructor() {
    this.methods_ = new FakeMethodResolver();

    // Setup method resolvers.
    this.methods_.register('getAllAcceleratorConfig');
    this.methods_.register('getLayoutInfo');
    this.methods_.register('isMutable');
    this.methods_.register('addUserAccelerator');
    this.methods_.register('replaceAccelerator');
    this.methods_.register('removeAccelerator');
    this.methods_.register('restoreAllDefaults');
    this.methods_.register('restoreActionDefaults');
  }

  getAllAcceleratorConfig(): Promise<AcceleratorConfig> {
    return this.methods_.resolveMethod('getAllAcceleratorConfig');
  }

  getLayoutInfo(): Promise<LayoutInfoList> {
    return this.methods_.resolveMethod('getLayoutInfo');
  }

  isMutable(source: AcceleratorSource): Promise<boolean> {
    this.methods_.setResult('isMutable', source !== AcceleratorSource.BROWSER);
    return this.methods_.resolveMethod('isMutable');
  }

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
   * getAllAcceleratorConfig().
   */
  setFakeAcceleratorConfig(config: AcceleratorConfig) {
    this.methods_.setResult('getAllAcceleratorConfig', config);
  }

  /**
   * Sets the value that will be returned when calling
   * getLayoutInfo().
   */
  setFakeLayoutInfo(layout: LayoutInfoList) {
    this.methods_.setResult('getLayoutInfo', layout);
  }
}
