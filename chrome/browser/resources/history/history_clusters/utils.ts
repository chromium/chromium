// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {highlight} from 'chrome://resources/js/search_highlight_utils.js';
import {quoteString} from 'chrome://resources/js/util.m.js';

type Range = {
  start: number,
  length: number,
};

/**
 * Returns an array of ranges indicating where in the text the query appears.
 * Returns an empty array if no matches are found.
 * TODO(crbug.com/1295121): Move this logic to a cross-platform location to be
 * shared by various surfaces.
 */
function getRanges(text: string, query: string): Range[] {
  if (!text || !query) {
    return [];
  }
  const escapedText = quoteString(query);
  const ranges: Range[] = [];
  let match = null;
  for (const re = new RegExp(escapedText, 'gi'); match = re.exec(text);) {
    ranges.push({
      start: match.index,
      length: query.length,
    });
  }
  return ranges;
}

/**
 * Populates the container with the highlighted text based on the given query.
 */
export function insertHighlightedTextIntoElement(
    container: HTMLElement, text: string, query: string) {
  const ranges = getRanges(text, query);
  container.textContent = '';
  const node = document.createTextNode(text);
  container.appendChild(node);
  if (ranges.length > 0) {
    highlight(node, ranges);
  }
}
