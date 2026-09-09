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
 * Represents an auto-complete suggestion displayed in the search dropdown.
 */
export interface SearchSuggestion {
  /** The query string to set or append to the search box when chosen. */
  query: string;
  /** The text displayed in the suggestion list item. */
  label: string;
  /** Informational description shown alongside or below the label. */
  description?: string;
}

/**
 * Configuration for a search qualifier token supported by the search UI.
 */
interface QualifierDef {
  /** The primary prefix string (e.g. 'tag:'). */
  prefix: string;
  /** Human-readable explanation shown in the suggestions menu. */
  description: string;
}

/**
 * All recognized search qualifiers displayed in autocomplete suggestions.
 */
const QUALIFIERS: readonly QualifierDef[] = [
  {
    prefix: 'tag:',
    description: 'Filter by tag (e.g. tag:recipes)',
  },
  {
    prefix: 'collection:',
    description: 'Filter by collection (e.g. collection:Work)',
  },
  {
    prefix: 'type:',
    description: 'Filter by type (tab / selected_text)',
  },
  {
    prefix: 'title:',
    description: 'Filter by page title (e.g. title:"Wikipedia")',
  },
  {
    prefix: 'url:',
    description: 'Filter by URL or domain (e.g. url:github.com)',
  },
  {
    prefix: 'note:',
    description: 'Filter within notes (e.g. note:"todo")',
  },
  {
    prefix: 'text:',
    description: 'Filter by saved text snippet (e.g. text:"summary")',
  },
];

/** Maximum number of suggestions to display in the dropdown. */
const MAX_SUGGESTIONS = 10;

/** Candidate values suggested for "type:". */
const TYPE_CANDIDATES: readonly string[] = ['tab', 'selected_text'];

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

    const parsedToken = splitQualifier(token);
    if (parsedToken) {
      const {qualifier, value} = parsedToken;
      const val = value.trim();
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
 * Computes auto-complete search suggestions based on the user's current input.
 *
 * Suggestion behaviors:
 * 1. Empty or trailing-space input: suggests all qualifier prefixes (e.g.
 * "tag:", "type:").
 * 2. Token without colon: suggests matching qualifier prefixes (e.g. "ta" ->
 * "tag:").
 * 3. Token with qualifier prefix (e.g. "tag:", "collection:", "type:",
 * "title:"): suggests matching values for that qualifier or retains format
 * guidance.
 *
 * @param input The current search field input.
 * @param allTags Unique tags present across memory bank entries.
 * @param allCollections Unique collection names present across memory bank
 *     entries.
 * @return Up to MAX_SUGGESTIONS matching suggestions for the current token.
 */
export function computeSuggestions(
    input: string, allTags: string[],
    allCollections: string[]): SearchSuggestion[] {
  const lastBoundary = getLastTokenBoundary(input);
  const base = lastBoundary >= 0 ? input.slice(0, lastBoundary + 1) : '';
  const token = lastBoundary >= 0 ? input.slice(lastBoundary + 1) : input;

  const parsedToken = splitQualifier(token);
  if (parsedToken) {
    const {qualifier, value} = parsedToken;
    const cleanValue = value.replaceAll('"', '').trim();
    const qualifierDef = QUALIFIERS.find(q => q.prefix === `${qualifier}:`);

    if (!qualifierDef) {
      return [];
    }

    let candidates: readonly string[];
    switch (qualifier) {
      case 'tag':
        candidates = allTags;
        break;
      case 'collection':
        candidates = allCollections;
        break;
      case 'type':
        candidates = TYPE_CANDIDATES;
        break;
      default:
        // Free-text qualifiers (title:, url:, note:, text:) retain format
        // guidance while empty so the suggestions menu remains helpful.
        if (cleanValue === '') {
          return [{
            query: `${base}${qualifierDef.prefix}`,
            label: qualifierDef.prefix,
            description: qualifierDef.description,
          }];
        }
        return [];
    }

    return findMatches(candidates, cleanValue, MAX_SUGGESTIONS)
        .map(match => ({
               query: `${base}${qualifier}:${quoteIfSpaced(match)} `,
               label: `${qualifier}:${match}`,
               description: `Filter by ${qualifier} "${match}"`,
             }));
  }

  // Suggest matching qualifier prefixes (or all if token is empty).
  const lower = token.toLowerCase();
  return QUALIFIERS.filter(q => q.prefix.startsWith(lower))
      .map(q => ({
             query: `${base}${q.prefix}`,
             label: q.prefix,
             description: q.description,
           }));
}

/**
 * Finds the index of the whitespace separating the base query from the active
 * token being typed, ignoring spaces inside matching quotes.
 *
 * For example, in `collection:"Work Projects" tag:`, the space inside
 * `"Work Projects"` is ignored, and the index of the space after the closing
 * quote is returned.
 */
function getLastTokenBoundary(input: string): number {
  let lastBoundary = -1;
  let inQuotes = false;
  for (let i = 0; i < input.length; i++) {
    if (input[i] === '"' && (i === 0 || input[i - 1] !== '\\')) {
      inQuotes = !inQuotes;
    } else if ((input[i] === ' ' || input[i] === '\t') && !inQuotes) {
      lastBoundary = i;
    }
  }
  return lastBoundary;
}

/**
 * Splits a token into qualifier prefix and value if it contains a colon.
 * Returns null if the token does not contain a qualifier prefix.
 */
function splitQualifier(token: string): {qualifier: string, value: string}|
    null {
  const colonIndex = token.indexOf(':');
  if (colonIndex <= 0) {
    return null;
  }
  return {
    qualifier: token.slice(0, colonIndex).toLowerCase(),
    value: token.slice(colonIndex + 1),
  };
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

/**
 * Encloses a suggested value in quotes if it contains spaces (e.g.
 * `collection:"Work Projects"` instead of `collection:Work Projects`).
 *
 * This ensures that when a user selects a multi-word suggestion (such as a tag
 * or collection name with spaces), the search parser treats the entire string
 * as a single qualifier value rather than splitting subsequent words into
 * freeform search terms.
 */
function quoteIfSpaced(val: string): string {
  return val.includes(' ') ? `"${val}"` : val;
}

/**
 * Finds items starting with a query string up to a given limit.
 */
function findMatches(
    items: readonly string[], query: string, limit: number): string[] {
  const q = query.toLowerCase();
  const results: string[] = [];
  for (const item of items) {
    if (!q || item.toLowerCase().startsWith(q)) {
      results.push(item);
      if (results.length >= limit) {
        break;
      }
    }
  }
  return results;
}
