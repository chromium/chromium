// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
import {$} from 'chrome://resources/js/util.m.js';

import {LensInternalsBrowserProxy, LensInternalsBrowserProxyImpl} from './lens_internals_browser_proxy.js';

/** @param {boolean} showEnableButton Whether to show the "start" button. */
function toggleDebugModeButton(showEnableButton) {
  $('start-debug-mode').toggleAttribute('hidden', !showEnableButton);
  $('stop-debug-mode').toggleAttribute('hidden', showEnableButton);
}

/**
 * @param {!Array<!Array<string>>} data The debug data to display in table
 *     form.
 */
function onDebugDataRefreshed(data) {
  if (data.length === 0) {
    toggleDebugModeButton(/*showEnableButton=*/ true);
  } else {
    toggleDebugModeButton(/*showEnableButton=*/ false);
    updateDebugDataTable(data);
  }
}

/**
 * @param {!Array<!Array<string>>} tableData The debug data to display in
 *     table form.
 */
function updateDebugDataTable(tableData) {
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

  $('debug-data-table').replaceWith(table);
  table.id = 'debug-data-table';
}

function initialize() {
  /** @type {!LensInternalsBrowserProxy} */
  const browserProxy = LensInternalsBrowserProxyImpl.getInstance();

  $('start-debug-mode').onclick = function() {
    browserProxy.startDebugMode().then(function() {
      // After starting debug mode automatically refresh data.  This will
      // toggle the button if the call was successful.
      browserProxy.refreshDebugData().then(onDebugDataRefreshed);
    });
  };

  $('stop-debug-mode').onclick = function() {
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
