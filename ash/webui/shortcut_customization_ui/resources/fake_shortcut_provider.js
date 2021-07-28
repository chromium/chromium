// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {FakeMethodResolver} from 'chrome://resources/ash/common/fake_method_resolver.js';

import {AcceleratorConfig, ShortcutProviderInterface} from './shortcut_types.js';

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
  }

  /**
   * @return {!Promise<!AcceleratorConfig>}
   */
  getAllAcceleratorConfig() {
    return this.methods_.resolveMethod('getAllAcceleratorConfig');
  }

  /**
   * Sets the value that will be returned when calling
   * getAllAcceleratorConfig().
   * @param {!AcceleratorConfig} config
   */
  setFakeAcceleratorConfig(config) {
    this.methods_.setResult('getAllAcceleratorConfig', config);
  }
}
