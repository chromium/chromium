// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Registers all NTP modules given their respective descriptors.
 */

import {loadTimeData} from '../i18n_setup.js';

import {chromeCartDescriptor} from './cart/module.js';
import {chromeCartDescriptor as chromeCartV2Descriptor} from './cart_v2/module.js';
import {driveDescriptor} from './drive/module.js';
import {driveDescriptor as driveV2Descriptor} from './drive_v2/module.js';
// <if expr="not is_official_build">
import {dummyDescriptor, dummyDescriptor2} from './dummy/module.js';
// </if>
import {ModuleDescriptor} from './module_descriptor.js';
import {photosDescriptor} from './photos/module.js';
import {recipeTasksDescriptor, shoppingTasksDescriptor} from './task_module/module.js';
import {recipeTasksDescriptor as recipeTasksV2Descriptor} from './recipes_v2/module.js';

/** @type {!Array<!ModuleDescriptor>} */
export const descriptors = [];

if (loadTimeData.getBoolean('shoppingTasksModuleEnabled')) {
  descriptors.push(shoppingTasksDescriptor);
}

if (loadTimeData.getBoolean('recipeTasksModuleEnabled')) {
  if (loadTimeData.getBoolean('modulesRedesignedEnabled')) {
    descriptors.push(recipeTasksV2Descriptor);
  } else {
    descriptors.push(recipeTasksDescriptor);
  }
}

if (loadTimeData.getBoolean('chromeCartModuleEnabled')) {
  if (loadTimeData.getBoolean('modulesRedesignedEnabled')) {
    descriptors.push(chromeCartV2Descriptor);
  } else {
    descriptors.push(chromeCartDescriptor);
  }
}

if (loadTimeData.getBoolean('driveModuleEnabled')) {
  if (loadTimeData.getBoolean('modulesRedesignedEnabled')) {
    descriptors.push(driveV2Descriptor);
  } else {
    descriptors.push(driveDescriptor);
  }
}

if (loadTimeData.getBoolean('photosModuleEnabled')) {
  descriptors.push(photosDescriptor);
}

// <if expr="not is_official_build">
descriptors.push(dummyDescriptor);
descriptors.push(dummyDescriptor2);
// </if>
