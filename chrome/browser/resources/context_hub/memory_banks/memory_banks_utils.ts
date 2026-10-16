// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assertNotReachedCase} from '//resources/js/assert.js';

import {EntryType} from '../context_hub.mojom-webui.js';
import type {MemoryBankEntry} from '../context_hub.mojom-webui.js';

/**
 * Converts a Mojo Time (microseconds since the Windows epoch: Jan 1, 1601) to
 * a JavaScript Date object (milliseconds since the Unix epoch: Jan 1, 1970).
 * Returns null if mojoTime is null, undefined, or missing internalValue.
 */
export function convertMojoTimeToDate(mojoTime?: {internalValue: bigint}|null):
    Date|null {
  if (!mojoTime || mojoTime.internalValue === undefined ||
      mojoTime.internalValue === null) {
    return null;
  }
  // 11,644,473,600,000,000n is the Windows-to-Unix epoch delta in microseconds.
  const unixEpochUs = mojoTime.internalValue - 11644473600000000n;
  return new Date(Number(unixEpochUs / 1000n));
}

/**
 * Formats a single MemoryBankEntry as a structured Markdown document.
 */
export function formatEntryAsMarkdown(entry: MemoryBankEntry): string {
  const date = convertMojoTimeToDate(entry.timestamp);
  const dateStr = date ? date.toLocaleString() : '';
  const typeStr = getEntryTypeLabel(entry.type);
  const lines = [
    `## [${typeStr}]`,
    `- **Title:** ${entry.tabTitle}`,
    `- **URL:** ${entry.url}`,
  ];
  if (entry.collection) {
    lines.push(`- **Collection:** ${entry.collection}`);
  }
  if (entry.tags && entry.tags.length > 0) {
    lines.push(`- **Tags:** ${entry.tags.join(', ')}`);
  }
  if (dateStr) {
    lines.push(`- **Saved Date:** ${dateStr}`);
  }
  if (entry.note) {
    const formattedNote = entry.note.replace(/\r?\n/g, '\n  > ');
    lines.push(`- **Note:**\n  > ${formattedNote}`);
  }
  if (entry.selectedText) {
    const formattedContent = entry.selectedText.replace(/\r?\n/g, '\n  > ');
    lines.push(`- **Content:**\n  > ${formattedContent}`);
  }
  if (entry.imageUrl) {
    lines.push(`- **Image URL:** ${entry.imageUrl}`);
  }
  return lines.join('\n');
}

/**
 * Formats an array of MemoryBankEntry objects into a combined Markdown document
 * separated by horizontal rules.
 */
export function formatEntriesAsMarkdown(entries: MemoryBankEntry[]): string {
  return entries.map(entry => formatEntryAsMarkdown(entry)).join('\n\n---\n\n');
}

/**
 * Maps an entry type enum to its human-readable Markdown label.
 */
function getEntryTypeLabel(type: EntryType): string {
  switch (type) {
    case EntryType.kTab:
      return 'Saved Tab';
    case EntryType.kTextSelection:
      return 'Saved Text Selection';
    case EntryType.kImage:
      return 'Saved Image';
    default:
      assertNotReachedCase(type);
  }
}
