// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {quoteString} from 'chrome://resources/js/util.m.js';
import {get as deepGet} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import Fuse from './fuse.js';
import {TabData} from './tab_data.js';

/**
 * @param {string} input
 * @param {!Array<!TabData>} records
 * @param {!Object} options
 * @return {!Array<!TabData>} A new array of entries satisfying the input. If no
 *     search input is present, returns a shallow copy of the records.
 */
export function fuzzySearch(input, records, options) {
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
  performance.mark('tab_search:search_algorithm:metric_begin');
  let result;
  if (options.threshold === 0.0) {
    result = exactSearch(input, records, options);
  } else {
    const keyNames = options.keys.reduce((acc, {name}) => {
      acc.push(name);
      return acc;
    }, []);
    result = new Fuse(records, options).search(input).map(result => {
      const item = cloneTabDataObj(result.item);
      item.highlightRanges = keyNames.reduce((acc, key) => {
        const match = result.matches.find(e => e.key === key);
        if (match) {
          acc[key] = convertToRanges(match.indices);
        }

        return acc;
      }, {});

      return item;
    });
  }
  performance.mark('tab_search:search_algorithm:metric_end');
  return result;
}

/**
 * @param {!TabData} tabData
 * @return {!TabData}
 */
function cloneTabDataObj(tabData) {
  const clone = Object.assign({}, tabData);
  clone.highlightRanges = {};
  Object.setPrototypeOf(clone, Object.getPrototypeOf(tabData));

  return /** @type {!TabData} */ (clone);
}

/**
 * Convert fuse.js matches [start1, end1], [start2, end2] ... to
 * ranges {start:start1, length:length1}, {start:start2, length:length2} ...
 * to be used by search_highlight_utils.js
 * @param {!Array<!Array<number>>} matches
 * @return {!Array<!{start: number, length: number}>}
 */
function convertToRanges(matches) {
  return matches.map(
      ([start, end]) => ({start: start, length: end - start + 1}));
}

////////////////////////////////////////////////////////////////////////////////
// Exact Match Implementation :

/**
 * The exact match algorithm returns records ranked according to the following
 * priorities (highest to lowest priority):
 * 1. All items with a search key matching the searchText at the beginning of
 *    the string.
 * 2. All items with a search key matching the searchText at the beginning of a
 *    word in the string.
 * 3. All remaining items with a search key matching the searchText elsewhere in
 *    the string.
 * @param {string} searchText
 * @param {!Array<!TabData>} records
 * @param {!Object} options
 * @return {!Array<!TabData>}
 */
function exactSearch(searchText, records, options) {
  if (searchText.length === 0) {
    return records;
  }

  // Default distance to calculate score for search fields based on match
  // position.
  const defaultDistance = 200;
  const distance = options.distance || defaultDistance;

  // Controls how heavily weighted the search field weights are relative to each
  // other in the scoring function.
  const searchFieldWeights = options.keys.reduce((acc, {name, weight}) => {
    acc[name] = weight;
    return acc;
  }, {});

  // Perform an exact match search with range discovery.
  const exactMatches = [];
  for (const tabDataRecord of records) {
    let matchFound = false;
    const matchedRecord = cloneTabDataObj(tabDataRecord);
    // Searches for fields or nested fields in the record.
    for (const fieldPath in searchFieldWeights) {
      const text =
          /** @type {string} */ (deepGet(tabDataRecord, fieldPath));
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
        score: scoringFunction(matchedRecord, distance, searchFieldWeights)
      });
    }
  }

  // Sort by score.
  exactMatches.sort((a, b) => (b.score - a.score));

  // Prioritize items.
  const itemsMatchingStringStart = [];
  const itemsMatchingWordStart = [];
  const others = [];
  const wordStartRegexp = new RegExp(`\\b${quoteString(searchText)}`, 'i');
  const keys = Object.keys(searchFieldWeights);
  for (const {tab} of exactMatches) {
    // Find matches that occur at the beginning of the string.
    if (hasMatchStringStart(tab, keys)) {
      itemsMatchingStringStart.push(tab);
    } else if (hasRegexMatch(tab, wordStartRegexp, keys)) {
      itemsMatchingWordStart.push(tab);
    } else {
      others.push(tab);
    }
  }
  return itemsMatchingStringStart.concat(itemsMatchingWordStart, others);
}

/**
 * Determines whether the given tab has a search field with identified matches
 * at the beginning of the string.
 * @param {!TabData} tab
 * @param {!Array<string>} keys
 * @return {boolean}
 */
function hasMatchStringStart(tab, keys) {
  return keys.some(
      (key) => tab.highlightRanges[key] !== undefined &&
          tab.highlightRanges[key][0].start === 0);
}

/**
 * Determines whether the given tab has a match for the given regexp in its
 * search fields.
 * @param {!TabData} tab
 * @param {RegExp} regexp
 * @param {!Array<string>} keys
 * @return {boolean}
 */
function hasRegexMatch(tab, regexp, keys) {
  return keys.some(
      (key) => tab.highlightRanges[key] !== undefined &&
          deepGet(tab, key).search(regexp) !== -1);
}

/**
 * Returns an array of matches that indicate where in the target string the
 * searchText appears. If there are no identified matches an empty array is
 * returned.
 * @param {string} target
 * @param {string} searchText
 * @return {!Array<!{start: number, length: number}>}
 */
function getRanges(target, searchText) {
  const escapedText = quoteString(searchText);
  const ranges = [];
  let match = null;
  for (const re = new RegExp(escapedText, 'gi'); match = re.exec(target);) {
    ranges.push({
      start : match.index,
      length : searchText.length,
    });
  }
  return ranges;
}

/**
 * A scoring function based on match indices of specified search fields.
 * Matches near the beginning of the string will have a higher score than
 * matches near the end of the string. Multiple matches will have a higher score
 * than single matches.
 * @param {!TabData} tabData
 * @param {number} distance
 * @param {!Object} searchFieldWeights
 */
function scoringFunction(tabData, distance, searchFieldWeights) {
  let score = 0;
  // For every match, map the match index in [0, distance] to a scalar value in
  // [1, 0].
  for (const key in searchFieldWeights) {
    if (tabData.highlightRanges[key]) {
      for (const {start} of tabData.highlightRanges[key]) {
        score += Math.max((distance - start) / distance, 0) *
            searchFieldWeights[key];
      }
    }
  }

  return score;
}
