// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_tab_box/cr_tab_box.js';

import {assertInstanceof} from 'chrome://resources/js/assert.js';
import {addWebUiListener, sendWithPromise} from 'chrome://resources/js/cr.js';
import {getRequiredElement} from 'chrome://resources/js/util.js';

interface MediaRouterLog {
  time: string;
  sessionId: string;
  sinkId: string;
  mediaSource: string;
  category: string;
  component: string;
  severity: string;
  message: string;
}

function formatJson(jsonObj: object) {
  return JSON.stringify(jsonObj, null, /* spacing level = */ 2);
}

function displayMirroringStats(mirroringStats: object) {
  getRequiredElement('mirroring-stats-div').textContent =
      formatJson(mirroringStats);
}

// Build the table which displays Media Router logs.
function displayLogsTable(logs: MediaRouterLog[]) {
  const logsTable = getRequiredElement('logs-table');
  assertInstanceof(logs, Array);
  for (const log of logs) {
    const logRow = document.createElement('tr');
    const logDict = log as unknown as {[key: string]: string};
    ['time', 'message', 'sessionId', 'sinkId', 'mediaSource', 'category',
     'component', 'severity']
        .forEach(fieldName => {
          const element = document.createElement('td');
          element.textContent = logDict[fieldName] ?? '';
          logRow.appendChild(element);
        });

    logsTable.appendChild(logRow);
  }
}

// Handles user events for the Media Router Internals UI.
document.addEventListener('DOMContentLoaded', function() {
  sendWithPromise('getState').then((status: object) => {
    getRequiredElement('sink-status-div').textContent = formatJson(status);
  });
  sendWithPromise('getProviderState', 'CAST').then((status: object) => {
    getRequiredElement('cast-status-div').textContent = formatJson(status);
  });
  sendWithPromise('getLogs').then(displayLogsTable);
  addWebUiListener(
      'on-mirroring-stats-update',
      (mirroringStats: object) => displayMirroringStats(mirroringStats));
  sendWithPromise('getMirroringStats').then(displayMirroringStats);
});
