// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {quoteString} from 'chrome://resources/js/util.m.js';

import Fuse from './fuse.js';

/**
 * TODO(tluk): Fix the typing for tabSearch.mojom.Tab here given we are updating
 * the fields on this object ( https://crbug.com/1133558 ).
 * @suppress {checkTypes}
 * @param {string} input
 * @param {!Array<!tabSearch.mojom.Tab>} records
 * @param {!Object} options
 * @return {!Array<!tabSearch.mojom.Tab>}
 */
export function fuzzySearch(input, records, options) {
  if (input.length === 0) {
    return records;
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
  if (options.threshold === 0.0) {
    return exactSearch(input, records);
  } else {
    return new Fuse(records, options).search(input).map(result => {
      const titleMatch = result.matches.find(e => e.key === 'title');
      const hostnameMatch = result.matches.find(e => e.key === 'hostname');
      const item = Object.assign({}, result.item);
      if (titleMatch) {
        item.titleHighlightRanges = convertToRanges(titleMatch.indices);
      }
      if (hostnameMatch) {
        item.hostnameHighlightRanges = convertToRanges(hostnameMatch.indices);
      }
      return item;
    });
  }
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
 * @suppress {checkTypes}
 * @param {string} searchText
 * @param {!Array<!tabSearch.mojom.Tab>} records
 * @return {!Array<!tabSearch.mojom.Tab>}
 */
function exactSearch(searchText, records) {
  if (searchText.length === 0) {
    return records;
  }

  // Perform an exact match search with range discovery.
  const exactMatches = [];
  for (let tab of records) {
    const titleHighlightRanges = getRanges(tab.title, searchText);
    const hostnameHighlightRanges = getRanges(tab.hostname, searchText);
    if (!titleHighlightRanges.length && !hostnameHighlightRanges.length) {
      continue;
    }
    const matchedTab = Object.assign({}, tab);
    if (titleHighlightRanges.length) {
      matchedTab.titleHighlightRanges = titleHighlightRanges;
    }
    if (hostnameHighlightRanges.length) {
      matchedTab.hostnameHighlightRanges = hostnameHighlightRanges;
    }
    exactMatches.push(matchedTab);
  }

  // Prioritize items.
  const itemsMatchingStringStart = [];
  const itemsMatchingWordStart = [];
  const others = [];
  const wordStartRegexp = new RegExp(`\\b${quoteString(searchText)}`, 'i');
  for (let tab of exactMatches) {
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
 * @suppress {checkTypes}
 * @param {!tabSearch.mojom.Tab} tab
 * @return {boolean}
 */
function hasMatchStringStart(tab) {
  return (tab.titleHighlightRanges &&
          tab.titleHighlightRanges[0].start === 0) ||
         (tab.hostnameHighlightRanges &&
          tab.hostnameHighlightRanges[0].start === 0);
}

/**
 * Determines whether the given tab has a match for the given regexp in its
 * title or hostname.
 * @suppress {checkTypes}
 * @param {!tabSearch.mojom.Tab} tab
 * @param {RegExp} regexp
 * @return {boolean}
 */
function hasRegexMatch(tab, regexp) {
  return (tab.titleHighlightRanges && tab.title.search(regexp) !== -1) ||
         (tab.hostnameHighlightRanges && tab.hostname.search(regexp) !== -1);
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
  let ranges = [];
  let match = null;
  for (const re = new RegExp(escapedText, 'gi'); match = re.exec(target);) {
    ranges.push({
      start : match.index,
      length : searchText.length,
    });
  }
  return ranges;
}
