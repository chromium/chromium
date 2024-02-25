// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Handles DumpDatabase tab for syncfs-internals.
 */

import {assert} from 'chrome://resources/js/assert.js';
import {sendWithPromise} from 'chrome://resources/js/cr.js';

import {createElementFromText} from './utils.js';

/**
 * Get the database dump.
 */
function refreshDatabaseDump() {
  sendWithPromise('getDatabaseDump').then(onGetDatabaseDump);
}

/**
 * Creates a table by filling |header| and |body|.
 * @param div The outer container of the table to be
 *     renderered.
 * @param header The table header element to be fillied by
 *     this function.
 * @param body The table body element to be filled by this
 *     function.
 * @param databaseDump List of dictionaries for the database dump.
 *     The first element must have metadata of the entry.
 *     The remaining elements must be dictionaries for the database dump,
 *     which can be iterated using the 'keys' fields given by the first
 *     element.
 */
function createDatabaseDumpTable(
    div: HTMLElement, header: HTMLElement, body: HTMLElement,
    databaseDump: Array<{[key: string]: string | string[]}>) {
  const metadata = databaseDump.shift();
  assert(metadata);
  div.appendChild(createElementFromText('h3', metadata['title'] as string));
  const keys = metadata['keys'] as string[];

  let tr = document.createElement('tr');
  for (let i = 0; i < keys.length; ++i) {
    tr.appendChild(createElementFromText('td', keys[i]!));
  }
  header.appendChild(tr);

  for (let i = 0; i < databaseDump.length; i++) {
    const entry = databaseDump[i]!;
    tr = document.createElement('tr');
    for (let k = 0; k < keys.length; ++k) {
      tr.appendChild(createElementFromText('td', entry[keys[k]!] as string));
    }
    body.appendChild(tr);
  }
}

/**
 * Handles callback from onGetDatabaseDump.
 * @param databaseDump List of lists for the database dump.
 */
function onGetDatabaseDump(
    databaseDump: Array<Array<{[key: string]: string | string[]}>>) {
  const placeholder =
      document.querySelector<HTMLElement>('#dump-database-placeholder');
  assert(placeholder);
  assert(window.trustedTypes);
  placeholder.innerHTML = window.trustedTypes.emptyHTML;
  for (let i = 0; i < databaseDump.length; ++i) {
    const div = document.createElement('div');
    const table = document.createElement('table');
    const header = document.createElement('thead');
    const body = document.createElement('tbody');
    createDatabaseDumpTable(div, header, body, databaseDump[i]!);
    table.appendChild(header);
    table.appendChild(body);
    div.appendChild(table);
    placeholder.appendChild(div);
  }
}

function main() {
  refreshDatabaseDump();
  const refresh = document.querySelector<HTMLElement>('#refresh-database-dump');
  assert(refresh);
  refresh.addEventListener('click', refreshDatabaseDump);
}

document.addEventListener('DOMContentLoaded', main);
