// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Handles DumpDatabase tab for syncfs-internals.
 */

import {sendWithPromise} from 'chrome://resources/js/cr.m.js';
import {$} from 'chrome://resources/js/util.m.js';
import {createElementFromText} from './utils.js';

/**
 * Get the database dump.
 */
function refreshDatabaseDump() {
  sendWithPromise('getDatabaseDump').then(onGetDatabaseDump);
}

/**
 * Creates a table by filling |header| and |body|.
 * @param {!HTMLElement} div The outer container of the table to be
 *     renderered.
 * @param {!HTMLElement} header The table header element to be fillied by
 *     this function.
 * @param {!HTMLElement} body The table body element to be filled by this
 *     function.
 * @param {Array} databaseDump List of dictionaries for the database dump.
 *     The first element must have metadata of the entry.
 *     The remaining elements must be dictionaries for the database dump,
 *     which can be iterated using the 'keys' fields given by the first
 *     element.
 */
function createDatabaseDumpTable(div, header, body, databaseDump) {
  const metadata = databaseDump.shift();
  div.appendChild(createElementFromText('h3', metadata['title']));

  let tr = document.createElement('tr');
  for (let i = 0; i < metadata.keys.length; ++i) {
    tr.appendChild(createElementFromText('td', metadata.keys[i]));
  }
  header.appendChild(tr);

  for (let i = 0; i < databaseDump.length; i++) {
    const entry = databaseDump[i];
    tr = document.createElement('tr');
    for (let k = 0; k < metadata.keys.length; ++k) {
      tr.appendChild(createElementFromText('td', entry[metadata.keys[k]]));
    }
    body.appendChild(tr);
  }
}

/**
 * Handles callback from onGetDatabaseDump.
 * @param {Array} databaseDump List of lists for the database dump.
 */
function onGetDatabaseDump(databaseDump) {
  const placeholder = $('dump-database-placeholder');
  placeholder.innerHTML = trustedTypes.emptyHTML;
  for (let i = 0; i < databaseDump.length; ++i) {
    const div = /** @type {!HTMLElement} */ (document.createElement('div'));
    const table = document.createElement('table');
    const header =
        /** @type {!HTMLElement} */ (document.createElement('thead'));
    const body =
        /** @type {!HTMLElement} */ (document.createElement('tbody'));
    createDatabaseDumpTable(div, header, body, databaseDump[i]);
    table.appendChild(header);
    table.appendChild(body);
    div.appendChild(table);
    placeholder.appendChild(div);
  }
}

function main() {
  refreshDatabaseDump();
  $('refresh-database-dump').addEventListener('click', refreshDatabaseDump);
}

document.addEventListener('DOMContentLoaded', main);
