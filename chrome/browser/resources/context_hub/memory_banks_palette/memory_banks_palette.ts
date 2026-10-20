// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_elements/cr_icon/cr_icon.js';
import '//resources/cr_elements/cr_icon_button/cr_icon_button.js';
import '//resources/cr_elements/cr_search_field/cr_search_field.js';
import '//resources/cr_elements/icons.html.js';
import '../icons.html.js';

import type {PropertyValues} from '//resources/lit/v3_0/lit.rollup.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';

import type {MemoryBankEntry, MemoryBankEntryAnnotations} from '../context_hub.mojom-webui.js';
import {browserProxyFactory} from '../context_hub.mojom-webui.js';
import type {ParsedSearchQuery} from '../memory_banks/memory_banks_search.js';
import {matchesMemoryBankEntry, parseSearchQuery} from '../memory_banks/memory_banks_search.js';
import {convertMojoTimeToDate} from '../memory_banks/memory_banks_utils.js';

import {getCss} from './memory_banks_palette.css.js';
import {getHtml} from './memory_banks_palette.html.js';

/** Minimum character length of a snippet before offering an expand toggle. */
const SNIPPET_PREVIEW_CHAR_THRESHOLD = 90;

const ONE_MINUTE_MS = 60 * 1000;
const MINUTES_PER_HOUR = 60;
const HOURS_PER_DAY = 24;
const DAYS_PER_WEEK = 7;
const ONE_HOUR_MS = MINUTES_PER_HOUR * ONE_MINUTE_MS;
const ONE_DAY_MS = HOURS_PER_DAY * ONE_HOUR_MS;

export interface DisplayItem {
  title: string;
  snippet: string;
  collection: string;
  tags: string[];
  url: string;
  relativeTime?: string;
}

