// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {quoteString} from 'chrome://resources/js/util.js';

import type {ItemData, TabData, TabGroupData} from './tab_data.js';

export interface OptionKeyObject {
  name: string;
  getter: (data: TabData|TabGroupData) => string | undefined;
  weight: number;
}

export interface SearchOptions {
  includeScore?: boolean;
  includeMatches?: boolean;
  ignoreLocation?: boolean;
  threshold?: number;
  distance?: number;
  keys: OptionKeyObject[];
}

/**
 * @return A new array of entries satisfying the input. If no search input is
 *     present, returns a shallow copy of the records.
 */
export function search<T extends ItemData>(
    input: string, records: T[], options: SearchOptions): T[] {
  if (input.length === 0) {
    return [...records];
  }
  const searchStartTime = Date.now();
  const result = exactSearch(input, records, options);
  chrome.metricsPrivate.recordTime(
      'Tabs.TabSearch.WebUI.SearchAlgorithmDuration',
      Math.round(Date.now() - searchStartTime));
  return result;
}

function cloneTabDataObj<T extends ItemData>(tabData: T): T {
  const clone = Object.assign({}, tabData);
  clone.highlightRanges = {};
  Object.setPrototypeOf(clone, Object.getPrototypeOf(tabData));

  return clone;
}

////////////////////////////////////////////////////////////////////////////////
// Exact Match Implementation :

/**
 * The exact match algorithm returns records ranked according to priorities
 * and scores. Records are ordered by priority (higher priority comes
 * first) and sorted by score within the same priority. See `scoringFunction`
 * for how to calculate score and `prioritizeMatchResult` for how to calculate
 * priority.
 */
function exactSearch<T extends ItemData>(
    searchText: string, records: T[], options: SearchOptions): T[] {
  if (searchText.length === 0) {
    return records;
  }

  // Default distance to calculate score for search fields based on match
  // position.
  const defaultDistance = 200;
  const distance = options.distance || defaultDistance;

  // Controls how heavily weighted the search field weights are relative to each
  // other in the scoring function.
  const searchFieldWeights =
      (options.keys as OptionKeyObject[]).reduce((acc, {name, weight}) => {
        acc[name as string] = weight;
        return acc;
      }, {} as {[key: string]: number});

  // Perform an exact match search with range discovery.
  const exactMatches = [];
  for (const tabDataRecord of records) {
    let matchFound = false;
    const matchedRecord = cloneTabDataObj(tabDataRecord);
    // Searches for fields or nested fields in the record.
    for (const key of options.keys) {
      const text = key.getter(tabDataRecord as TabData | TabGroupData);
      if (text) {
        const ranges = getRanges(text, searchText);
        if (ranges.length !== 0) {
          matchedRecord.highlightRanges[key.name] = ranges;
          matchFound = true;
        }
      }
    }

    if (matchFound) {
      exactMatches.push({
        tab: matchedRecord,
        score: scoringFunction(matchedRecord, distance, searchFieldWeights),
      });
    }
  }

  // Sort by score.
  exactMatches.sort((a, b) => (b.score - a.score));

  // Reorder match result by priorities.
  return prioritizeMatchResult(
      searchText, options.keys, exactMatches.map(item => item.tab));
}

/**
 * Determines whether the given tab has a search field with identified matches
 * at the beginning of the string.
 */
function hasMatchStringStart(
    tab: ItemData, searchText: string, keys: OptionKeyObject[]): boolean {
  return keys.some(key => {
    const value = key.getter(tab as TabData | TabGroupData);
    return value !== undefined && value.startsWith(searchText);
  });
}

/**
 * Determines whether the given tab has a match for the given regexp in its
 * search fields.
 */
function hasRegexMatch(
    tab: ItemData, regexp: RegExp, keys: OptionKeyObject[]): boolean {
  return keys.some((key) => {
    const value = key.getter(tab as TabData | TabGroupData);
    return value !== undefined && value.search(regexp) !== -1;
  });
}

/**
 * Returns an array of matches that indicate where in the target string the
 * searchText appears. If there are no identified matches an empty array is
 * returned.
 */
function getRanges(target: string, searchText: string):
    Array<{start: number, length: number}> {
  const escapedText = quoteString(searchText);
  const ranges = [];
  let match = null;
  for (const re = new RegExp(escapedText, 'gi'); match = re.exec(target);) {
    ranges.push({
      start: match.index,
      length: searchText.length,
    });
  }
  return ranges;
}

/**
 * A scoring function based on match indices of specified search fields.
 * Matches near the beginning of the string will have a higher score than
 * matches near the end of the string. Multiple matches will have a higher score
 * than single matches.
 */
function scoringFunction(
    tabData: ItemData, distance: number,
    searchFieldWeights: {[key: string]: number}) {
  let score = 0;
  // For every match, map the match index in [0, distance] to a scalar value in
  // [1, 0].
  for (const key in searchFieldWeights) {
    if (tabData.highlightRanges[key]) {
      for (const {start} of tabData.highlightRanges[key]!) {
        score += Math.max((distance - start) / distance, 0) *
            searchFieldWeights[key]!;
      }
    }
  }

  return score;
}

/**
 * Reorder match result based on priorities (highest to lowest priority):
 * 1. All items with a search key matching the searchText at the beginning of
 *    the string.
 * 2. All items with a search key matching the searchText at the beginning of a
 *    word in the string.
 * 3. All remaining items with a search key matching the searchText elsewhere in
 *    the string.
 */
function prioritizeMatchResult<T extends ItemData>(
    searchText: string, keys: OptionKeyObject[], result: T[]): T[] {
  const itemsMatchingStringStart = [];
  const itemsMatchingWordStart = [];
  const others = [];
  const wordStartRegexp = new RegExp(`\\b${quoteString(searchText)}`, 'i');
  for (const tab of result) {
    // Find matches that occur at the beginning of the string.
    if (hasMatchStringStart(tab, searchText, keys)) {
      itemsMatchingStringStart.push(tab);
    } else if (hasRegexMatch(tab, wordStartRegexp, keys)) {
      itemsMatchingWordStart.push(tab);
    } else {
      others.push(tab);
    }
  }
  return itemsMatchingStringStart.concat(itemsMatchingWordStart, others);
}
