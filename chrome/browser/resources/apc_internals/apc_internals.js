// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {addWebUIListener} from 'chrome://resources/js/cr.m.js';
import {$} from 'chrome://resources/js/util.m.js';

function createTableRow(...args) {
  const row = document.createElement('tr');
  for (const content of args) {
    const col = document.createElement('td');
    col.appendChild(document.createTextNode(content));
    row.appendChild(col);
  }
  return row;
}

function onFlagsInfoReceived(flags) {
  const addEntry = function(flag) {
    const nameLabel = flag['name'];
    const enabledLabel = flag['enabled'];
    const table = $('flags_table');
    table.appendChild(createTableRow(nameLabel, enabledLabel));
    // If they exist, also list feature parameters.
    if ('parameters' in flag) {
      for (const [parameter, value] of Object.entries(flag['parameters'])) {
        table.appendChild(createTableRow('* ' + parameter, value));
      }
    }
  };
  flags.forEach(addEntry);
}

function onScriptFetchingInfoReceived(scriptFetcherInfo) {
  if (!scriptFetcherInfo) {
    return;
  }
  const table = $('script_fetching_table');
  for (const [key, value] of Object.entries(scriptFetcherInfo)) {
    table.appendChild(createTableRow(key, value));
  }
}

function onAutofillAssistantInfoReceived(autofillAssistantInfo) {
  if (!autofillAssistantInfo) {
    return;
  }
  const table = $('autofill_assistant_table');
  for (const [key, value] of Object.entries(autofillAssistantInfo)) {
    table.appendChild(createTableRow(key, value));
  }
}

document.addEventListener('DOMContentLoaded', function(event) {
  addWebUIListener('on-flags-information-received', onFlagsInfoReceived);
  addWebUIListener(
      'on-script-fetching-information-received', onScriptFetchingInfoReceived);
  addWebUIListener(
      'on-autofill-assistant-information-received',
      onAutofillAssistantInfoReceived);

  chrome.send('loaded');
});
