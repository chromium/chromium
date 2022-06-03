// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {sendWithPromise} from 'chrome://resources/js/cr.m.js';
import {$} from 'chrome://resources/js/util.m.js';

/**
 * @typedef {{
 *   enabled: boolean,
 *   origin_db: !Array<!OriginData>
 * }}
 */
let ResourcePrefetchPredictorDb;

/**
 * @typedef {{
 *   main_frame_host: string,
 *   origins: !Array<!{
 *     origin: string,
 *     number_of_hits: number,
 *     number_of_misses: number,
 *     consecutive_misses: number,
 *     position: number,
 *     always_access_network: boolean,
 *     accessed_network: boolean,
 *     score: number
 *   }>
 * }}
 */
let OriginData;

/**
 * Requests the database from the backend.
 */
function requestResourcePrefetchPredictorDb() {
  sendWithPromise('requestResourcePrefetchPredictorDb')
      .then(updateResourcePrefetchPredictorDb);
}

/**
 * Callback from backend with the database contents. Sets up some globals and
 * calls to create the UI.
 * @param {!ResourcePrefetchPredictorDb} database Information about
 *     ResourcePrefetchPredictor including the database as a flattened list, a
 *     boolean indicating if the system is enabled.
 */
function updateResourcePrefetchPredictorDb(database) {
  updateResourcePrefetchPredictorDbView(database);
}

/**
 * Truncates the string to keep the database readable.
 * @param {string} str The string to truncate.
 * @return {string} The truncated string.
 */
function truncateString(str) {
  return str.length < 100 ? str : str.substring(0, 99);
}

/**
 * Updates the table from the database.
 * @param {!ResourcePrefetchPredictorDb} database Information about
 *     ResourcePrefetchPredictor including the database as a flattened list, a
 *     boolean indicating if the system is enabled and the current hit weight.
 */
function updateResourcePrefetchPredictorDbView(database) {
  if (!database.enabled) {
    $('rpp_enabled').style.display = 'none';
    $('rpp_disabled').style.display = 'block';
    return;
  }

  $('rpp_enabled').style.display = 'block';
  $('rpp_disabled').style.display = 'none';

  const hasOriginData = database.origin_db && database.origin_db.length > 0;

  if (hasOriginData) {
    renderOriginData($('rpp_origin_body'), database.origin_db);
  }
}

/**
 * Renders the content of the predictor origin table.
 * @param {HTMLElement} body element of table to render into.
 * @param {!Array<!OriginData>} database to render.
 */
function renderOriginData(body, database) {
  body.textContent = '';
  for (const main of database) {
    for (let j = 0; j < main.origins.length; ++j) {
      const origin = main.origins[j];
      const row = document.createElement('tr');

      if (j == 0) {
        const t = document.createElement('td');
        t.rowSpan = main.origins.length;
        t.textContent = truncateString(main.main_frame_host);
        row.appendChild(t);
      }

      row.className = 'action-none';
      row.appendChild(document.createElement('td')).textContent =
          truncateString(origin.origin);
      row.appendChild(document.createElement('td')).textContent =
          origin.number_of_hits;
      row.appendChild(document.createElement('td')).textContent =
          origin.number_of_misses;
      row.appendChild(document.createElement('td')).textContent =
          origin.consecutive_misses;
      row.appendChild(document.createElement('td')).textContent =
          origin.position;
      row.appendChild(document.createElement('td')).textContent =
          origin.always_access_network;
      row.appendChild(document.createElement('td')).textContent =
          origin.accessed_network;
      row.appendChild(document.createElement('td')).textContent =
          origin.score;
      body.appendChild(row);
    }
  }
}

document.addEventListener(
    'DOMContentLoaded', requestResourcePrefetchPredictorDb);
