// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Handles the Extension ID -> SyncStatus tab for syncfs-internals.
 */

import {assert} from 'chrome://resources/js/assert.js';
import {sendWithPromise} from 'chrome://resources/js/cr.js';

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
 */
function onGetExtensionStatuses(extensionStatuses: Array<{
  extensionName: string,
  extensionID: string,
  status: string,
}>) {
  const itemContainer =
      document.querySelector<HTMLElement>('#extension-entries');
  assert(itemContainer);
  itemContainer.textContent = '';

  for (let i = 0; i < extensionStatuses.length; i++) {
    const originEntry = extensionStatuses[i]!;
    const tr = document.createElement('tr');
    tr.appendChild(createElementFromText('td', originEntry.extensionName));
    tr.appendChild(createElementFromText('td', originEntry.extensionID));
    tr.appendChild(createElementFromText('td', originEntry.status));
    itemContainer.appendChild(tr);
  }
}

function main() {
  refreshExtensionStatuses();
  const refresh =
      document.querySelector<HTMLElement>('#refresh-extensions-statuses');
  assert(refresh);
  refresh.addEventListener('click', refreshExtensionStatuses);
}

document.addEventListener('DOMContentLoaded', main);