export class MemoryBanksPaletteElement extends CrLitElement {
  static get is() {
    return 'memory-banks-palette';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      entries: {type: Array},
      collections: {type: Array},
      tags: {type: Array},
      displayedItems: {type: Array},
      searchQuery: {type: String},
      filterType: {type: String},
      expandedIndex: {type: Number},
    };
  }

  accessor entries: MemoryBankEntry[] = [];
  accessor collections: string[] = [];
  accessor tags: string[] = [];
  accessor displayedItems: DisplayItem[] = [];
  accessor searchQuery: string = '';
  accessor filterType: 'all'|'collections'|'tags'|'memories' = 'all';
  accessor expandedIndex: number = -1;

  private listenerIds: number[] = [];
  private sortedCollections: string[] = [];
  private sortedTags: string[] = [];
  private sortedEntries: MemoryBankEntry[] = [];
  private entriesByCollection: Map<string, MemoryBankEntry[]> = new Map();
  private entriesByTag: Map<string, MemoryBankEntry[]> = new Map();
  private rtf: Intl.RelativeTimeFormat =
      new Intl.RelativeTimeFormat(undefined, {numeric: 'auto', style: 'short'});

  override connectedCallback() {
    super.connectedCallback();
    const callbackRouter = browserProxyFactory.getInstance().callbackRouter;
    this.listenerIds.push(
        callbackRouter.onMemoryBankEntryAdded.addListener(
            (entry: MemoryBankEntry) => {
              this.entries = [
                entry,
                ...this.entries.filter(existing => existing.id !== entry.id),
              ];
              this.fetchCollectionsAndTags();
            }),
        callbackRouter.onMemoryBankEntryUpdated.addListener(
            (id: bigint, annotations: MemoryBankEntryAnnotations) => {
              this.entries = this.entries.map(
                  existing => existing.id === id ? {
                    ...existing,
                    collection: annotations.collection,
                    note: annotations.note,
                    tags: annotations.tags ?? [],
                  } :
                                                   existing);
              this.fetchCollectionsAndTags();
            }),
        callbackRouter.onMemoryBankEntriesDeleted.addListener(
            (ids: bigint[]) => {
              const deletedIds = new Set(ids);
              this.entries =
                  this.entries.filter(entry => !deletedIds.has(entry.id));
              this.fetchCollectionsAndTags();
            }));
    this.fetchData();
  }

  override disconnectedCallback() {
    super.disconnectedCallback();
    const callbackRouter = browserProxyFactory.getInstance().callbackRouter;
    this.listenerIds.forEach(id => callbackRouter.removeListener(id));
    this.listenerIds = [];
  }

  override willUpdate(changedProperties: PropertyValues<this>) {
    super.willUpdate(changedProperties);

    if (changedProperties.has('entries') ||
        changedProperties.has('collections') || changedProperties.has('tags')) {
      this.updateCachedEntryStructures();
    }

    if (changedProperties.has('searchQuery') ||
        changedProperties.has('filterType') ||
        changedProperties.has('entries') ||
        changedProperties.has('collections') || changedProperties.has('tags')) {
      this.expandedIndex = -1;
      this.displayedItems = this.computeDisplayedItems();
    }
  }

  private async fetchData() {
    const proxy = browserProxyFactory.getInstance().handler;
    const [{entries}, {collections}, {tags}] = await Promise.all([
      proxy.getAllMemoryBankEntries(),
      proxy.getAllMemoryBankCollections(),
      proxy.getAllMemoryBankTags(),
    ]);

    this.entries = entries || [];
    this.collections = collections || [];
    this.tags = tags || [];
  }

  private async fetchCollectionsAndTags() {
    const proxy = browserProxyFactory.getInstance().handler;
    const [{collections}, {tags}] = await Promise.all([
      proxy.getAllMemoryBankCollections(),
      proxy.getAllMemoryBankTags(),
    ]);
    this.collections = collections || [];
    this.tags = tags || [];
  }

  // TODO(b/574624499): Move #tag shorthand parsing into parseSearchQuery()
  // in memory_banks_search.ts so it is unified across all search interfaces.
  private parsePaletteSearchQuery(rawQuery: string): ParsedSearchQuery {
    const parsed = parseSearchQuery(rawQuery);
    const remainingFreeform: string[] = [];
    for (const term of parsed.freeform) {
      if (term.startsWith('#')) {
        parsed.tag.push(term.slice(1));
      } else {
        remainingFreeform.push(term);
      }
    }
    parsed.freeform = remainingFreeform;
    return parsed;
  }

  private formatRelativeTime(mojoTime?: {internalValue: bigint}|null): string {
    const date = convertMojoTimeToDate(mojoTime);
    if (!date) {
      return '';
    }
    const diffMs = date.getTime() - Date.now();
    const diffMins = Math.round(diffMs / ONE_MINUTE_MS);
    if (Math.abs(diffMins) < 1) {
      return 'Just now';
    }
    if (Math.abs(diffMins) < MINUTES_PER_HOUR) {
      return this.rtf.format(diffMins, 'minute');
    }
    const diffHours = Math.round(diffMs / ONE_HOUR_MS);
    if (Math.abs(diffHours) < HOURS_PER_DAY) {
      return this.rtf.format(diffHours, 'hour');
    }
    const diffDays = Math.round(diffMs / ONE_DAY_MS);
    if (Math.abs(diffDays) < DAYS_PER_WEEK) {
      return this.rtf.format(diffDays, 'day');
    }
    return date.toLocaleDateString(undefined, {month: 'short', day: 'numeric'});
  }

  private updateCachedEntryStructures() {
    // Sort collections alphabetically (A-Z).
    this.sortedCollections = [...this.collections].sort(
        (a, b) => a.localeCompare(
            b, undefined, {sensitivity: 'base', numeric: true}));

    // Sort tags alphabetically (A-Z).
    this.sortedTags = [...this.tags].sort(
        (a, b) => a.localeCompare(
            b, undefined, {sensitivity: 'base', numeric: true}));

    // Sort entries newest first (descending timestamp).
    this.sortedEntries = [...this.entries].sort((a, b) => {
      const timeA = a.timestamp?.internalValue ?? 0n;
      const timeB = b.timestamp?.internalValue ?? 0n;
      return timeB > timeA ? 1 : (timeB < timeA ? -1 : 0);
    });

    // Build lookup maps keyed by lowercase collection and tag names.
    this.entriesByCollection = new Map();
    this.entriesByTag = new Map();
    for (const entry of this.sortedEntries) {
      if (entry.collection) {
        const colKey = entry.collection.toLowerCase();
        const colList = this.entriesByCollection.get(colKey);
        if (colList) {
          colList.push(entry);
        } else {
          this.entriesByCollection.set(colKey, [entry]);
        }
      }
      if (entry.tags) {
        const seenTagKeys = new Set<string>();
        for (const tag of entry.tags) {
          const tagKey = tag.toLowerCase();
          if (seenTagKeys.has(tagKey)) {
            continue;
          }
          seenTagKeys.add(tagKey);
          const tagList = this.entriesByTag.get(tagKey);
          if (tagList) {
            tagList.push(entry);
          } else {
            this.entriesByTag.set(tagKey, [entry]);
          }
        }
      }
    }
  }

  private computeDisplayedItems(): DisplayItem[] {
    const queryLower = this.searchQuery.trim().toLowerCase();
    const hasSearch = queryLower.length > 0;
    const items: DisplayItem[] = [];

    // 1. Collections (alphabetical A-Z)
    if (this.filterType === 'all' || this.filterType === 'collections') {
      for (const col of this.sortedCollections) {
        const colLower = col.toLowerCase();
        if (!hasSearch || colLower.includes(queryLower)) {
          const entries = this.entriesByCollection.get(colLower) ?? [];
          items.push({
            title: col,
            snippet: `${entries.length} ${
                entries.length === 1 ? 'memory' : 'memories'} in collection`,
            collection: col,
            tags: [],
            url: '',
          });
        }
      }
    }

    // 2. Tags (alphabetical A-Z)
    if (this.filterType === 'all' || this.filterType === 'tags') {
      for (const tag of this.sortedTags) {
        const tagLower = tag.toLowerCase();
        if (!hasSearch || `#${tagLower}`.includes(queryLower)) {
          const entries = this.entriesByTag.get(tagLower) ?? [];
          items.push({
            title: `#${tag}`,
            snippet: `${entries.length} ${
                entries.length === 1 ? 'memory' : 'memories'} tagged`,
            collection: '',
            tags: [tag],
            url: '',
          });
        }
      }
    }

    // 3. Individual Memories (recent date, newest first)
    if (this.filterType === 'all' || this.filterType === 'memories') {
      const parsedQuery = this.parsePaletteSearchQuery(queryLower);
      const entriesToRender = hasSearch ?
          this.sortedEntries.filter(
              e => matchesMemoryBankEntry(e, parsedQuery)) :
          this.sortedEntries;
      for (const entry of entriesToRender) {
        const snippet = entry.selectedText && entry.note ?
            `${entry.selectedText}\nNote: ${entry.note}` :
            (entry.selectedText || entry.note || '');
        items.push({
          title: entry.tabTitle || 'Untitled Memory',
          snippet,
          collection: entry.collection || '',
          tags: entry.tags || [],
          url: entry.url || '',
          relativeTime: this.formatRelativeTime(entry.timestamp),
        });
      }
    }

    return items;
  }

  protected onFilterAllClick() {
    this.filterType = 'all';
  }

  protected onFilterCollectionsClick() {
    this.filterType = 'collections';
  }

  protected onFilterTagsClick() {
    this.filterType = 'tags';
  }

  protected onFilterMemoriesClick() {
    this.filterType = 'memories';
  }

  protected isHttpOrHttpsUrl(urlStr: string): boolean {
    if (!urlStr) {
      return false;
    }
    try {
      const {protocol} = new URL(urlStr);
      return protocol === 'http:' || protocol === 'https:';
    } catch {
      return false;
    }
  }

  protected async onOpenSourceClick(e: Event) {
    e.stopPropagation();
    const target = e.currentTarget as HTMLElement;
    const url = target.dataset['url'];
    if (!url || !this.isHttpOrHttpsUrl(url)) {
      return;
    }
    await browserProxyFactory.getInstance().handler.openWebUrl(url);
    window.close();
  }

  protected onTogglePreviewClick(e: Event) {
    e.stopPropagation();
    const target = e.currentTarget as HTMLElement;
    const indexStr = target.dataset['index'];
    if (indexStr !== undefined) {
      const index = Number(indexStr);
      this.expandedIndex = this.expandedIndex === index ? -1 : index;
    }
  }

  protected shouldShowPreviewToggle(snippet: string): boolean {
    return !!snippet &&
        (snippet.length > SNIPPET_PREVIEW_CHAR_THRESHOLD ||
         snippet.includes('\n'));
  }

  protected onSearchChanged(e: CustomEvent<string>) {
    this.searchQuery = e.detail;
  }

  protected onCloseClick() {
    window.close();
  }

  protected async onSettingsClick() {
    await browserProxyFactory.getInstance().handler.openContextHubPage();
    window.close();
  }

  protected getEmptyStateTitle(): string {
    if (this.searchQuery.trim()) {
      return 'No matching items found';
    }
    if (this.filterType === 'collections') {
      return 'No collections saved yet';
    }
    if (this.filterType === 'tags') {
      return 'No tags saved yet';
    }
    return 'No memories saved yet';
  }

  protected getEmptyStateSubtitle(): string {
    if (this.searchQuery.trim()) {
      return 'Try searching with different keywords, #tags, or collections.';
    }
    if (this.filterType === 'collections') {
      return 'Assign a collection when saving a memory to organize your bank.';
    }
    if (this.filterType === 'tags') {
      return 'Add #tags when saving a memory to categorize your bank.';
    }
    return 'Select text on any page and right click "Save to Memory" ' +
        'to start building your bank.';
  }

  protected getHostname(urlStr: string): string {
    try {
      const url = new URL(urlStr);
      return url.hostname.replace(/^www\./, '');
    } catch {
      return urlStr;
    }
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'memory-banks-palette': MemoryBanksPaletteElement;
  }
}

customElements.define(MemoryBanksPaletteElement.is, MemoryBanksPaletteElement);
