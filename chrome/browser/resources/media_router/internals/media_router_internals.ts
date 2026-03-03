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

// Holds data that is currently displayed.
let currentLogs: MediaRouterLog[] = [];
let currentGeneralState: object = {};
let currentMirroringStats: object = {};
let currentProviderStates: {[key: string]: object} = {};

function displayMirroringStats(mirroringStats: object) {
  currentMirroringStats = mirroringStats;
  getRequiredElement('mirroring-stats-div').textContent =
      formatJson(mirroringStats);
}

// Build the table which displays Media Router logs.
// Build the table which displays Media Router logs.
function displayLogsTable(logs: MediaRouterLog[]) {
  currentLogs = logs;
  const logsTbody = getRequiredElement('logs-tbody');
  assertInstanceof(logs, Array);

  // Clear existing logs.
  logsTbody.replaceChildren();

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

    logsTbody.appendChild(logRow);
  }
}

function downloadSession() {
  const aggregatedData = {
    logs: currentLogs,
    generalState: currentGeneralState,
    mirroringStats: currentMirroringStats,
    providerStates: currentProviderStates,
  };
  const blob = new Blob([JSON.stringify(aggregatedData, null, 2)], {
    type: 'application/json',
  });
  const url = URL.createObjectURL(blob);
  const a = document.createElement('a');
  a.href = url;
  a.download =
      `media_router_internals_session_${new Date().toISOString()}.json`;
  document.body.appendChild(a);
  a.click();

  // Clean up the blob and the anchor element after a short delay to ensure
  // the download was initiated.
  setTimeout(() => {
    URL.revokeObjectURL(url);
    a.remove();
  }, 2000);
}

function handleImportSession() {
  const input = getRequiredElement('import-input');
  if (!(input instanceof HTMLInputElement)) {
    return;
  }
  if (input.files && input.files.length > 0) {
    const reader = new FileReader();
    reader.onload = (e) => {
      try {
        const result = e.target?.result as string;
        const data = JSON.parse(result);

        // Restore state from imported data
        if (data.generalState) {
          currentGeneralState = data.generalState;
          getRequiredElement('sink-status-div').textContent =
              formatJson(currentGeneralState);
        }
        if (data.providerStates) {
          currentProviderStates = data.providerStates;
          if (currentProviderStates['CAST']) {
            getRequiredElement('cast-status-div').textContent =
                formatJson(currentProviderStates['CAST']);
          }
        }
        if (data.mirroringStats) {
          displayMirroringStats(data.mirroringStats);
        }
        if (data.logs) {
          displayLogsTable(data.logs);
        }
      } catch (error) {
        console.error('Failed to parse imported session:', error);
        alert('Failed to parse imported session. See console for details.');
      }
    };
    const file = input.files[0];
    if (file) {
      reader.readAsText(file);
    }
  }
  // Reset input so the same file can be selected again if needed.
  input.value = '';
}

function clearSession() {
  currentLogs = [];
  currentGeneralState = {};
  currentMirroringStats = {};
  currentProviderStates = {};

  getRequiredElement('logs-tbody').replaceChildren();
  getRequiredElement('sink-status-div').textContent = '';
  getRequiredElement('cast-status-div').textContent = '';
  getRequiredElement('mirroring-stats-div').textContent = '';
}

// Handles user events for the Media Router Internals UI.
document.addEventListener('DOMContentLoaded', function() {
  // Bind buttons
  getRequiredElement('download-session')
      .addEventListener('click', downloadSession);
  getRequiredElement('import-session').addEventListener('click', () => {
    getRequiredElement('import-input').click();
  });
  getRequiredElement('import-input')
      .addEventListener('change', handleImportSession);
  getRequiredElement('clear-session').addEventListener('click', clearSession);

  // Initial fetch
  sendWithPromise('getState').then((status: object) => {
    currentGeneralState = status;
    getRequiredElement('sink-status-div').textContent = formatJson(status);
  });
  sendWithPromise('getProviderState', 'CAST').then((status: object) => {
    currentProviderStates['CAST'] = status;
    getRequiredElement('cast-status-div').textContent = formatJson(status);
  });
  sendWithPromise('getLogs').then(displayLogsTable);
  sendWithPromise('getMirroringStats').then((mirroringStats: object) => {
    displayMirroringStats(mirroringStats);
  });
  // The backend will fire 'on-mirroring-stats-update' when there are updates.
  addWebUiListener('on-mirroring-stats-update', displayMirroringStats);
  addWebUiListener('on-log-added', (log: MediaRouterLog) => {
    currentLogs.push(log);
    const logsTbody = getRequiredElement('logs-tbody');
    const logRow = document.createElement('tr');
    const logDict = log as unknown as {[key: string]: string};
    ['time', 'message', 'sessionId', 'sinkId', 'mediaSource', 'category',
     'component', 'severity']
        .forEach(fieldName => {
          const element = document.createElement('td');
          element.textContent = logDict[fieldName] ?? '';
          logRow.appendChild(element);
        });
    logsTbody.appendChild(logRow);
  });
});
