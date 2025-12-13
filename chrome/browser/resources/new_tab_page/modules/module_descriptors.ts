// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Registers all NTP modules given their respective descriptors.
 */

import {loadTimeData} from '../i18n_setup.js';
import {NewTabPageProxy} from '../new_tab_page_proxy.js';

import {microsoftAuthModuleDescriptor} from './authentication/microsoft_auth_module.js';
import {googleCalendarDescriptor} from './calendar/google_calendar_module.js';
import {outlookCalendarDescriptor} from './calendar/outlook_calendar_module.js';
// <if expr="not is_official_build">
import {dummyV2Descriptor} from './dummy/module.js';
// </if>
import {driveModuleDescriptor} from './file_suggestion/drive_module.js';
import {microsoftFilesModuleDescriptor} from './file_suggestion/microsoft_files_module.js';
import type {ModuleDescriptor} from './module_descriptor.js';
import {ModuleRegistry} from './module_registry.js';
import {mostRelevantTabResumptionDescriptor} from './most_relevant_tab_resumption/module.js';
import {tabGroupsDescriptor} from './tab_groups/module.js';

export const descriptors: ModuleDescriptor[] = [];
descriptors.push(mostRelevantTabResumptionDescriptor);
descriptors.push(driveModuleDescriptor);
descriptors.push(googleCalendarDescriptor);
descriptors.push(microsoftAuthModuleDescriptor);
descriptors.push(outlookCalendarDescriptor);
descriptors.push(microsoftFilesModuleDescriptor);
descriptors.push(tabGroupsDescriptor);

// <if expr="not is_official_build">
descriptors.push(dummyV2Descriptor);
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
}
counterfactualLoad();
