// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {sendWithPromise} from 'chrome://resources/js/cr.m.js';
import {$} from 'chrome://resources/js/util.m.js';

function formatJson(jsonObj) {
  return JSON.stringify(jsonObj, null, /* spacing level = */ 2);
}

// Handles user events for the Media Router Internals UI.
document.addEventListener('DOMContentLoaded', function() {
  sendWithPromise('getState').then(status => {
    $('sink-status-div').textContent = formatJson(status);
  });
  sendWithPromise('getProviderState', 'CAST').then(status => {
    $('cast-status-div').textContent = formatJson(status);
  });
  sendWithPromise('getLogs').then(logs => {
    // TODO(crbug.com/687380): Present the logs in a table format.
    $('logs-div').textContent = formatJson(logs);
  });
});
