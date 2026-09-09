// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {EntryType} from '../context_hub.mojom-webui.js';
import type {MemoryBankEntry} from '../context_hub.mojom-webui.js';

/**
 * Represents structured search criteria parsed from a user's query string.
 *
 * Different qualifiers and freeform terms are evaluated in conjunction (AND),
 * while multiple values for the same qualifier match any (OR).
 */
export interface ParsedSearchQuery {
  /** Tag filters parsed from "tag:<val>" (matches entry.tags). */
  tag: string[];
  /** Collection filters parsed from "collection:<val>". */
  collection: string[];
  /** Note filters parsed from "note:<val>". */
  note: string[];
  /** URL filters parsed from "url:<val>". */
  url: string[];
  /** Title filters parsed from "title:<val>" (matches entry.tabTitle). */
  title: string[];
  /**
   * Text snippet filters parsed from "text:<val>" (matches
   * entry.selectedText).
   */
  text: string[];
  /**
   * Entry type filters parsed from "type:<val>" (matches entry.type; e.g.
   * "tab", "selected_text").
   */
  entryType: string[];
  /** Freeform search terms matching any searchable property. */
  freeform: string[];
}

/**
 * Parses a raw search query string into structured filter fields.
 *
 * Supports qualifiers (e.g. `tag:recipes`, `collection:"My Project"`,
 * `type:tab`, `type:selected_text`), quoted phrases, and freeform terms.
 *
 * @param rawQuery The raw query input string.
 * @return Structured filters split into specific qualifiers and freeform terms.
 */
export function parseSearchQuery(rawQuery: string): ParsedSearchQuery {
  const result: ParsedSearchQuery = {
    tag: [],
    collection: [],
    note: [],
    url: [],
    title: [],
    text: [],
    entryType: [],
    freeform: [],
  };

  if (!rawQuery?.trim()) {
    return result;
  }

  // Split query into tokens, keeping quoted phrases together.
  const tokens = rawQuery.match(/(?:[^\s"]+|"[^"]*")+/g) || [];

  for (const rawToken of tokens) {
    const token = rawToken.replaceAll('"', '').trim();
    if (!token) {
      continue;
    }

    const colonIndex = token.indexOf(':');
    if (colonIndex > 0) {
      const qualifier = token.slice(0, colonIndex).toLowerCase();
      const val = token.slice(colonIndex + 1).trim();
      if (val) {
        switch (qualifier) {
          case 'tag':
          case 'collection':
          case 'note':
          case 'url':
          case 'title':
          case 'text':
            result[qualifier].push(val);
            continue;
          case 'type':
            // Maps the user-facing "type:" qualifier to the entryType field.
            result.entryType.push(val);
            continue;
          default:
            // Unrecognized qualifier; fall through to freeform search.
            break;
        }
      }
    }

    result.freeform.push(token);
  }

  return result;
}

/**
 * Evaluates whether a given memory bank entry matches all criteria in a parsed
 * query.
 *
 * Matching requires:
 * 1. Multiple values for the same qualifier match ANY (OR).
 * 2. Filters across different qualifiers must all match (AND).
 * 3. Each freeform search term must match at least one field of the entry
 * (AND).
 *
 * @param entry The memory bank entry to test.
 * @param parsed The parsed search criteria.
 * @return True if the entry matches all search criteria.
 */
export function matchesMemoryBankEntry(
    entry: MemoryBankEntry, parsed: ParsedSearchQuery): boolean {
  // Entry must match at least one tag filter if any were specified (OR).
  if (parsed.tag.length > 0 &&
      !parsed.tag.some(q => entry.tags.some(t => matches(t, q)))) {
    return false;
  }

  // All qualifier criteria must be satisfied (AND across qualifiers).
  // Within each qualifier, matching any of its values succeeds (OR).
  if (!matchesAny(parsed.collection, entry.collection) ||
      !matchesAny(parsed.title, entry.tabTitle) ||
      !matchesAny(parsed.url, entry.url) ||
      !matchesAny(parsed.note, entry.note) ||
      !matchesAny(parsed.text, entry.selectedText) ||
      !matchesAny(parsed.entryType, getEntryTypeName(entry.type))) {
    return false;
  }

  // Check that every freeform term matches at least one field on the entry.
  for (const term of parsed.freeform) {
    const matched = matches(entry.tabTitle, term) || matches(entry.url, term) ||
        matches(entry.selectedText, term) || matches(entry.note, term) ||
        matches(entry.collection, term) ||
        entry.tags.some(t => matches(t, term));
    if (!matched) {
      return false;
    }
  }

  return true;
}

/**
 * Performs a case-insensitive substring match against a string.
 */
function matches(value: string|null|undefined, query: string): boolean {
  return Boolean(value?.toLowerCase().includes(query.toLowerCase()));
}

/**
 * Returns true if no queries are specified (qualifier was not used, relying on
 * freeform search), or if at least one query matches the value (OR).
 */
function matchesAny(queries: string[], value: string|null|undefined): boolean {
  return queries.length === 0 || queries.some(q => matches(value, q));
}

/**
 * Maps an entry type enum to its queryable string name ('tab' or
 * 'selected_text').
 */
function getEntryTypeName(type: EntryType): string {
  return type === EntryType.kTab ? 'tab' : 'selected_text';
}
