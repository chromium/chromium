// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from 'chrome://resources/js/assert.js';
import {sendWithPromise} from 'chrome://resources/js/cr.js';

interface ResourcePrefetchPredictorDb {
  enabled: boolean;
  origin_db: OriginData[];
}

interface OriginData {
  main_frame_host: string;
  origins: Array<{
    origin: string,
    number_of_hits: number,
    number_of_misses: number,
    consecutive_misses: number,
    position: number,
    always_access_network: boolean,
    accessed_network: boolean,
    score: number,
  }>;
}

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
 * @param database Information about
 *     ResourcePrefetchPredictor including the database as a flattened list, a
 *     boolean indicating if the system is enabled.
 */
function updateResourcePrefetchPredictorDb(
    database: ResourcePrefetchPredictorDb) {
  updateResourcePrefetchPredictorDbView(database);
}

/**
 * Truncates the string to keep the database readable.
 * @param str The string to truncate.
 * @return The truncated string.
 */
function truncateString(str: string): string {
  return str.length < 100 ? str : str.substring(0, 99);
}

/**
 * Updates the table from the database.
 * @param database Information about
 *     ResourcePrefetchPredictor including the database as a flattened list, a
 *     boolean indicating if the system is enabled and the current hit weight.
 */
function updateResourcePrefetchPredictorDbView(
    database: ResourcePrefetchPredictorDb) {
  const rppEnabled = document.body.querySelector<HTMLElement>('#rpp_enabled');
  const rppDisabled = document.body.querySelector<HTMLElement>('#rpp_disabled');
  assert(rppEnabled);
  assert(rppDisabled);

  if (!database.enabled) {
    rppEnabled.style.display = 'none';
    rppDisabled.style.display = 'block';
    return;
  }

  rppEnabled.style.display = 'block';
  rppDisabled.style.display = 'none';

  const hasOriginData = database.origin_db && database.origin_db.length > 0;

  if (hasOriginData) {
    const originBody =
        document.body.querySelector<HTMLElement>('#rpp_origin_body');
    assert(originBody);
    renderOriginData(originBody, database.origin_db);
  }
}

/**
 * Renders the content of the predictor origin table.
 * @param body element of table to render into.
 * @param database to render.
 */
function renderOriginData(body: HTMLElement, database: OriginData[]) {
  body.textContent = '';
  for (const main of database) {
    for (let j = 0; j < main.origins.length; ++j) {
      const origin = main.origins[j]!;
      const row = document.createElement('tr');

      if (j === 0) {
        const t = document.createElement('td');
        t.rowSpan = main.origins.length;
        t.textContent = truncateString(main.main_frame_host);
        row.appendChild(t);
      }

      row.className = 'action-none';
      row.appendChild(document.createElement('td')).textContent =
          truncateString(origin.origin);
      row.appendChild(document.createElement('td')).textContent =
          origin.number_of_hits.toString();
      row.appendChild(document.createElement('td')).textContent =
          origin.number_of_misses.toString();
      row.appendChild(document.createElement('td')).textContent =
          origin.consecutive_misses.toString();
      row.appendChild(document.createElement('td')).textContent =
          origin.position.toString();
      row.appendChild(document.createElement('td')).textContent =
          origin.always_access_network.toString();
      row.appendChild(document.createElement('td')).textContent =
          origin.accessed_network.toString();
      row.appendChild(document.createElement('td')).textContent =
          origin.score.toString();
      body.appendChild(row);
    }
  }
}

document.addEventListener(
    'DOMContentLoaded', requestResourcePrefetchPredictorDb);
