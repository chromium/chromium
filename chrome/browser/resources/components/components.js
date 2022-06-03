// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './strings.m.js';

import {addWebUIListener, isChromeOS, sendWithPromise} from 'chrome://resources/js/cr.m.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {$} from 'chrome://resources/js/util.m.js';

/**
 * An array of the latest component data including ID, name, status and
 * version. This is populated in returnComponentsData() for the convenience of
 * tests.
 */
let currentComponentsData = null;

/**
 * Takes the |componentsData| input argument which represents data about the
 * currently installed components and populates the html jstemplate with
 * that data. It expects an object structure like the above.
 * @param {Object} componentsData Detailed info about installed components.
 *      Same expected format as returnComponentsData().
 */
function renderTemplate(componentsData) {
  // This is the javascript code that processes the template:
  const input = new JsEvalContext(componentsData);
  const output = $('component-template').cloneNode(true);
  $('component-placeholder').innerHTML = trustedTypes.emptyHTML;
  $('component-placeholder').appendChild(output);
  jstProcess(input, output);
  output.removeAttribute('hidden');
}

/**
 * Asks the C++ ComponentsDOMHandler to get details about the installed
 * components.
 */
function requestComponentsData() {
  sendWithPromise('requestComponentsData').then(returnComponentsData);
}

/**
 * Called by the WebUI to re-populate the page with data representing the
 * current state of installed components. The componentsData will also be
 * stored in currentComponentsData to be available to JS for testing purposes.
 * @param {{
 *   components: !Array<!{name: string, version: string}>
 * }} componentsData Detailed info about installed components. The template
 * expects each component's format to match the following structure to correctly
 *     populate the page:
 *   {
 *     components: [
 *       {
 *          name: 'Component1',
 *          version: '1.2.3',
 *       },
 *       {
 *          name: 'Component2',
 *          version: '4.5.6',
 *       },
 *     ]
 *   }
 */
function returnComponentsData(componentsData) {
  const bodyContainer = $('body-container');
  const body = document.body;

  bodyContainer.style.visibility = 'hidden';
  body.className = '';

  // Initialize |currentComponentsData|, which can also be updated in
  // onComponentEvent() later.
  currentComponentsData = componentsData.components;

  renderTemplate(componentsData);

  // Add handlers to dynamically created HTML elements.
  const links = document.getElementsByClassName('button-check-update');
  for (let i = 0; i < links.length; i++) {
    links[i].onclick = function(e) {
      handleCheckUpdate(this);
      e.preventDefault();
    };
  }

  // Disable some controls for Guest mode in ChromeOS.
  if (isChromeOS && loadTimeData.getBoolean('isGuest')) {
    document.querySelectorAll('[guest-disabled]').forEach(function(element) {
      element.disabled = true;
    });
  }

  bodyContainer.style.visibility = 'visible';
  body.className = 'show-tmi-mode-initial';
}

/**
 * Listener called when state of component updater service changes.
 * @param {Object} eventArgs Contains event and component ID. Component ID is
 * optional.
 */
function onComponentEvent(eventArgs) {
  if (!eventArgs['id']) {
    return;
  }

  const id = eventArgs['id'];

  const filteredComponents = currentComponentsData.filter(function(entry) {
    return entry.id === id;
  });

  // A component may be added from another page so the status and version
  // should only be updated if the component is listed on this page.
  if (filteredComponents.length === 0) {
    return;
  }

  const component = filteredComponents[0];

  const status = eventArgs['event'];
  $('status-' + id).textContent = status;
  component['status'] = status;

  if (eventArgs['version']) {
    const version = eventArgs['version'];
    $('version-' + id).textContent = version;
    component['version'] = version;
  }
}

/**
 * Handles an 'enable' or 'disable' button getting clicked.
 * @param {HTMLElement} node The HTML element representing the component
 *     being checked for update.
 */
function handleCheckUpdate(node) {
  $('status-' + String(node.id)).textContent =
      loadTimeData.getString('checkingLabel');

  // Tell the C++ ComponentssDOMHandler to check for update.
  chrome.send('checkUpdate', [String(node.id)]);
}

// Get data and have it displayed upon loading.
document.addEventListener('DOMContentLoaded', function() {
  addWebUIListener('component-event', onComponentEvent);
  requestComponentsData();
});
