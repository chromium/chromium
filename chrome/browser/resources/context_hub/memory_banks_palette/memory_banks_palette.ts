// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_elements/cr_button/cr_button.js';
import '//resources/cr_elements/cr_icon/cr_icon.js';
import '//resources/cr_elements/cr_icon_button/cr_icon_button.js';
import '//resources/cr_elements/cr_search_field/cr_search_field.js';
import '//resources/cr_elements/cr_toast/cr_toast.js';
import '//resources/cr_elements/icons.html.js';
import '../icons.html.js';

import type {CrToastElement} from '//resources/cr_elements/cr_toast/cr_toast.js';
import {isMac} from '//resources/js/platform.js';
import type {PropertyValues} from '//resources/lit/v3_0/lit.rollup.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';

import type {MemoryBankEntry, MemoryBankEntryAnnotations, MemoryBankFileAttachment} from '../context_hub.mojom-webui.js';
import {browserProxyFactory, CopyAttachmentsToClipboardStatus} from '../context_hub.mojom-webui.js';
import type {ParsedSearchQuery} from '../memory_banks/memory_banks_search.js';
import {matchesMemoryBankEntry, parseSearchQuery} from '../memory_banks/memory_banks_search.js';
import {convertMojoTimeToDate, formatEntriesAsMarkdown} from '../memory_banks/memory_banks_utils.js';

import {getCss} from './memory_banks_palette.css.js';
import {getHtml} from './memory_banks_palette.html.js';

/** Duration in milliseconds to display the copy confirmation toast. */
const COPY_TOAST_DURATION_MS = 1800;

/** Duration in milliseconds to display the file attachment toast. */
const ATTACH_TOAST_DURATION_MS = 2800;

/** Minimum character length of a snippet before offering an expand toggle. */
const SNIPPET_PREVIEW_CHAR_THRESHOLD = 90;

/**
 * Maximum number of files that can be staged to the clipboard at once.
 * Must not exceed `kMaxFilesToCopy` in `context_hub_page_handler.cc`.
 */
const MAX_ATTACHED_FILES = 10;

/** Max character length for an attachment filename title. */
const MAX_FILENAME_CHARS = 50;

const ONE_MINUTE_MS = 60 * 1000;
const MINUTES_PER_HOUR = 60;
const HOURS_PER_DAY = 24;
const DAYS_PER_WEEK = 7;
const ONE_HOUR_MS = MINUTES_PER_HOUR * ONE_MINUTE_MS;
const ONE_DAY_MS = HOURS_PER_DAY * ONE_HOUR_MS;

