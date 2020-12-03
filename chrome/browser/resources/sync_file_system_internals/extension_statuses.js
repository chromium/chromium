// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Handles the Extension ID -> SyncStatus tab for syncfs-internals.
 */

import {sendWithPromise} from 'chrome://resources/js/cr.m.js';
import {$} from 'chrome://resources/js/util.m.js';
import {createElementFromText} from './utils.js';

/**
 * Get initial map of extension statuses (pending batch sync, enabled and
 * disabled).
 */
function refreshExtensionStatuses() {
  sendWithPromise('getExtensionStatuses').then(onGetExtensionStatuses);
}

/**
 * Handles callback from onGetExtensionStatuses.
 * @param {!Array<!{
 *   extensionName: string,
 *   extensionID: string,
 *   status: string,
 * }>} extensionStatuses
 */
function onGetExtensionStatuses(extensionStatuses) {
  const itemContainer = $('extension-entries');
  itemContainer.textContent = '';

  for (let i = 0; i < extensionStatuses.length; i++) {
    const originEntry = extensionStatuses[i];
    const tr = document.createElement('tr');
    tr.appendChild(createElementFromText('td', originEntry.extensionName));
    tr.appendChild(createElementFromText('td', originEntry.extensionID));
    tr.appendChild(createElementFromText('td', originEntry.status));
    itemContainer.appendChild(tr);
  }
}

function main() {
  refreshExtensionStatuses();
  $('refresh-extensions-statuses')
      .addEventListener('click', refreshExtensionStatuses);
}

document.addEventListener('DOMContentLoaded', main);
