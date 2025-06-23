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

function startLogging() {
  BrowserProxy.getInstance().handler.startLogging();
  getRequiredElement('start-logging').style.display = 'none';
  getRequiredElement('stop-logging').style.display = 'inline-block';
}

function stopLogging() {
  BrowserProxy.getInstance().handler.stopLogging();
  getRequiredElement('start-logging').style.display = 'inline-block';
  getRequiredElement('stop-logging').style.display = 'none';
}

window.onload = function() {
  const proxy = BrowserProxy.getInstance();
  proxy.callbackRouter.journalEntryAdded.addListener((entry: JournalEntry) => {
    const table = getRequiredElement<HTMLTableElement>('actor-events-table');

    const template =
        getRequiredElement<HTMLTemplateElement>('actor-events-row');
    // Clone the new row and insert it into the table
    const clone = (template.content.cloneNode(true) as DocumentFragment);
    const tr = clone.children[0] as HTMLElement;
    tr.dataset['timestamp'] = entry.timestamp.getTime().toString();
    const td = clone.querySelectorAll('td');
    td[0]!.textContent = entry.url;
    td[1]!.textContent = entry.event;
    td[2]!.textContent = entry.type;
    td[3]!.textContent = entry.details;
    td[4]!.textContent = new Date(entry.timestamp).toUTCString();

    const rows = table.rows;
    for (let i = rows.length - 1; i > 0; i--) {
      const timestamp = Number(rows[i]!.dataset['timestamp']);
      if (entry.timestamp.getTime() >= timestamp) {
        if (i === rows.length - 1) {
          break;
        }
        table.insertBefore(clone, rows[i + 1]!);
        return;
      }
    }

    table.appendChild(clone);
  });

  getRequiredElement('start-logging').onclick = startLogging;
  getRequiredElement('stop-logging').onclick = stopLogging;
};
