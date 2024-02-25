// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from 'chrome://resources/js/assert.js';
import {sendWithPromise} from 'chrome://resources/js/cr.js';

interface AutocompleteActionPredictorDb {
  enabled: boolean;
  db: Array<{
    user_text: string,
    url: string,
    hit_count: number,
    miss_count: number,
    confidence: number,
  }>;
}

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
 * @param database Information about
 *     AutocompleteActionPredictor including the database as a flattened list,
 *     a boolean indicating if the system is enabled and the current hit weight.
 */
function updateAutocompleteActionPredictorDb(
    database: AutocompleteActionPredictorDb) {
  const filter = document.body.querySelector<HTMLInputElement>('#filter');
  assert(filter);
  filter.disabled = false;
  filter.onchange = function() {
    updateAutocompleteActionPredictorDbView(database);
  };

  updateAutocompleteActionPredictorDbView(database);
}

/**
 * Updates the table from the database.
 * @param database Information about
 *     AutocompleteActionPredictor including the database as a flattened list,
 *     a boolean indicating if the system is enabled and the current hit weight.
 */
function updateAutocompleteActionPredictorDbView(
    database: AutocompleteActionPredictorDb) {
  const databaseSection =
      document.body.querySelector<HTMLElement>('#databaseTableBody');
  assert(databaseSection);
  const showEnabled = database.enabled && !!database.db;

  const enabledMode = document.body.querySelector<HTMLElement>(
      '#autocompleteActionPredictorEnabledMode');
  const disabledMode = document.body.querySelector<HTMLElement>(
      '#autocompleteActionPredictorDisabledMode');
  assert(enabledMode);
  assert(disabledMode);
  enabledMode.hidden = !showEnabled;
  disabledMode.hidden = showEnabled;

  if (!showEnabled) {
    return;
  }

  const filter = document.body.querySelector<HTMLInputElement>('#filter');
  assert(filter);

  // Clear any previous list.
  databaseSection.textContent = '';

  for (let i = 0; i < database.db.length; ++i) {
    const entry = database.db[i]!;

    if (!filter.checked || entry.confidence > 0) {
      const row = document.createElement('tr');

      // These values should be synchronized with the values in
      // chrome/browser/predictors/autocomplete_action_predictor.cc.
      // TODO(crbug.com/326277753): Avoid hard-coding the values here.
      let cssClass = 'action-none';
      if (entry.confidence >= 0.3) {
        cssClass = 'action-preconnect';
      }
      if (entry.confidence >= 0.5) {
        cssClass = 'action-prerender';
      }
      row.classList.add(cssClass);

      row.appendChild(document.createElement('td')).textContent =
          entry.user_text;
      row.appendChild(document.createElement('td')).textContent = entry.url;
      row.appendChild(document.createElement('td')).textContent =
          entry.hit_count.toString();
      row.appendChild(document.createElement('td')).textContent =
          entry.miss_count.toString();
      row.appendChild(document.createElement('td')).textContent =
          entry.confidence.toString();

      databaseSection.appendChild(row);
    }
  }
  const banner = document.body.querySelector<HTMLElement>('#countBanner');
  assert(banner);
  banner.textContent = 'Entries: ' + databaseSection.children.length;
}

document.addEventListener(
    'DOMContentLoaded', requestAutocompleteActionPredictorDb);
