// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {addWebUIListener} from 'chrome://resources/js/cr.m.js';
import {$} from 'chrome://resources/js/util.js';

function createButton(text, onClickClosure) {
  const button = document.createElement('button');
  button.innerText = text;
  button.addEventListener('click', onClickClosure);
  return button;
}

function createTableRow(...args) {
  const row = document.createElement('tr');
  for (const content of args) {
    const col = document.createElement('td');
    if (typeof content === 'object') {
      col.appendChild(content);
    } else {
      col.appendChild(document.createTextNode(content));
    }
    row.appendChild(col);
  }
  return row;
}

function clearTable(table) {
  while (table.firstChild) {
    table.removeChild(table.lastChild);
  }
}

function onFlagsInfoReceived(flags) {
  const addEntry = function(flag) {
    const nameLabel = flag['name'];
    const enabledLabel = flag['enabled'];
    const table = $('flags-table');
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
  const table = $('script-fetching-table');
  for (const [key, value] of Object.entries(scriptFetcherInfo)) {
    table.appendChild(createTableRow(key, value));
  }
}

function onPrefsInfoReceived(prefsInfo) {
  if (!prefsInfo) {
    return;
  }

  const table = $('prefs-table');
  clearTable(table);

  const addEntry = function(pref) {
    const nameLabel = pref['name'];
    const valueLabel = pref['value'];
    const controlLabel = pref['control_level'];
    const toggleButton = createButton('Toggle', () => {
      chrome.send('toggle-user-pref', [pref['name']]);
    });
    const removeButton = createButton('Remove user pref', () => {
      chrome.send('remove-user-pref', [pref['name']]);
    });
    table.appendChild(createTableRow(
        nameLabel, valueLabel, controlLabel, toggleButton, removeButton));
  };
  prefsInfo.forEach(addEntry);
}

function onAutofillAssistantInfoReceived(autofillAssistantInfo) {
  if (!autofillAssistantInfo) {
    return;
  }
  const table = $('autofill-assistant-table');
  clearTable(table);
  for (const [key, value] of Object.entries(autofillAssistantInfo)) {
    table.appendChild(createTableRow(key, value));
  }
}

function hideScriptCache() {
  const element = $('script-cache-content');
  element.textContent = 'Cache not shown.';
  $('script-start-parameters').style.display = 'none';
}

function showScriptCache() {
  chrome.send('get-script-cache');
}

function refreshScriptCache() {
  chrome.send('refresh-script-cache');
}

function setAutofillAssistantUrl() {
  const autofillAssistantUrl = $('autofill-assistant-url').value;
  chrome.send('set-autofill-assistant-url', [autofillAssistantUrl]);
}

function setAutofillAssistantCountryCode() {
  const autofillAssistantCountryCode =
      $('autofill-assistant-country-code').value;
  chrome.send(
      'set-autofill-assistant-country-code', [autofillAssistantCountryCode]);
}

function launchScript(origin) {
  chrome.send('launch-script', [origin, $('bundle-id').value, $('ldap').value]);
}

function onScriptCacheReceived(scriptsCacheInfo) {
  $('script-start-parameters').style.display = 'block';

  const element = $('script-cache-content');
  if (!scriptsCacheInfo.length) {
    element.textContent = 'Cache is empty.';
    return;
  }

  const table = document.createElement('table');
  for (const cacheEntry of scriptsCacheInfo) {
    const columns = [cacheEntry['url']];
    if ('has_script' in cacheEntry) {
      if (cacheEntry['has_script']) {
        columns.push('Available');

        const startButton = document.createElement('button');
        startButton.innerText = 'Start';
        startButton.addEventListener('click', function() {
          launchScript(cacheEntry['url']);
        });
        columns.push(startButton);
      } else {
        columns.push('Not available');
        columns.push('');
      }
    }

    const row = createTableRow(...columns);
    table.appendChild(row);
  }
  element.replaceChildren(table);
}

document.addEventListener('DOMContentLoaded', function(event) {
  addWebUIListener('on-flags-information-received', onFlagsInfoReceived);
  addWebUIListener('on-prefs-information-received', onPrefsInfoReceived);
  addWebUIListener(
      'on-script-fetching-information-received', onScriptFetchingInfoReceived);
  addWebUIListener(
      'on-autofill-assistant-information-received',
      onAutofillAssistantInfoReceived);
  addWebUIListener('on-script-cache-received', onScriptCacheReceived);

  hideScriptCache();
  $('script-cache-hide').onclick = hideScriptCache;
  $('script-cache-show').onclick = showScriptCache;
  $('script-cache-refresh').onclick = refreshScriptCache;
  $('set-autofill-assistant-url').onclick = setAutofillAssistantUrl;
  $('set-autofill-assistant-country-code').onclick =
      setAutofillAssistantCountryCode;

  chrome.send('loaded');
});