export interface DisplayItem {
  id: string;
  type: 'collection'|'tag'|'memory';
  title: string;
  snippet: string;
  collection: string;
  tags: string[];
  url: string;
  relativeTime?: string;
  entries: MemoryBankEntry[];
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
      attachedFilesMap: {type: Object},
      expandedIndex: {type: Number},
      toastMessage: {type: String},
      toastSubtitle: {type: String},
      toastDuration: {type: Number},
      toastIsError: {type: Boolean},
    };
  }

  accessor entries: MemoryBankEntry[] = [];
  accessor collections: string[] = [];
  accessor tags: string[] = [];
  accessor displayedItems: DisplayItem[] = [];
  accessor searchQuery: string = '';
  accessor filterType: 'all'|'collections'|'tags'|'memories' = 'all';
  accessor attachedFilesMap: Map<string, MemoryBankFileAttachment> = new Map();
  accessor expandedIndex: number = -1;
  accessor toastMessage: string = '';
  accessor toastSubtitle: string = '';
  accessor toastDuration: number = COPY_TOAST_DURATION_MS;
  accessor toastIsError: boolean = false;

  private isClipboardOperationPending: boolean = false;
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

  // TODO(crbug.com/574624499): Move #tag shorthand parsing into
  // parseSearchQuery() in memory_banks_search.ts so it is unified across all
  // search interfaces.
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
            id: `col-${col}`,
            type: 'collection',
            title: col,
            snippet: `${entries.length} ${
                entries.length === 1 ? 'memory' : 'memories'} in collection`,
            collection: col,
            tags: [],
            url: '',
            entries,
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
            id: `tag-${tag}`,
            type: 'tag',
            title: `#${tag}`,
            snippet: `${entries.length} ${
                entries.length === 1 ? 'memory' : 'memories'} tagged`,
            collection: '',
            tags: [tag],
            url: '',
            entries,
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
          id: `mem-${entry.id}`,
          type: 'memory',
          title: entry.tabTitle || 'Untitled Memory',
          snippet,
          collection: entry.collection || '',
          tags: entry.tags || [],
          url: entry.url || '',
          relativeTime: this.formatRelativeTime(entry.timestamp),
          entries: [entry],
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

  private getItemFromEvent(e: Event): DisplayItem|undefined {
    const indexStr = (e.currentTarget as HTMLElement).dataset['index'];
    return indexStr !== undefined ? this.displayedItems[Number(indexStr)] :
                                    undefined;
  }

  protected onCopyClick(e: Event) {
    const item = this.getItemFromEvent(e);
    if (item) {
      this.copyItem(item);
    }
  }

  protected onAttachClick(e: Event) {
    const item = this.getItemFromEvent(e);
    if (item) {
      this.attachItem(item);
    }
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

  protected isItemAttached(id: string): boolean {
    return this.attachedFilesMap.has(id);
  }

  protected getPasteShortcut(): string {
    return isMac ? 'Cmd+V' : 'Ctrl+V';
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

  private async showToastNotification(
      message: string, subtitle: string, durationMs: number,
      isError: boolean = false) {
    this.toastMessage = message;
    this.toastSubtitle = subtitle;
    this.toastDuration = durationMs;
    this.toastIsError = isError;
    await this.updateComplete;
    const toast = this.shadowRoot?.querySelector<CrToastElement>('#toast');
    toast?.show();
  }

  private async copyItem(item: DisplayItem) {
    if (this.isClipboardOperationPending) {
      return;
    }
    this.isClipboardOperationPending = true;

    try {
      const payload = formatEntriesAsMarkdown(item.entries);
      await navigator.clipboard.writeText(payload);
      this.attachedFilesMap = new Map();
      await this.showToastNotification(
          'Copied to clipboard.', '', COPY_TOAST_DURATION_MS);
    } catch (err) {
      console.error('Failed to copy to clipboard', err);
      await this.showToastNotification(
          'Failed to copy to clipboard.', '', COPY_TOAST_DURATION_MS,
          /*isError=*/ true);
    } finally {
      this.isClipboardOperationPending = false;
    }
  }

  private buildFileAttachment(item: DisplayItem): MemoryBankFileAttachment {
    // Keeps only letters, accents, numbers, underscores, hyphens, and spaces,
    // then replaces spaces with '_'. For example:
    // - chromium: docs! -> chromium_docs
    // - página! -> página
    const cleanedTitle = item.title.replace(/[^\p{L}\p{M}\p{N}_\-\s]/gu, '')
                             .trim()
                             .replace(/\s+/g, '_');
    const safeTitle = Array.from(cleanedTitle)
                          .slice(0, MAX_FILENAME_CHARS)
                          .join('')
                          .replace(/_+$/, '') ||
        'memory';

    const prefix = item.type === 'memory' ?
        '' :
        `${item.type === 'collection' ? 'Collection' : 'Tag'}_`;
    return {
      filename: `${prefix}${safeTitle}.md`,
      content: formatEntriesAsMarkdown(item.entries),
    };
  }

  private async attachItem(item: DisplayItem) {
    if (this.isClipboardOperationPending) {
      return;
    }

    const isRemoving = this.attachedFilesMap.has(item.id);
    if (!isRemoving && this.attachedFilesMap.size >= MAX_ATTACHED_FILES) {
      await this.showToastNotification(
          `Maximum of ${MAX_ATTACHED_FILES} files can be attached.`, '',
          ATTACH_TOAST_DURATION_MS, /*isError=*/ true);
      return;
    }

    this.isClipboardOperationPending = true;
    try {
      const nextFilesMap = new Map(this.attachedFilesMap);
      if (isRemoving) {
        nextFilesMap.delete(item.id);
      } else {
        nextFilesMap.set(item.id, this.buildFileAttachment(item));
      }

      if (nextFilesMap.size === 0) {
        await this.clearAllAttachments();
        return;
      }

      await this.syncAttachmentsToClipboard(nextFilesMap, isRemoving);
    } catch (err) {
      console.error('Failed to copy as file attachment', err);
      const errorMsg =
          isRemoving ? 'Failed to update clipboard.' : 'Failed to attach file.';
      await this.showToastNotification(
          errorMsg, '', ATTACH_TOAST_DURATION_MS, /*isError=*/ true);
    } finally {
      this.isClipboardOperationPending = false;
    }
  }

  private async clearAllAttachments() {
    this.attachedFilesMap = new Map();
    try {
      await navigator.clipboard.writeText('');
    } catch {
      // Ignore clipboard write failures on clearing.
    }
    await this.showToastNotification(
        'Removed from clipboard.', '', ATTACH_TOAST_DURATION_MS);
  }

  private async syncAttachmentsToClipboard(
      nextFilesMap: Map<string, MemoryBankFileAttachment>,
      isRemoving: boolean) {
    const files = Array.from(nextFilesMap.values());
    const {status} = await browserProxyFactory.getInstance()
                         .handler.copyAttachmentsToClipboard(files);
    if (status !== CopyAttachmentsToClipboardStatus.kSuccess) {
      const errorMsg =
          isRemoving ? 'Failed to update clipboard.' : 'Failed to attach file.';
      await this.showToastNotification(
          errorMsg, '', ATTACH_TOAST_DURATION_MS, /*isError=*/ true);
      return;
    }

    this.attachedFilesMap = nextFilesMap;
    await this.showAttachmentSuccessToast(files.length, isRemoving);
  }

  private async showAttachmentSuccessToast(count: number, isRemoving: boolean) {
    const shortcut = this.getPasteShortcut();
    if (isRemoving) {
      await this.showToastNotification(
          'Removed from clipboard.',
          `${count} ${count === 1 ? 'file' : 'files'} remaining - ${
              shortcut} to paste`,
          ATTACH_TOAST_DURATION_MS);
      return;
    }
    if (count > 1) {
      await this.showToastNotification(
          'Attached.', `${count} files on clipboard - ${shortcut} to paste`,
          ATTACH_TOAST_DURATION_MS);
      return;
    }
    await this.showToastNotification(
        'Attached as file.', `Press ${shortcut} in page to paste.`,
        ATTACH_TOAST_DURATION_MS);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'memory-banks-palette': MemoryBanksPaletteElement;
  }
}

customElements.define(MemoryBanksPaletteElement.is, MemoryBanksPaletteElement);
