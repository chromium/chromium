// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {KeyValuePairEntry} from './key_value_pair_entry.js';

// Contents of lines that act as delimiters for multi-line values.
const DELIM_START = '---------- START ----------';
const DELIM_END = '---------- END ----------';

/**
 * Convert text-based log into list of name-value pairs.
 * @param text The raw text of a log.
 * @return The parse result or null if any parsing error occurred.
 */
export function parseKeyValuePairEntry(text: string): KeyValuePairEntry[]|null {
  const details = [];
  const lines = text.split('\n');
  for (let i = 0, len = lines.length; i < len; i++) {
    // Skip empty lines.
    if (!lines[i]) {
      continue;
    }

    const delimiter = lines[i]!.indexOf('=');
    if (delimiter <= 0) {
      if (i === lines.length - 1) {
        break;
      }
      // If '=' is missing here, format is wrong.
      return null;
    }

    const name = lines[i]!.substring(0, delimiter);
    let value = '';
    // Set value if non-empty
    if (lines[i]!.length > delimiter + 1) {
      value = lines[i]!.substring(delimiter + 1);
    }

    // Delimiters are based on kMultilineIndicatorString, kMultilineStartString,
    // and kMultilineEndString in components/feedback/feedback_data.cc.
    // If these change, we should check for both the old and new versions.
    if (value === '<multiline>') {
      // Skip start delimiter.
      if (i === len - 1 || lines[++i]!.indexOf(DELIM_START) === -1) {
        return null;
      }

      ++i;
      value = '';
      // Append lines between start and end delimiters.
      while (i < len && lines[i] !== DELIM_END) {
        value += lines[i++] + '\n';
      }

      // Remove trailing newline.
      if (value) {
        value = value.substr(0, value.length - 1);
      }
    }
    details.push({key: name, value: value});
  }

  return details;
}
