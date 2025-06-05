// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Typescript for actor_internals.html, served from chrome://actor-internals/
 * This is used to debug actor events recording. It displays a live
 * stream of all actor events that occur in chromium while the
 * chrome://actor-internals/ page is open.
 */

import {getRequiredElement} from '//resources/js/util.js';

import type {JournalEntry} from './actor_internals.mojom-webui.js';
import {BrowserProxy} from './browser_proxy.js';


window.onload = function() {
  const proxy = BrowserProxy.getInstance();
  proxy.callbackRouter.journalEntryAdded.addListener((entry: JournalEntry) => {
    const table = getRequiredElement('actor-events-table');
    const tr = document.createElement('tr');
    let td = document.createElement('td');
    td.textContent = entry.url;
    tr.appendChild(td);
    td = document.createElement('td');
    td.textContent = entry.event;
    tr.appendChild(td);
    td = document.createElement('td');
    td.textContent = entry.type;
    tr.appendChild(td);
    td = document.createElement('td');
    td.textContent = entry.details;
    tr.appendChild(td);
    td = document.createElement('td');
    td.textContent = new Date(entry.timestamp).toUTCString();
    tr.appendChild(td);
    table.appendChild(tr);
  });
};
