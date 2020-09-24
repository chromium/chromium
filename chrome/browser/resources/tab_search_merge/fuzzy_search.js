// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import Fuse from './fuse.js';

/**
 * @param {string} input
 * @param {!Array<!tabSearch.mojom.Tab>} records
 * @param {!Object} options
 * @return {!Array<!tabSearch.mojom.Tab>}
 */
export function fuzzySearch(input, records, options) {
  if (input.length === 0) {
    return records;
  }
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
