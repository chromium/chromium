// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {FakeMethodResolver} from 'chrome://resources/ash/common/fake_method_resolver.js';

import {AcceleratorConfig, AcceleratorSource, LayoutInfoList, ShortcutProviderInterface} from './shortcut_types.js';

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
