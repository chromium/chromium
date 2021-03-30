// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {quoteString} from 'chrome://resources/js/util.m.js';

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
  performance.mark('search_algorithm:benchmark_begin');
  let result;
  if (options.threshold === 0.0) {
    result = exactSearch(input, records, options);
  } else {
    result = new Fuse(records, options).search(input).map(result => {
      const titleMatch = result.matches.find(e => e.key === 'tab.title');
      const hostnameMatch = result.matches.find(e => e.key === 'hostname');
      const item = cloneTabDataObj(result.item);
      if (titleMatch) {
        item.titleHighlightRanges = convertToRanges(titleMatch.indices);
      }
      if (hostnameMatch) {
        item.hostnameHighlightRanges = convertToRanges(hostnameMatch.indices);
      }
      return item;
    });
  }
  performance.mark('search_algorithm:benchmark_end');
  return result;
}

/**
 * @param {!TabData} tabData
 * @return {!TabData}
 */
function cloneTabDataObj(tabData) {
  const clone = Object.assign({}, tabData);
  Object.setPrototypeOf(clone, TabData.prototype);
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
 * 1. All items with |title| or |hostname| matching the searchText at the
 *    beginning of the string.
 * 2. All items with |title| or |hostname| matching the searchText at the
 *    beginning of a word in the string.
 * 3. All remaining items with |title| or |hostname| matching the searchText
 *    elsewhere in the string.
 * @param {string} searchText
 * @param {!Array<!TabData>} records
 * @param {!Object} options
 * @return {!Array<!TabData>}
 */
function exactSearch(searchText, records, options) {
  if (searchText.length === 0) {
    return records;
  }

  // Controls how heavily weighted the tab's title is relative to the hostname
  // in the scoring function.
  const key =
      options.keys ? options.keys.find(e => e.name === 'tab.title') : undefined;
  const titleToHostnameWeightRatio = key ? key.weight : 1;
  // Default distance to calculate score for title/hostname based on match
  // position.
  const defaultDistance = 200;
  const distance = options.distance || defaultDistance;

  // Perform an exact match search with range discovery.
  const exactMatches = [];
  for (const tab of records) {
    const titleHighlightRanges = getRanges(tab.tab.title, searchText);
    const hostnameHighlightRanges = getRanges(tab.hostname, searchText);
    if (!titleHighlightRanges.length && !hostnameHighlightRanges.length) {
      continue;
    }
    const matchedTab = cloneTabDataObj(tab);
    if (titleHighlightRanges.length) {
      matchedTab.titleHighlightRanges = titleHighlightRanges;
    }
    if (hostnameHighlightRanges.length) {
      matchedTab.hostnameHighlightRanges = hostnameHighlightRanges;
    }
    exactMatches.push({
      tab: matchedTab,
      score: scoringFunction(matchedTab, distance, titleToHostnameWeightRatio)
    });
  }

  // Sort by score.
  exactMatches.sort((a, b) => (b.score - a.score));

  // Prioritize items.
  const itemsMatchingStringStart = [];
  const itemsMatchingWordStart = [];
  const others = [];
  const wordStartRegexp = new RegExp(`\\b${quoteString(searchText)}`, 'i');
  for (const {tab} of exactMatches) {
    // Find matches that occur at the beginning of the string.
    if (hasMatchStringStart(tab)) {
      itemsMatchingStringStart.push(tab);
    } else if (hasRegexMatch(tab, wordStartRegexp)) {
      itemsMatchingWordStart.push(tab);
    } else {
      others.push(tab);
    }
  }
  return itemsMatchingStringStart.concat(itemsMatchingWordStart, others);
}

/**
 * Determines whether the given tab has a title or hostname with identified
 * matches at the beginning of the string.
 * @param {!TabData} tab
 * @return {boolean}
 */
function hasMatchStringStart(tab) {
  return (tab.titleHighlightRanges !== undefined &&
          tab.titleHighlightRanges[0].start === 0) ||
      (tab.hostnameHighlightRanges !== undefined &&
       tab.hostnameHighlightRanges[0].start === 0);
}

/**
 * Determines whether the given tab has a match for the given regexp in its
 * title or hostname.
 * @param {!TabData} tab
 * @param {RegExp} regexp
 * @return {boolean}
 */
function hasRegexMatch(tab, regexp) {
  return (tab.titleHighlightRanges !== undefined &&
          tab.tab.title.search(regexp) !== -1) ||
      (tab.hostnameHighlightRanges !== undefined &&
       tab.hostname.search(regexp) !== -1);
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
 * A scoring function based on match indices of title and hostname.
 * Matches near the beginning of the string will have a higher score than
 * matches near the end of the string. Multiple matches will have a higher score
 * than single matches.
 * @param {!TabData} tab
 * @param {number} distance
 * @param {number} titleToHostnameWeightRatio
 */
function scoringFunction(tab, distance, titleToHostnameWeightRatio) {
  let score = 0;
  // For every match, map the match index in [0, distance] to a scalar value in
  // [1, 0].
  if (tab.titleHighlightRanges) {
    for (const {start} of tab.titleHighlightRanges) {
      score += Math.max((distance - start) / distance, 0) *
          titleToHostnameWeightRatio;
    }
  }
  if (tab.hostnameHighlightRanges) {
    for (const {start} of tab.hostnameHighlightRanges) {
      score += Math.max((distance - start) / distance, 0);
    }
  }
  return score;
}
