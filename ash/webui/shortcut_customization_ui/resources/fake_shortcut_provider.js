// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {FakeMethodResolver} from 'chrome://resources/ash/common/fake_method_resolver.js';

import {AcceleratorConfig, AcceleratorConfigResult, AcceleratorKeys, AcceleratorSource, LayoutInfoList, ShortcutProviderInterface} from './shortcut_types.js';

/**
 * @fileoverview
 * Implements a fake version of the FakeShortcutProvider mojo interface.
 */

/** @implements {ShortcutProviderInterface} */
export class FakeShortcutProvider {
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

  /**
   * @return {!Promise<!AcceleratorConfig>}
   */
  getAllAcceleratorConfig() {
    return this.methods_.resolveMethod('getAllAcceleratorConfig');
  }

  /**
   * @return {!Promise<!LayoutInfoList>}
   */
  getLayoutInfo() {
    return this.methods_.resolveMethod('getLayoutInfo');
  }

  /**
   * @param {!AcceleratorSource} source
   * @return {!Promise<boolean>}
   */
  isMutable(source) {
    this.methods_.setResult('isMutable', source !== AcceleratorSource.kBrowser);
    return this.methods_.resolveMethod('isMutable');
  }

  /**
   * @param {AcceleratorSource} source
   * @param {number} action
   * @param {!AcceleratorKeys} accelerator
   */
  addUserAccelerator(source, action, accelerator) {
    // Always return kSuccess in this fake.
    this.methods_.setResult(
        'addUserAccelerator', AcceleratorConfigResult.kSuccess);
    return this.methods_.resolveMethod('addUserAccelerator');
  }

  /**
   * @param {AcceleratorSource} source
   * @param {number} action
   * @param {!AcceleratorKeys} oldAccelerator
   * @param {!AcceleratorKeys} newAccelerator
   */
  replaceAccelerator(source, action, oldAccelerator, newAccelerator) {
    // Always return kSuccess in this fake.
    this.methods_.setResult(
        'replaceAccelerator', AcceleratorConfigResult.kSuccess);
    return this.methods_.resolveMethod('replaceAccelerator');
  }

  /**
   * @param {!AcceleratorSource} source
   * @param {number} action
   * @param {!AcceleratorKeys} accelerator
   */
  removeAccelerator(source, action, accelerator) {
    // Always return kSuccess in this fake.
    this.methods_.setResult(
        'removeAccelerator', AcceleratorConfigResult.kSuccess);
    return this.methods_.resolveMethod('removeAccelerator');
  }

  restoreAllDefaults() {
    // Always return kSuccess in this fake.
    this.methods_.setResult(
        'restoreAllDefaults', AcceleratorConfigResult.kSuccess);
    return this.methods_.resolveMethod('restoreAllDefaults');
  }

  /**
   * @param {!AcceleratorSource} source
   * @param {number} action
   */
  restoreActionDefaults(source, action) {
    // Always return kSuccess in this fake.
    this.methods_.setResult(
        'restoreActionDefaults', AcceleratorConfigResult.kSuccess);
    return this.methods_.resolveMethod('restoreActionDefaults');
  }

  /**
   * Sets the value that will be returned when calling
   * getAllAcceleratorConfig().
   * @param {!AcceleratorConfig} config
   */
  setFakeAcceleratorConfig(config) {
    this.methods_.setResult('getAllAcceleratorConfig', config);
  }

  /**
   * Sets the value that will be returned when calling
   * getLayoutInfo().
   * @param {!LayoutInfoList} layout
   */
  setFakeLayoutInfo(layout) {
    this.methods_.setResult('getLayoutInfo', layout);
  }
}
