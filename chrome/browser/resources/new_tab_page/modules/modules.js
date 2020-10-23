// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Registers all NTP modules given their respective descriptors.
 */

import {ChromeCartProxy} from './dummy/chrome_cart_proxy.js';
import {dummyDescriptor} from './dummy/module.js';
import {ModuleDescriptor} from './module_descriptor.js';
import {ModuleRegistry} from './module_registry.js';

/** @type {!Array<!ModuleDescriptor>} */
const descriptors = [];
loadModules();

async function loadModules() {
  const shouldShowModule =
      await ChromeCartProxy.getInstance().handler.shouldShowModule();
  if (shouldShowModule.shouldShow) {
    descriptors.push(dummyDescriptor);
  }

  ModuleRegistry.getInstance().registerModules(descriptors);
}
