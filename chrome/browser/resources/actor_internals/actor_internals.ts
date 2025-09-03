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

function clearFilter() {
  getRequiredElement<HTMLInputElement>('task-id-filter').value = '';
  const table = getRequiredElement<HTMLTableElement>('actor-events-table');
  const rows = Array.from(table.rows);
  for (let i = 1; i < rows.length; i++) {
    rows[i]!.style.display = '';
  }
}

// Displays rows that match the task id filter. Includes null task ids.
function filterByTaskId() {
  const taskId =
      getRequiredElement<HTMLInputElement>('task-id-filter').value.trim();
  if (!taskId) {
    return;
  }

  const table = getRequiredElement<HTMLTableElement>('actor-events-table');
  const rows = Array.from(table.rows);
  let firstIndex = -1;
  let lastIndex = -1;

  for (let i = 1; i < rows.length; i++) {
    if (rows[i]!.dataset['taskId'] === taskId) {
      if (firstIndex === -1) {
        firstIndex = i;
      }
      lastIndex = i;
    }
  }

  // If now rows match the filter, return.
  if (firstIndex === -1) {
    return;
  }

  for (let i = 1; i < rows.length; i++) {
    if (i >= firstIndex && i <= lastIndex) {
      rows[i]!.style.display = '';
    } else {
      rows[i]!.style.display = 'none';
    }
  }
}



window.onload = function() {
  const proxy = BrowserProxy.getInstance();
  proxy.callbackRouter.journalEntryAdded.addListener((entry: JournalEntry) => {
    const table = getRequiredElement<HTMLTableElement>('actor-events-table');
    const template =
        getRequiredElement<HTMLTemplateElement>('actor-events-row');
    const clone = (template.content.cloneNode(true) as DocumentFragment);
    const tr = clone.children[0] as HTMLElement;
    tr.dataset['timestamp'] = entry.timestamp.getTime().toString();
    tr.dataset['taskId'] = entry.taskId.toString();
    const td = clone.querySelectorAll('td');
    td[0]!.textContent = entry.taskId === 0 ? '' : entry.taskId.toString();
    td[1]!.textContent = entry.url;
    td[2]!.textContent = entry.event;
    td[3]!.textContent = entry.type;
    td[4]!.textContent = entry.details;
    td[5]!.textContent =
        new Date(entry.timestamp).toLocaleTimeString(undefined, {
          hour12: false,
          timeZoneName: 'short',
        });

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
  getRequiredElement('filter-by-task-id').onclick = filterByTaskId;
  getRequiredElement('clear-filter').onclick = clearFilter;
};
