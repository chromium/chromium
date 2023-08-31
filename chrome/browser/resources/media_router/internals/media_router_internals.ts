// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {addWebUiListener, sendWithPromise} from 'chrome://resources/js/cr.js';
import {getRequiredElement} from 'chrome://resources/js/util_ts.js';

function formatJson(jsonObj: object) {
  return JSON.stringify(jsonObj, null, /* spacing level = */ 2);
}

function displayMirroringStats(mirroringStats: object) {
  getRequiredElement('mirroring-stats-div').textContent =
      formatJson(mirroringStats);
}

// Handles user events for the Media Router Internals UI.
document.addEventListener('DOMContentLoaded', function() {
  sendWithPromise('getState').then((status: object) => {
    getRequiredElement('sink-status-div').textContent = formatJson(status);
  });
  sendWithPromise('getProviderState', 'CAST').then((status: object) => {
    getRequiredElement('cast-status-div').textContent = formatJson(status);
  });
  sendWithPromise('getLogs').then((logs: object) => {
    // TODO(crbug.com/687380): Present the logs in a table format.
    getRequiredElement('logs-div').textContent = formatJson(logs);
  });
  addWebUiListener(
      'on-mirroring-stats-update',
      (mirroringStats: object) => displayMirroringStats(mirroringStats));
  sendWithPromise('getMirroringStats').then(displayMirroringStats);
});
