// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';
import type {TemplateResult} from '//resources/lit/v3_0/lit.rollup.js';
import type {Range, SearchOptions as SharedSearchOptions} from '/tab_group_shared/search.js';
import {search as sharedSearch} from '/tab_group_shared/search.js';

export interface OptionKeyObject<T> {
  name: string;
  getter: (data: T) => string[] | undefined;
  weight: number;
}

export interface SearchOptions<T> {
  includeScore?: boolean;
  includeMatches?: boolean;
  ignoreLocation?: boolean;
  threshold?: number;
  distance?: number;
  keys: Array<OptionKeyObject<T>>;
}

/**
 * Searches |records| for |input| across multi-part string fields specified in
 * |options.keys|. Parts within each field are joined with a separator during
 * search so matches cannot span across part boundaries, and the resulting
 * match ranges are sliced back into per-part Range[][] arrays on each matched
 * item's `highlightRanges`.
 */
export async function search<T>(
    input: string, records: T[], options: SearchOptions<T>): Promise<T[]> {
  if (input.length === 0) {
    return [...records];
  }

  const sharedOptions: SharedSearchOptions<T> = {
    ...options,
    keys: options.keys.map(
        key => ({
          name: key.name,
          // A newline character is used instead of a space because the search
          // input is single-line and cannot contain newlines, preventing search
          // queries from accidentally matching across part boundaries (e.g.,
          // matching the end of one title and the start of another in a split
          // view).
          getter: (item: T) => key.getter(item)?.join('\n'),
          weight: key.weight,
        })),
  };

  const results = await sharedSearch(input, records, sharedOptions);

  for (const result of results) {
    const rawHighlightRanges =
        (result as {highlightRanges?: Record<string, Range[]>}).highlightRanges;
    if (!rawHighlightRanges) {
      continue;
    }

    const slicedHighlightRanges: Record<string, Range[][]> = {};
    for (const key of options.keys) {
      const parts = key.getter(result);
      if (parts) {
        slicedHighlightRanges[key.name] =
            sliceRangesForParts(parts, rawHighlightRanges[key.name]);
      }
    }
    (result as {highlightRanges?: Record<string, Range[][]>}).highlightRanges =
        slicedHighlightRanges;
  }

  return results;
}

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
 * Splits a list of match ranges covering joined parts (separated by
 * |separator|) into a list of match ranges for each individual part.
 */
export function sliceRangesForParts(
    parts: string[], ranges?: Range[], separator: string = '\n'): Range[][] {
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
