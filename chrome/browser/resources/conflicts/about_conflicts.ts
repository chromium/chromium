// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {assert} from 'chrome://resources/js/assert.js';
import {sendWithPromise} from 'chrome://resources/js/cr.js';
import {getRequiredElement} from 'chrome://resources/js/util.js';

interface ModuleData {
  code_id: string;
  description: string;
  digital_signer: string;
  location: string;
  name: string;
  process_types: string;
  type_description: string;
  version: string;
  third_party_module_status: string;
}

interface ModuleListData {
  moduleCount: number;
  moduleList: ModuleData[];
  thirdPartyFeatureStatus: string;
  thirdPartyFeatureEnabled: boolean;
  hasModules: boolean;
}

type DomBindElement = HTMLElement&{data: ModuleListData};

/**
 * Takes the |moduleListData| input argument which represents data about
 * the currently available modules and populates the HTML template
 * with that data. It expects an object structure like the above.
 */
function renderTemplate(moduleListData: ModuleListData) {
  const bind = document.body.querySelector<DomBindElement>('dom-bind');
  assert(bind);

  moduleListData.hasModules = moduleListData.moduleList.length > 0;
  bind.data = moduleListData;
}

/**
 * Filters list of displayed modules to those listed in the process types
 * specified in the url fragment. For instance, chrome://conflicts/#r will show
 * only those modules that have loaded into a renderer.
 */
function filterModuleListData() {
  const filter = window.location.hash.substr(1).toLowerCase();
  const modules = document.body.querySelectorAll<HTMLElement>('.module');

  // Loop through all modules, and hide all that don't match the filter.
  for (const module of modules) {
    module.style.display =
        module.dataset['process']!.toLowerCase().includes(filter) ? '' : 'none';
  }
}

/**
 * Called by the WebUI to re-populate the page with data representing the
 * current state of installed modules.
 */
function returnModuleList(moduleListData: ModuleListData) {
  renderTemplate(moduleListData);
  if (window.location.hash.length > 1) {
    filterModuleListData();
  }
  getRequiredElement('loading-message').style.visibility = 'hidden';
  getRequiredElement('body-container').style.visibility = 'visible';
}

// Get data and have it displayed upon loading.
document.addEventListener('DOMContentLoaded', () => {
  // Ask the C++ ConflictsHandler to get details about the available modules
  // and return detailed data about the configuration.
  sendWithPromise('requestModuleList').then(returnModuleList);
  window.addEventListener('hashchange', filterModuleListData, false);
});
