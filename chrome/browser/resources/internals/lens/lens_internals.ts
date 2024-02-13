// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {getRequiredElement} from 'chrome://resources/js/util.js';

import type {LensInternalsBrowserProxy} from './lens_internals_browser_proxy.js';
import {LensInternalsBrowserProxyImpl} from './lens_internals_browser_proxy.js';

/** @param showEnableButton Whether to show the "start" button. */
function toggleDebugModeButton(showEnableButton: boolean) {
  getRequiredElement('start-debug-mode')
      .toggleAttribute('hidden', !showEnableButton);
  getRequiredElement('stop-debug-mode')
      .toggleAttribute('hidden', showEnableButton);
}

/**
 * @param data The debug data to display in table form.
 */
function onDebugDataRefreshed(data: string[][]) {
  if (data.length === 0) {
    toggleDebugModeButton(/*showEnableButton=*/ true);
  } else {
    toggleDebugModeButton(/*showEnableButton=*/ false);
    updateDebugDataTable(data);
  }
}

/**
 * @param tableData The debug data to display in table form.
 */
function updateDebugDataTable(tableData: string[][]) {
  const table = document.createElement('table');
  table.style.display = 'block';
  const tableBody = document.createElement('tbody');

  tableData.forEach(function(rowData) {
    const row = document.createElement('tr');
    rowData.forEach(function(cellData) {
      const cell = document.createElement('td');
      cell.appendChild(document.createTextNode(cellData));
      row.appendChild(cell);
    });
    tableBody.appendChild(row);
  });
  table.appendChild(tableBody);

  getRequiredElement('debug-data-table').replaceWith(table);
  table.id = 'debug-data-table';
}

function initialize() {
  const browserProxy: LensInternalsBrowserProxy =
      LensInternalsBrowserProxyImpl.getInstance();

  getRequiredElement('start-debug-mode').onclick = function() {
    browserProxy.startDebugMode().then(function() {
      // After starting debug mode automatically refresh data.  This will
      // toggle the button if the call was successful.
      browserProxy.refreshDebugData().then(onDebugDataRefreshed);
    });
  };

  getRequiredElement('stop-debug-mode').onclick = function() {
    browserProxy.stopDebugMode().then(function() {
      // After stopping debug mode automatically refresh data.  This will
      // toggle the button if the call was successful.
      browserProxy.refreshDebugData().then(onDebugDataRefreshed);
    });
  };

  // Start by attempting to refresh the debug data. Whether the java layer
  // supplied data back determines which debug mode button is displayed.
  browserProxy.refreshDebugData().then(onDebugDataRefreshed);
}

document.addEventListener('DOMContentLoaded', initialize);
