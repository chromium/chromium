// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Registers all NTP modules given their respective descriptors.
 */

import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';

import {chromeCartDescriptor} from './cart/module.js';
import {driveDescriptor} from './drive/module.js';
// <if expr="not is_official_build">
import {dummyDescriptor, dummyDescriptor2} from './dummy/module.js';
// </if>
import {ModuleDescriptor} from './module_descriptor.js';
import {ModuleRegistry} from './module_registry.js';
import {recipeTasksDescriptor, shoppingTasksDescriptor} from './task_module/module.js';

/** @type {!Array<!ModuleDescriptor>} */
const descriptors = [];

if (loadTimeData.getBoolean('shoppingTasksModuleEnabled')) {
  descriptors.push(shoppingTasksDescriptor);
}

if (loadTimeData.getBoolean('recipeTasksModuleEnabled')) {
  descriptors.push(recipeTasksDescriptor);
}

if (loadTimeData.getBoolean('chromeCartModuleEnabled')) {
  descriptors.push(chromeCartDescriptor);
}

if (loadTimeData.getBoolean('driveModuleEnabled')) {
  descriptors.push(driveDescriptor);
}

// <if expr="not is_official_build">
descriptors.push(dummyDescriptor);
descriptors.push(dummyDescriptor2);
// </if>

ModuleRegistry.getInstance().registerModules(descriptors);
