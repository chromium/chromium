// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Registers all NTP modules given their respective descriptors.
 */

import {loadTimeData} from '../i18n_setup.js';
import {NewTabPageProxy} from '../new_tab_page_proxy.js';

import {chromeCartDescriptor} from './cart/module.js';
import {chromeCartDescriptor as chromeCartV2Descriptor} from './cart_v2/module.js';
import {driveDescriptor} from './drive/module.js';
import {driveDescriptor as driveV2Descriptor} from './drive_v2/module.js';
// <if expr="not is_official_build">
import {dummyV2Descriptor, dummyV2Descriptor02, dummyV2Descriptor03, dummyV2Descriptor04, dummyV2Descriptor05, dummyV2Descriptor06, dummyV2Descriptor07, dummyV2Descriptor08, dummyV2Descriptor09, dummyV2Descriptor10, dummyV2Descriptor11, dummyV2Descriptor12} from './dummy_v2/module.js';
// </if>
import {ModuleDescriptor, ModuleDescriptorV2} from './module_descriptor.js';
import {ModuleRegistry} from './module_registry.js';
import {photosDescriptor} from './photos/module.js';
import {recipeTasksDescriptor as recipeTasksV2Descriptor} from './recipes_v2/module.js';
import {recipeTasksDescriptor, shoppingTasksDescriptor} from './task_module/module.js';

export const descriptors: ModuleDescriptor[] = [];

export const descriptorsV2: ModuleDescriptorV2[] = [];

if (loadTimeData.getBoolean('shoppingTasksModuleEnabled')) {
  descriptors.push(shoppingTasksDescriptor);
}

if (loadTimeData.getBoolean('recipeTasksModuleEnabled')) {
  if (loadTimeData.getBoolean('modulesRedesignedEnabled')) {
    descriptorsV2.push(recipeTasksV2Descriptor);
  } else {
    descriptors.push(recipeTasksDescriptor);
  }
}

if (loadTimeData.getBoolean('chromeCartModuleEnabled')) {
  if (loadTimeData.getBoolean('modulesRedesignedEnabled')) {
    descriptorsV2.push(chromeCartV2Descriptor);
  } else {
    descriptors.push(chromeCartDescriptor);
  }
}

if (loadTimeData.getBoolean('driveModuleEnabled')) {
  if (loadTimeData.getBoolean('modulesRedesignedEnabled')) {
    descriptorsV2.push(driveV2Descriptor);
  } else {
    descriptors.push(driveDescriptor);
  }
}

if (loadTimeData.getBoolean('photosModuleEnabled')) {
  descriptors.push(photosDescriptor);
}

// <if expr="not is_official_build">
if (loadTimeData.getBoolean('dummyModulesEnabled')) {
  if (loadTimeData.getBoolean('modulesRedesignedEnabled')) {
    descriptorsV2.push(dummyV2Descriptor);
    descriptorsV2.push(dummyV2Descriptor02);
    descriptorsV2.push(dummyV2Descriptor03);
    descriptorsV2.push(dummyV2Descriptor04);
    descriptorsV2.push(dummyV2Descriptor05);
    descriptorsV2.push(dummyV2Descriptor06);
    descriptorsV2.push(dummyV2Descriptor07);
    descriptorsV2.push(dummyV2Descriptor08);
    descriptorsV2.push(dummyV2Descriptor09);
    descriptorsV2.push(dummyV2Descriptor10);
    descriptorsV2.push(dummyV2Descriptor11);
    descriptorsV2.push(dummyV2Descriptor12);
  }
}
// </if>

export async function counterfactualLoad() {
  // Instantiate modules even if |modulesEnabled| is false to counterfactually
  // trigger a HaTS survey in a potential control group.
  if (!loadTimeData.getBoolean('modulesEnabled') &&
      loadTimeData.getBoolean('modulesLoadEnabled')) {
    const modules = await ModuleRegistry.getInstance().initializeModules(
        loadTimeData.getInteger('modulesLoadTimeout'));
    if (modules) {
      NewTabPageProxy.getInstance().handler.onModulesLoadedWithData();
    }
  }
}
counterfactualLoad();
