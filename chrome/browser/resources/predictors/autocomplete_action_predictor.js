// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {sendWithPromise} from 'chrome://resources/js/cr.m.js';
import {$} from 'chrome://resources/js/util.m.js';

/**
 * @typedef {{
 *   enabled: boolean,
 *   db: !Array<!{
 *     user_text: string,
 *     url: string,
 *     hit_count: number,
 *     miss_count: number,
 *     confidence: number,
 *   }>
 * }}
 */
let AutocompleteActionPredictorDb;

/**
 * Requests the database from the backend.
 */
function requestAutocompleteActionPredictorDb() {
  sendWithPromise('requestAutocompleteActionPredictorDb')
      .then(updateAutocompleteActionPredictorDb);
}

/**
 * Callback from backend with the database contents. Sets up some globals and
 * calls to create the UI.
 * @param {!AutocompleteActionPredictorDb} database Information about
 *     AutocompleteActionPredictor including the database as a flattened list,
 *     a boolean indicating if the system is enabled and the current hit weight.
 */
function updateAutocompleteActionPredictorDb(database) {
  console.debug('Updating Table NAP DB');

  const filter = $('filter');
  filter.disabled = false;
  filter.onchange = function() {
    updateAutocompleteActionPredictorDbView(database);
  };

  updateAutocompleteActionPredictorDbView(database);
}

/**
 * Updates the table from the database.
 * @param {!AutocompleteActionPredictorDb} database Information about
 *     AutocompleteActionPredictor including the database as a flattened list,
 *     a boolean indicating if the system is enabled and the current hit weight.
 */
function updateAutocompleteActionPredictorDbView(database) {
  const databaseSection = $('databaseTableBody');
  const showEnabled = database.enabled && !!database.db;

  $('autocompleteActionPredictorEnabledMode').hidden = !showEnabled;
  $('autocompleteActionPredictorDisabledMode').hidden = showEnabled;

  if (!showEnabled) {
    return;
  }

  const filter = $('filter');

  // Clear any previous list.
  databaseSection.textContent = '';

  for (let i = 0; i < database.db.length; ++i) {
    const entry = database.db[i];

    if (!filter.checked || entry.confidence > 0) {
      const row = document.createElement('tr');
      row.className =
          (entry.confidence > 0.8 ?
               'action-prerender' :
               (entry.confidence > 0.5 ? 'action-preconnect' : 'action-none'));

      row.appendChild(document.createElement('td')).textContent =
          entry.user_text;
      row.appendChild(document.createElement('td')).textContent = entry.url;
      row.appendChild(document.createElement('td')).textContent =
          entry.hit_count;
      row.appendChild(document.createElement('td')).textContent =
          entry.miss_count;
      row.appendChild(document.createElement('td')).textContent =
          entry.confidence;

      databaseSection.appendChild(row);
    }
  }
  $('countBanner').textContent = 'Entries: ' + databaseSection.children.length;
}

document.addEventListener(
    'DOMContentLoaded', requestAutocompleteActionPredictorDb);
