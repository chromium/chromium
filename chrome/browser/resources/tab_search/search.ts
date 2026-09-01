// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {loadTimeData} from 'chrome://resources/js/load_time_data.js';

import {SearchApiProxyImpl} from './search_api_proxy.js';

export interface Range {
  start: number;
  length: number;
}

// Regex covering Hiragana, Katakana, Kanji, and Hangul (CJK ranges).
const CJK_REGEX = new RegExp(
    '[\u3040-\u30ff\u3400-\u4dbf\u4e00-\u9fff\uf900-\ufaff' +
    '\uff66-\uff9f\u3131-\uD79D]');

let segmenter: Intl.Segmenter|null = null;

function getSegmenter(): Intl.Segmenter {
  if (!segmenter) {
    segmenter = new Intl.Segmenter(undefined, {granularity: 'word'});
  }
  return segmenter;
}

export interface OptionKeyObject<T = any> {
  name: string;
  getter: (data: T) => string | undefined;
  weight: number;
}

export interface SearchOptions<T = any> {
  includeScore?: boolean;
  includeMatches?: boolean;
  ignoreLocation?: boolean;
  threshold?: number;
  distance?: number;
  keys: Array<OptionKeyObject<T>>;
}

/**
 * @return A new array of entries satisfying the input. If no search input is
 *     present, returns a shallow copy of the records.
 */
export async function search<T>(
    input: string, records: T[], options: SearchOptions<T>): Promise<T[]> {
  if (input.length === 0) {
    return [...records];
  }
  const searchStartTime = Date.now();
  const result = await exactSearch(input, records, options);
  chrome.metricsPrivate?.recordTime?.(
      'Tabs.TabSearch.WebUI.SearchAlgorithmDuration',
      Math.round(Date.now() - searchStartTime));
  return result;
}

type WithHighlightRanges<T> = T&{highlightRanges: Record<string, Range[]>};

function cloneTabDataObj<T>(tabData: T): WithHighlightRanges<T> {
  const clone = Object.assign({}, tabData) as WithHighlightRanges<T>;
  clone.highlightRanges = {};
  Object.setPrototypeOf(clone, Object.getPrototypeOf(tabData));

  return clone;
}

/**
 * The exact match algorithm returns records ranked according to priorities
 * and scores. The search is case-insensitive and diacritic-insensitive (accents
 * are folded), offloading the character matching to the browser process via
 * Mojo.
 *
 * Records are ordered by priority (higher priority comes first) and sorted by
 * score within the same priority. See `scoringFunction` for how to calculate
 * score and `prioritizeMatchResult` for how to calculate priority.
 */
