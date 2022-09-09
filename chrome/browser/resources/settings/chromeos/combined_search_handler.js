// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {String16} from 'chrome://resources/mojo/mojo/public/mojom/base/string16.mojom-webui.js';

import {SearchResult as PersonalizationSearchResult} from '../mojom-webui/personalization/search.mojom-webui.js';
import {ParentResultBehavior, SearchResult as SettingsSearchResult} from '../mojom-webui/search/search.mojom-webui.js';

import {getPersonalizationSearchHandler} from './personalization_search_handler.js';
import {getSettingsSearchHandler} from './settings_search_handler.js';

/**
 * @typedef {SettingsSearchResult|PersonalizationSearchResult}
 */
export let SearchResult;

/**
 * Return array of the top |maxNumResults| search results.
 * @param {!Array<!SearchResult>} a
 * @param {!Array<!SearchResult>} b
 * @param {number} maxNumResults
 * @return {!Array<!SearchResult>}
 */
function mergeResults(a, b, maxNumResults) {
  // Simple concat and sort is faster than 2-pointer algorithm for small arrays.
  return a.concat(b)
      .sort((x, y) => y.relevanceScore - x.relevanceScore)
      .slice(0, maxNumResults);
}

/**
 * Search both settings and personalization and merge the results.
 * @param {!String16} query
 * @param {!number} maxNumResults
 * @param {!ParentResultBehavior} parentResultBehavior
 * @return {!Promise<{results: !Array<!SearchResult>}>}
 */
export async function combinedSearch(
    query, maxNumResults, parentResultBehavior) {
  const [settingsResponse, personalizationResponse] = await Promise.all([
    getSettingsSearchHandler().search(
        query, maxNumResults, parentResultBehavior),
    loadTimeData.getBoolean('isPersonalizationHubEnabled') ?
        getPersonalizationSearchHandler().search(query, maxNumResults) :
        {results: []},
  ]);
  return {
    results: mergeResults(
        settingsResponse.results, personalizationResponse.results,
        maxNumResults),
  };
}
