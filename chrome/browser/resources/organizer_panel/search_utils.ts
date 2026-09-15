// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';
import type {TemplateResult} from '//resources/lit/v3_0/lit.rollup.js';
import type {Range} from '/tab_search/shared/search.js';

/**
 * Renders |text| with <b> wrappers around matches if |ranges| are provided, or
 * returns the plain string if none.
 */
export function renderHighlightedText(
    text: string, ranges?: Range[]): TemplateResult|string {
  if (!ranges || ranges.length === 0) {
    return text;
  }

  const tokens: Array<TemplateResult|string> = [];
  let lastIndex = 0;

  for (const range of ranges) {
    if (range.start > lastIndex) {
      tokens.push(text.substring(lastIndex, range.start));
    }
    const matchText = text.substring(range.start, range.start + range.length);
    tokens.push(html`<b>${matchText}</b>`);
    lastIndex = range.start + range.length;
  }

  if (lastIndex < text.length) {
    tokens.push(text.substring(lastIndex));
  }

  return html`${tokens}`;
}

/**
 * Separator used to join multi-part text fields (such as title or description
 * parts) for search. A newline character is used instead of a space because the
 * search input is single-line and cannot contain newlines, preventing search
 * queries from accidentally matching across part boundaries (e.g., matching the
 * end of one title and the start of another in a split view).
 *
 * TODO(crbug.com/562032217): Consider handling this entirely within the search
 * util methods instead.
 */
export const SEARCH_PART_SEPARATOR: string = '\n';

/**
 * Splits a list of match ranges covering joined parts (separated by
 * |separator|) into a list of match ranges for each individual part.
 */
export function sliceRangesForParts(
    parts: string[], ranges?: Range[],
    separator: string = SEARCH_PART_SEPARATOR): Range[][] {
  if (!ranges || ranges.length === 0) {
    return parts.map(() => []);
  }
  const result: Range[][] = [];
  let offset = 0;
  for (const part of parts) {
    const partStart = offset;
    const partEnd = offset + part.length;
    const partRanges: Range[] = [];
    for (const range of ranges) {
      const rangeStart = range.start;
      const rangeEnd = range.start + range.length;
      const start = Math.max(rangeStart, partStart);
      const end = Math.min(rangeEnd, partEnd);
      if (end > start) {
        partRanges.push({start: start - partStart, length: end - start});
      }
    }
    result.push(partRanges);
    // Accounts for the separator added when joining parts for search.
    offset = partEnd + separator.length;
  }
  return result;
}