async function exactSearch<T>(
    searchText: string, records: T[], options: SearchOptions<T>): Promise<T[]> {
  if (searchText.length === 0) {
    return records;
  }

  // Default distance to calculate score for search fields based on match
  // position.
  const defaultDistance = 200;
  const distance = options.distance || defaultDistance;

  // Controls how heavily weighted the search field weights are relative to each
  // other in the scoring function.
  const searchFieldWeights = (options.keys).reduce((acc, {name, weight}) => {
    acc[name] = weight;
    return acc;
  }, {} as {[key: string]: number});

  // Batch all strings to search in.
  const targets: string[] = [];
  for (const record of records) {
    for (const searchField of options.keys) {
      const fieldText = searchField.getter(record);
      if (fieldText) {
        targets.push(fieldText);
      }
    }
  }

  // Query the browser process to find match ranges. The C++ backend handles
  // case-insensitivity, diacritic-insensitivity, and quotation folding
  // natively.
  const {ranges} =
      await SearchApiProxyImpl.getInstance().getRangesIgnoringCaseAndAccents(
          searchText, targets);

  // Perform an exact match search with range discovery.
  const exactMatches = [];
  let targetIdx = 0;
  for (const tabDataRecord of records) {
    let matchFound = false;
    const matchedRecord = cloneTabDataObj(tabDataRecord);
    // Searches for fields or nested fields in the record.
    for (const searchField of options.keys) {
      const fieldText = searchField.getter(tabDataRecord);
      if (fieldText) {
        const matchRanges = ranges[targetIdx++];
        if (matchRanges && matchRanges.length !== 0) {
          // Convert Mojo TokenRange DTOs (Data Transfer Objects) to plain JS
          // Range domain objects. This decouples the UI layers from
          // Mojo-specific serialization internals and ensures type safety.
          matchedRecord.highlightRanges[searchField.name] =
              matchRanges.map(r => ({start: r.start, length: r.length}));
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
      options.keys, exactMatches.map(item => item.tab), searchText);
}

/**
 * A scoring function based on match indices of specified search fields.
 * Matches near the beginning of the string will have a higher score than
 * matches near the end of the string. Multiple matches will have a higher score
 * than single matches.
 */
function scoringFunction(
    tabData: {highlightRanges: Record<string, Range[]>}, distance: number,
    searchFieldWeights: {[key: string]: number}) {
  let score = 0;
  // For every match, map the match index in [0, distance] to a scalar value in
  // [1, 0].
  for (const key in searchFieldWeights) {
    if (tabData.highlightRanges[key]) {
      for (const {start} of tabData.highlightRanges[key]) {
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
function prioritizeMatchResult<T>(
    searchFields: Array<OptionKeyObject<T>>,
    result: Array<WithHighlightRanges<T>>,
    input: string): Array<WithHighlightRanges<T>> {
  const itemsMatchingStringStart = [];
  const itemsMatchingWordStart = [];
  const others = [];

  const hasCjkQuery = CJK_REGEX.test(input);

  for (const tab of result) {
    let matchesStringStart = false;
    let matchesWordStart = false;

    for (const searchField of searchFields) {
      const matchRanges = tab.highlightRanges[searchField.name];
      if (!matchRanges || matchRanges.length === 0) {
        continue;
      }

      const fieldText = searchField.getter(tab);
      if (!fieldText) {
        continue;
      }

      for (const matchRange of matchRanges) {
        if (matchRange.start === 0) {
          matchesStringStart = true;
          break;
        }
        if (matchRange.start > 0 &&
            isWordStart(fieldText, matchRange.start, hasCjkQuery)) {
          matchesWordStart = true;
        }
      }
      // We can only early exit if we found a String Start match,
      // because a Word Start match in this field could still be overridden
      // by a String Start match in a subsequent field.
      if (matchesStringStart) {
        break;
      }
    }

    if (matchesStringStart) {
      itemsMatchingStringStart.push(tab);
    } else if (matchesWordStart) {
      itemsMatchingWordStart.push(tab);
    } else {
      others.push(tab);
    }
  }
  return itemsMatchingStringStart.concat(itemsMatchingWordStart, others);
}

/**
 * Returns true if the match starting at `index` aligns with a semantic word
 * boundary in `text`.
 */
function isWordStart(
    text: string, index: number, hasCjkQuery: boolean): boolean {
  // If CJK word boundary detection feature is enabled and the search query
  // contains CJK characters, use the locale-aware segmenter.
  if (loadTimeData.getBoolean('cjkWordBoundaryEnabled') && hasCjkQuery) {
    const currSegments = getSegmenter().segment(text);
    const currSegment = currSegments.containing(index);
    // segment.index represents the start index of the semantic word segment
    // containing `index`. We verify:
    // 1. The segment exists.
    // 2. The match starts EXACTLY at the beginning of this segment.
    // 3. The segment is a word-like token (not spaces or punctuation).
    return !!currSegment && currSegment.index === index &&
        !!currSegment.isWordLike;
  }

  // Fast fallback for non-CJK text where simple character boundaries are
  // sufficient.
  const prevChar = text.charAt(index - 1);
  return /\W/.test(prevChar);
}
