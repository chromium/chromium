// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {quoteString} from 'chrome://resources/js/util.js';
import {get as deepGet} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import Fuse from './fuse.js';
import {ItemData} from './tab_data.js';

export type FuzzySearchOptions<T extends ItemData> =
    Fuse.IFuseOptions<T>&{useFuzzySearch: boolean};

/**
 * @return A new array of entries satisfying the input. If no search input is
 *     present, returns a shallow copy of the records.
 */
export function fuzzySearch<T extends ItemData>(
    input: string, records: T[], options: FuzzySearchOptions<T>): T[] {
  if (input.length === 0) {
    return [...records];
  }
  // Fuse does not handle exact match searches well. It indiscriminately
  // searches for direct matches that appear anywhere in the string. This
  // results in a bad search experience as users expect matches at the beginning
  // of the title / hostname, or at the beginning of words to receive
  // preferential treatment. Matched ranges returned by Fuse also fail to
  // highlight only the matching text, but instead match to any character
  // present in the input string.
  // To address these shortcomings we use the exactSearch implementation below
  // if the options indicate an exact matching algorithm should be used.
  const searchStartTime = Date.now();
  let result;
  if (options.useFuzzySearch) {
    const keyNames =
        (options.keys as Fuse.FuseOptionKeyObject[]).reduce((acc, {name}) => {
          acc.push(name as string);
          return acc;
        }, [] as string[]);
    result = new Fuse<T>(records, options).search(input).map(result => {
      const item = cloneTabDataObj<T>(result.item);
      item.highlightRanges = keyNames.reduce((acc, key) => {
        const match = result.matches!.find(e => e.key === key);
        if (match) {
          acc[key] = convertToRanges(match.indices);
        }

        return acc;
      }, {} as {[key: string]: Array<{start: number, length: number}>});

      return item;
    });
    // Reorder match result by priorities while retaining the
    // rank fuse.js returns within the same priority.
    result = prioritizeMatchResult(input, keyNames, result);
  } else {
    result = exactSearch(input, records, options);
  }
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

/**
 * Convert fuse.js matches [start1, end1], [start2, end2] ... to
 * ranges {start:start1, length:length1}, {start:start2, length:length2} ...
 * to be used by search_highlight_utils.js
 */
function convertToRanges(matches: readonly Fuse.RangeTuple[]):
    Array<{start: number, length: number}> {
  return matches.map(
      ([start, end]) => ({start: start, length: end - start + 1}));
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
    searchText: string, records: T[], options: Fuse.IFuseOptions<T>): T[] {
  if (searchText.length === 0) {
    return records;
  }

  // Default distance to calculate score for search fields based on match
  // position.
  const defaultDistance = 200;
  const distance = options.distance || defaultDistance;

  // Controls how heavily weighted the search field weights are relative to each
  // other in the scoring function.
  const searchFieldWeights = (options.keys as Fuse.FuseOptionKeyObject[])
                                 .reduce((acc, {name, weight}) => {
                                   acc[name as string] = weight;
                                   return acc;
                                 }, {} as {[key: string]: number});

  // Perform an exact match search with range discovery.
  const exactMatches = [];
  for (const tabDataRecord of records) {
    let matchFound = false;
    const matchedRecord = cloneTabDataObj(tabDataRecord);
    // Searches for fields or nested fields in the record.
    for (const fieldPath in searchFieldWeights) {
      const text = deepGet(tabDataRecord, fieldPath);
      if (text) {
        const ranges = getRanges(text, searchText);
        if (ranges.length !== 0) {
          matchedRecord.highlightRanges[fieldPath] = ranges;
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
      searchText, Object.keys(searchFieldWeights),
      exactMatches.map(item => item.tab));
}

/**
 * Determines whether the given tab has a search field with identified matches
 * at the beginning of the string.
 */
function hasMatchStringStart(
    tab: ItemData, searchText: string, keys: string[]): boolean {
  return keys.some((key) => {
    const value = deepGet(tab, key);
    return value !== undefined && value.startsWith(searchText);
  });
}

/**
 * Determines whether the given tab has a match for the given regexp in its
 * search fields.
 */
function hasRegexMatch(tab: ItemData, regexp: RegExp, keys: string[]): boolean {
  return keys.some((key) => {
    const value = deepGet(tab, key);
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
    searchText: string, keys: string[], result: T[]): T[] {
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
