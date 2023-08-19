// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {String16} from 'chrome://resources/mojo/mojo/public/mojom/base/string16.mojom-webui.js';

import {SearchResult as PersonalizationSearchResult} from '../mojom-webui/personalization_search.mojom-webui.js';
import {ParentResultBehavior, SearchResult as SettingsSearchResult} from '../mojom-webui/search.mojom-webui.js';

import {getPersonalizationSearchHandler} from './personalization_search_handler.js';
import {getSettingsSearchHandler} from './settings_search_handler.js';

export type SearchResult = SettingsSearchResult|PersonalizationSearchResult;

/**
 * Return array of the top |maxNumResults| search results.
 */
function mergeResults(
    a: SearchResult[], b: SearchResult[],
    maxNumResults: number): SearchResult[] {
  // Simple concat and sort is faster than 2-pointer algorithm for small arrays.
  return a.concat(b)
      .sort((x, y) => y.relevanceScore - x.relevanceScore)
      .slice(0, maxNumResults);
}

/**
 * Search both settings and personalization and merge the results.
 */
export async function combinedSearch(
    query: String16, maxNumResults: number,
    parentResultBehavior: ParentResultBehavior):
    Promise<{results: SearchResult[]}> {
  const [settingsResponse, personalizationResponse] = await Promise.all([
    getSettingsSearchHandler().search(
        query, maxNumResults, parentResultBehavior),
    getPersonalizationSearchHandler().search(query, maxNumResults),
  ]);
  return {
    results: mergeResults(
        settingsResponse.results, personalizationResponse.results,
        maxNumResults),
  };
}

export {
  getPersonalizationSearchHandler,
  getSettingsSearchHandler,
};
