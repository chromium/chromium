// Copyright 2020 The Chromium Authors
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
import {feedDescriptor, feedV2Descriptor} from './feed/module.js';
import {HistoryClustersProxyImpl} from './history_clusters/history_clusters_proxy.js';
import {historyClustersDescriptor} from './history_clusters/module.js';
import {historyClustersV2Descriptor} from './history_clusters_v2/module.js';
import {ModuleDescriptor} from './module_descriptor.js';
import {ModuleRegistry} from './module_registry.js';
import {photosDescriptor} from './photos/module.js';
import {recipeTasksDescriptor} from './recipes/module.js';
import {recipeTasksDescriptor as recipeTasksV2Descriptor} from './recipes_v2/module.js';

const modulesRedesignedEnabled: boolean =
    loadTimeData.getBoolean('modulesRedesignedEnabled');
export const descriptors: ModuleDescriptor[] = [];
descriptors.push(
    modulesRedesignedEnabled ? recipeTasksV2Descriptor : recipeTasksDescriptor);
descriptors.push(
    modulesRedesignedEnabled ? chromeCartV2Descriptor : chromeCartDescriptor);
descriptors.push(
    modulesRedesignedEnabled ? driveV2Descriptor : driveDescriptor);
descriptors.push(photosDescriptor);
descriptors.push(modulesRedesignedEnabled ? feedV2Descriptor : feedDescriptor);
descriptors.push(
    modulesRedesignedEnabled ? historyClustersV2Descriptor :
                               historyClustersDescriptor);

// <if expr="not is_official_build">
if (modulesRedesignedEnabled) {
  descriptors.push(dummyV2Descriptor);
  descriptors.push(dummyV2Descriptor02);
  descriptors.push(dummyV2Descriptor03);
  descriptors.push(dummyV2Descriptor04);
  descriptors.push(dummyV2Descriptor05);
  descriptors.push(dummyV2Descriptor06);
  descriptors.push(dummyV2Descriptor07);
  descriptors.push(dummyV2Descriptor08);
  descriptors.push(dummyV2Descriptor09);
  descriptors.push(dummyV2Descriptor10);
  descriptors.push(dummyV2Descriptor11);
  descriptors.push(dummyV2Descriptor12);
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
      NewTabPageProxy.getInstance().handler.onModulesLoadedWithData(
          modules.map(module => module.descriptor.id));
    }
  }
  // Instantiate history clusters module if |historyClustersModuleEnabled| is
  // false to counterfactually log metrics about the coverage of the history
  // clusters module without rendering it.
  if (!loadTimeData.getBoolean('historyClustersModuleEnabled') &&
      loadTimeData.getBoolean('historyClustersModuleLoadEnabled')) {
    HistoryClustersProxyImpl.getInstance().handler.getClusters();
  }
}
counterfactualLoad();
