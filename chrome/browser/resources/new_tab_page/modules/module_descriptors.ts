// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Registers all NTP modules given their respective descriptors.
 */

import {loadTimeData} from '../i18n_setup.js';
import {NewTabPageProxy} from '../new_tab_page_proxy.js';

import {chromeCartDescriptor} from './cart/module.js';
import {driveDescriptor} from './drive/module.js';
import {feedDescriptor} from './feed/module.js';
import {HistoryClustersProxyImpl} from './history_clusters/history_clusters_proxy.js';
import {historyClustersDescriptor} from './history_clusters/module.js';
import type {ModuleDescriptor} from './module_descriptor.js';
import {ModuleRegistry} from './module_registry.js';
import {photosDescriptor} from './photos/module.js';
import {googleCalendarDescriptor} from './v2/calendar/google_calendar_module.js';
import {outlookCalendarDescriptor} from './v2/calendar/outlook_calendar_module.js';
// <if expr="not is_official_build">
import {dummyV2Descriptor} from './v2/dummy/module.js';
// </if>
import {fileSuggestionDescriptor} from './v2/file_suggestion/module.js';
import {historyClustersDescriptor as historyClustersV2Descriptor} from './v2/history_clusters/module.js';
import {mostRelevantTabResumptionDescriptor} from './v2/most_relevant_tab_resumption/module.js';
import {tabResumptionDescriptor} from './v2/tab_resumption/module.js';

const modulesRedesignedEnabled: boolean =
    loadTimeData.getBoolean('modulesRedesignedEnabled');
export const descriptors: ModuleDescriptor[] = [];
descriptors.push(chromeCartDescriptor);
descriptors.push(
    modulesRedesignedEnabled ? fileSuggestionDescriptor : driveDescriptor);
descriptors.push(photosDescriptor);
descriptors.push(feedDescriptor);
descriptors.push(
    modulesRedesignedEnabled ? historyClustersV2Descriptor :
                               historyClustersDescriptor);
if (loadTimeData.getBoolean('mostRelevantTabResumptionEnabled')) {
  descriptors.push(mostRelevantTabResumptionDescriptor);
} else {
  descriptors.push(tabResumptionDescriptor);
}
descriptors.push(googleCalendarDescriptor);
descriptors.push(outlookCalendarDescriptor);

// <if expr="not is_official_build">
if (modulesRedesignedEnabled) {
  descriptors.push(dummyV2Descriptor);
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
