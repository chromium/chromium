// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * This variable structure is here to document the structure that the template
 * expects to correctly populate the page.
 */
const moduleListDataFormat = {
  'moduleList': [{
    'type_description':
        'The type of module (string), defaults to blank for regular modules',
    'location': 'The module path, not including filename',
    'name': 'The name of the module',
    'product_name': 'The name of the product the module belongs to',
    'description': 'The module description',
    'version': 'The module version',
    'digital_signer': 'The signer of the digital certificate for the module',
    'code_id': 'The code id of the module',
    'third_party_module_status': 'The module status'
  }]
};

/**
 * Takes the |moduleListData| input argument which represents data about
 * the currently available modules and populates the html jstemplate
 * with that data. It expects an object structure like the above.
 * @param {Object} moduleListData Information about available modules.
 */
function renderTemplate(moduleListData) {
  // This is the javascript code that processes the template:
  const input = new JsEvalContext(moduleListData);
  const output = $('modulesTemplate');
  jstProcess(input, output);
}

/**
 * Asks the C++ ConflictsHandler to get details about the available modules
 * and return detailed data about the configuration.
 */
function requestModuleListData() {
  cr.sendWithPromise('requestModuleList').then(returnModuleList);
}

/**
 * Filters list of displayed modules to those listed in the process types
 * specified in the url fragment. For instance, chrome://conflicts/#r will show
 * only those modules that have loaded into a renderer.
 */
function filterModuleListData() {
  const filter = window.location.hash.substr(1).toLowerCase();
  const modules = document.getElementsByClassName('module');

  // Loop through all modules, and hide all that don't match the filter.
  for (i = 0; i < modules.length; ++i) {
    modules[i].style.display =
        modules[i].dataset.process.includes(filter) ? '' : 'none';
  }
}

/**
 * Called by the WebUI to re-populate the page with data representing the
 * current state of installed modules.
 * @param {Object} moduleListData Information about available modules.
 */
function returnModuleList(moduleListData) {
  renderTemplate(moduleListData);
  if (window.location.hash.length > 1) {
    filterModuleListData();
  }
  $('loading-message').style.visibility = 'hidden';
  $('body-container').style.visibility = 'visible';
}

// Get data and have it displayed upon loading.
document.addEventListener('DOMContentLoaded', requestModuleListData);
window.addEventListener('hashchange', filterModuleListData, false);
