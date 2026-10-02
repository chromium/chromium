// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_elements/cr_action_menu/cr_action_menu.js';
import '//resources/cr_elements/cr_button/cr_button.js';
import '//resources/cr_elements/cr_checkbox/cr_checkbox.js';
import '//resources/cr_elements/cr_icon/cr_icon.js';
import '//resources/cr_elements/cr_icon_button/cr_icon_button.js';
import '//resources/cr_elements/cr_search_field/cr_search_field.js';
import '//resources/cr_elements/icons.html.js';
import '../icons.html.js';
import './memory_banks_edit_dialog.js';

import type {CrActionMenuElement} from '//resources/cr_elements/cr_action_menu/cr_action_menu.js';
import type {CrSearchFieldElement} from '//resources/cr_elements/cr_search_field/cr_search_field.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';
import type {PropertyValues} from '//resources/lit/v3_0/lit.rollup.js';

import {browserProxyFactory, EntryType} from '../context_hub.mojom-webui.js';
import type {MemoryBankEntry, MemoryBankEntryAnnotations} from '../context_hub.mojom-webui.js';

import {getCss} from './memory_banks.css.js';
import {getHtml} from './memory_banks.html.js';
import {computeSuggestions, matchesMemoryBankEntry, parseSearchQuery} from './memory_banks_search.js';
import type {SearchSuggestion} from './memory_banks_search.js';

function downloadFile(
    filename: string, content: string,
    mimeType: string = 'text/markdown;charset=utf-8') {
  if (!content) {
    return;
  }
  const blob = new Blob([content], {type: mimeType});
  const url = URL.createObjectURL(blob);
  const a = document.createElement('a');
  a.href = url;
  a.download = filename;
  document.body.appendChild(a);
  a.click();
  document.body.removeChild(a);
  URL.revokeObjectURL(url);
}


export interface MemoryBanksElement {
  $: {
    actionMenu: CrActionMenuElement,
  };
}

export class MemoryBanksElement extends CrLitElement {
  static get is() {
    return 'memory-banks';
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
      selectedIds: {type: Object},
      searchQuery: {type: String},
      selectedCollections_: {type: Object, state: true},
      selectedTags_: {type: Object, state: true},
      activeFilterMenu_: {type: String, state: true},
      availableCollections_: {type: Array, state: true},
      availableTags_: {type: Array, state: true},
      filteredEntries_: {type: Array, state: true},
      viewMode_: {type: String, state: true},
      currentPage_: {type: Number, state: true},
      geminiResponse_: {type: String, state: true},
      isAskingGemini_: {type: Boolean, state: true},
      showGeminiPanel_: {type: Boolean, state: true},
      editingEntry_: {type: Object, state: true},
      searchSuggestions_: {type: Array, state: true},
      highlightedSuggestionIndex_: {type: Number, state: true},
    };
  }

  accessor entries: MemoryBankEntry[] = [];
  accessor selectedIds: Set<bigint> = new Set();
  accessor searchQuery: string = '';
  // The collections / tags checked in the chip filters; when non-empty, only
  // entries matching at least one selected value are shown. `''` is the
  // "No collection" / "No tags" bucket. Everything starts out unchecked
  // (no filter applied, all entries shown).
  protected accessor selectedCollections_: Set<string> = new Set();
  protected accessor selectedTags_: Set<string> = new Set();
  protected accessor activeFilterMenu_: 'collections'|'tags'|null = null;
  protected accessor availableCollections_: string[] = [];
  protected accessor availableTags_: string[] = [];
  protected accessor filteredEntries_: MemoryBankEntry[] = [];
  protected accessor viewMode_: 'card'|'table' = 'table';
  protected accessor currentPage_: number = 0;
  protected pageSize_: number = 7;
  protected accessor geminiResponse_: string = '';
  protected accessor isAskingGemini_: boolean = false;
  protected accessor showGeminiPanel_: boolean = false;
  protected accessor editingEntry_: MemoryBankEntry|null = null;
  protected accessor searchSuggestions_: SearchSuggestion[] = [];
  protected accessor highlightedSuggestionIndex_: number = -1;
  private activeMenuEntry_: MemoryBankEntry|null = null;
  private listenerIds_: number[] = [];

  override connectedCallback() {
    super.connectedCallback();
    const callbackRouter = browserProxyFactory.getInstance().callbackRouter;
    this.listenerIds_.push(
        callbackRouter.onMemoryBankEntryAdded.addListener(
            (entry: MemoryBankEntry) => {
              this.entries = [
                entry,
                ...this.entries.filter(existing => existing.id !== entry.id),
              ];
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
            }),
        callbackRouter.onMemoryBankEntriesDeleted.addListener(
            (ids: bigint[]) => {
              const deletedIds = new Set(ids);
              this.entries =
                  this.entries.filter(entry => !deletedIds.has(entry.id));
            }));
    this.fetchEntries();
    document.addEventListener('pointerdown', this.onDocumentPointerDown_);
  }

  override disconnectedCallback() {
    super.disconnectedCallback();
    document.removeEventListener('pointerdown', this.onDocumentPointerDown_);
    this.listenerIds_.forEach(
        id => browserProxyFactory.getInstance().callbackRouter.removeListener(
            id));
    this.listenerIds_ = [];
  }

  override willUpdate(changedProperties: PropertyValues<this>) {
    super.willUpdate(changedProperties);
    const changedPrivateProperties =
        changedProperties as Map<PropertyKey, unknown>;
    if (changedProperties.has('entries')) {
      this.availableCollections_ = this.computeAvailableCollections_();
      this.availableTags_ = this.computeAvailableTags_();
      this.selectedCollections_ = new Set(this.availableCollections_.filter(
          c => this.selectedCollections_.has(c)));
      this.selectedTags_ =
          new Set(this.availableTags_.filter(t => this.selectedTags_.has(t)));
    }

    if (changedProperties.has('entries') ||
        changedProperties.has('searchQuery') ||
        changedPrivateProperties.has('selectedCollections_') ||
        changedPrivateProperties.has('selectedTags_')) {
      this.filteredEntries_ = this.computeFilteredEntries_();
      // Drop selections that are no longer visible, so bulk actions (copy,
      // download, delete, Ask Gemini) never act on entries the user can't see.
      const visibleIds = new Set(this.filteredEntries_.map(entry => entry.id));
      const nextSelectedIds = new Set(
          Array.from(this.selectedIds).filter(id => visibleIds.has(id)));
      if (nextSelectedIds.size !== this.selectedIds.size) {
        this.selectedIds = nextSelectedIds;
      }
      // The filtered set can shrink out from under the table, e.g. when the
      // last entry on the final page is deleted.
      const lastPage = Math.max(
          0, Math.ceil(this.filteredEntries_.length / this.pageSize_) - 1);
      this.currentPage_ = Math.min(this.currentPage_, lastPage);
    }
  }

  private async fetchEntries() {
    const {entries} = await browserProxyFactory.getInstance()
                          .handler.getAllMemoryBankEntries();
    this.entries = entries;
  }

  private computeAvailableCollections_(): string[] {
    const set = new Set<string>();
    let hasUncollected = false;
    for (const entry of this.entries) {
      if (entry.collection) {
        set.add(entry.collection);
      } else {
        hasUncollected = true;
      }
    }
    const sorted = Array.from(set).sort((a, b) => a.localeCompare(b));
    if (hasUncollected) {
      sorted.unshift('');
    }
    return sorted;
  }

  private computeAvailableTags_(): string[] {
    const set = new Set<string>();
    let hasUntagged = false;
    for (const entry of this.entries) {
      if (!entry.tags || entry.tags.length === 0) {
        hasUntagged = true;
      } else {
        for (const tag of entry.tags) {
          if (tag) {
            set.add(tag);
          }
        }
      }
    }
    const sorted = Array.from(set).sort((a, b) => a.localeCompare(b));
    if (hasUntagged) {
      sorted.unshift('');
    }
    return sorted;
  }

  private computeFilteredEntries_(): MemoryBankEntry[] {
    const query = this.searchQuery.trim();
    if (query) {
      const parsed = parseSearchQuery(query);
      return this.entries.filter(
          entry => matchesMemoryBankEntry(entry, parsed));
    }

    let result = this.entries;

    // `''` represents the "No collection" / "No tags" bucket for entries
    // without those annotations.
    if (this.isCollectionFilterActive_()) {
      result = result.filter(
          entry => this.selectedCollections_.has(entry.collection || ''));
    }

    if (this.isTagFilterActive_()) {
      result = result.filter(
          entry => !entry.tags || entry.tags.length === 0 ?
              this.selectedTags_.has('') :
              entry.tags.some(tag => this.selectedTags_.has(tag)));
    }

    return result;
  }

  protected onFilterChipClick_(e: MouseEvent) {
    e.stopPropagation();
    const target = e.currentTarget as HTMLElement;
    const type = target.dataset['type'] as 'collections' | 'tags';
    this.activeFilterMenu_ = this.activeFilterMenu_ === type ? null : type;
  }

  protected getSelectedCollectionCount_(): number {
    return this.selectedCollections_.size;
  }

  protected isCollectionFilterActive_(): boolean {
    return this.selectedCollections_.size > 0;
  }

  protected isAllCollectionsSelected_(): boolean {
    return this.selectedCollections_.size === 0;
  }

  protected isCollectionSelected_(collection: string): boolean {
    return this.selectedCollections_.has(collection);
  }

  protected onToggleAllCollectionsChange_(e: Event) {
    const checkbox = e.currentTarget as HTMLElement & {checked: boolean};
    checkbox.checked = true;
    this.selectedCollections_ = new Set();
    this.currentPage_ = 0;
  }

  protected onCollectionCheckboxChange_(e: Event) {
    const checkbox = e.currentTarget as HTMLElement & {checked: boolean};
    const collection = checkbox.dataset['collection'] ?? '';
    const updated = new Set(this.selectedCollections_);
    if (checkbox.checked) {
      updated.add(collection);
    } else {
      updated.delete(collection);
    }
    this.selectedCollections_ = updated;
    this.currentPage_ = 0;
  }

  protected getSelectedTagCount_(): number {
    return this.selectedTags_.size;
  }

  protected isTagFilterActive_(): boolean {
    return this.selectedTags_.size > 0;
  }

  protected isAllTagsSelected_(): boolean {
    return this.selectedTags_.size === 0;
  }

  protected isTagSelected_(tag: string): boolean {
    return this.selectedTags_.has(tag);
  }

  protected onToggleAllTagsChange_(e: Event) {
    const checkbox = e.currentTarget as HTMLElement & {checked: boolean};
    checkbox.checked = true;
    this.selectedTags_ = new Set();
    this.currentPage_ = 0;
  }

  protected onTagCheckboxChange_(e: Event) {
    const checkbox = e.currentTarget as HTMLElement & {checked: boolean};
    const tag = checkbox.dataset['tag'] ?? '';
    const updated = new Set(this.selectedTags_);
    if (checkbox.checked) {
      updated.add(tag);
    } else {
      updated.delete(tag);
    }
    this.selectedTags_ = updated;
    this.currentPage_ = 0;
  }

  protected onFilterDropdownKeydown_(e: KeyboardEvent) {
    if (e.key === 'Escape' && this.activeFilterMenu_) {
      e.preventDefault();
      e.stopPropagation();
      this.activeFilterMenu_ = null;
    }
  }

  protected onViewModeChange_(mode: 'card'|'table') {
    this.viewMode_ = mode;
  }

  protected onTableViewClick_() {
    this.onViewModeChange_('table');
  }

  protected onCardViewClick_() {
    this.onViewModeChange_('card');
  }

  protected getPaginatedEntries_(): MemoryBankEntry[] {
    const start = this.currentPage_ * this.pageSize_;
    return this.filteredEntries_.slice(start, start + this.pageSize_);
  }

  protected getPaginationInfo_(): string {
    const total = this.filteredEntries_.length;
    if (total === 0) {
      return 'Showing 0 of 0 saved items';
    }
    const pageCount = this.getPaginatedEntries_().length;
    const start = this.currentPage_ * this.pageSize_;
    return `Showing ${start + 1}–${start + pageCount} of ${total} saved items`;
  }

  protected isLastPage_(): boolean {
    return (this.currentPage_ + 1) * this.pageSize_ >=
        this.filteredEntries_.length;
  }

  protected onPreviousPageClick_() {
    if (this.currentPage_ > 0) {
      this.currentPage_--;
    }
  }

  protected onNextPageClick_() {
    if (!this.isLastPage_()) {
      this.currentPage_++;
    }
  }

  protected formatDisplayUrl_(url: string): string {
    try {
      const parsed = new URL(url);
      const domainAndPath = parsed.host + parsed.pathname + parsed.search;
      return domainAndPath.replace(/\/$/, '');
    } catch {
      return url.replace(/^https?:\/\//, '').replace(/\/$/, '');
    }
  }

  protected formatDate_(mojoTime: {internalValue: bigint}): string {
    return this.convertMojoTimeToDate(mojoTime).toLocaleDateString(undefined, {
      month: 'short',
      day: 'numeric',
      year: 'numeric',
    });
  }

  private onDocumentPointerDown_ = (e: PointerEvent) => {
    if (!this.activeFilterMenu_) {
      return;
    }
    const isInsideChipWrapper = e.composedPath().some(
        el => el instanceof HTMLElement &&
            el.classList.contains('filter-chip-wrapper'));
    if (!isInsideChipWrapper) {
      this.activeFilterMenu_ = null;
    }
  };

  protected onMoreActionsClick_(e: MouseEvent) {
    e.preventDefault();
    e.stopPropagation();
    const target = e.currentTarget as HTMLElement;
    const index = Number(target.dataset['index']);
    const entryIndex = this.viewMode_ === 'table' ?
        this.currentPage_ * this.pageSize_ + index :
        index;
    this.activeMenuEntry_ = this.filteredEntries_[entryIndex] ?? null;
    this.$.actionMenu.showAt(target);
  }

  protected onMenuEditClick_() {
    this.$.actionMenu.close();
    this.editingEntry_ = this.activeMenuEntry_;
    this.activeMenuEntry_ = null;
  }

  protected async onMenuDeleteClick_() {
    this.$.actionMenu.close();
    if (this.activeMenuEntry_) {
      const id = this.activeMenuEntry_.id;
      this.activeMenuEntry_ = null;
      await this.deleteEntries_([id]);
    }
  }

  protected onEditDialogClose_() {
    this.editingEntry_ = null;
  }

  convertMojoTimeToDate(mojoTime: {internalValue: bigint}): Date {
    // Mojo Time represents microseconds since the Windows epoch (January 1,
    // 1601). JavaScript Date expects milliseconds since the Unix epoch (January
    // 1, 1970).

    // 11,644,473,600,000,000n is the Windows-to-Unix epoch delta in
    // microseconds.
    const unixEpochUs = mojoTime.internalValue - 11644473600000000n;
    return new Date(Number(unixEpochUs / 1000n));
  }

  isSelected(id: bigint): boolean {
    return this.selectedIds.has(id);
  }

  protected isAllSelected_(): boolean {
    const filtered = this.filteredEntries_;
    return filtered.length > 0 &&
        filtered.every(entry => this.selectedIds.has(entry.id));
  }

  protected isSomeSelected_(): boolean {
    if (this.selectedIds.size === 0) {
      return false;
    }
    const filtered = this.filteredEntries_;
    const filteredSelected =
        filtered.filter(entry => this.selectedIds.has(entry.id));
    return filteredSelected.length > 0 &&
        filteredSelected.length < filtered.length;
  }

  onCheckboxClick(e: Event) {
    e.stopPropagation();
  }

  onCheckboxChange(e: Event) {
    const checkbox = e.target as HTMLElement & {checked: boolean};
    const id = BigInt(checkbox.dataset['id']!);
    const updated = new Set(this.selectedIds);
    if (checkbox.checked) {
      updated.add(id);
    } else {
      updated.delete(id);
    }
    this.selectedIds = updated;
  }

  protected onSelectAllChange_(e: Event) {
    const checkbox = e.target as HTMLElement & {checked: boolean};
    if (checkbox.checked) {
      this.selectedIds = new Set(this.filteredEntries_.map(entry => entry.id));
    } else {
      this.selectedIds = new Set();
    }
  }

  private get searchField_(): CrSearchFieldElement|null {
    return this.shadowRoot?.querySelector<CrSearchFieldElement>(
               '#search-field') ??
        null;
  }

  protected updateSuggestions_(input: string = this.searchQuery) {
    this.searchSuggestions_ = computeSuggestions(
        input, this.availableTags_.filter(Boolean),
        this.availableCollections_.filter(Boolean));
    this.highlightedSuggestionIndex_ = -1;
  }

  private closeSuggestions_() {
    this.searchSuggestions_ = [];
    this.highlightedSuggestionIndex_ = -1;
  }

  protected onSearchFocusin_() {
    this.updateSuggestions_();
  }

  protected onSearchFocusout_(e: FocusEvent) {
    const relatedTarget = e.relatedTarget as Node | null;
    if (relatedTarget && this.shadowRoot?.contains(relatedTarget)) {
      return;
    }
    this.closeSuggestions_();
  }

  protected onSearchChanged_(e: CustomEvent<string>) {
    this.searchQuery = e.detail;
    this.selectedIds = new Set();
    this.currentPage_ = 0;
    this.updateSuggestions_(this.searchQuery);
  }

  protected onSearchKeydown_(e: KeyboardEvent) {
    if (this.searchSuggestions_.length === 0) {
      if (e.key === 'ArrowDown' || e.key === 'ArrowUp') {
        this.updateSuggestions_();
      }
      return;
    }

    if (e.key === 'ArrowDown') {
      e.preventDefault();
      // Highlight the next suggestion. If unselected (-1), highlights the first
      // item (0). If at the bottom, wraps back to the top.
      this.highlightedSuggestionIndex_ =
          (this.highlightedSuggestionIndex_ + 1) %
          this.searchSuggestions_.length;
    } else if (e.key === 'ArrowUp') {
      e.preventDefault();
      // Highlight the previous suggestion. If unselected (-1) or already at
      // the top (0), wraps around to the last item.
      this.highlightedSuggestionIndex_ = this.highlightedSuggestionIndex_ <= 0 ?
          this.searchSuggestions_.length - 1 :
          this.highlightedSuggestionIndex_ - 1;
    } else if (e.key === 'Enter' || e.key === 'Tab') {
      const selected =
          this.searchSuggestions_[this.highlightedSuggestionIndex_];
      if (selected) {
        e.preventDefault();
        this.applySuggestion_(selected);
      } else if (e.key === 'Enter') {
        this.closeSuggestions_();
      }
    } else if (e.key === 'Escape') {
      e.preventDefault();
      this.closeSuggestions_();
    }
  }

  protected async applySuggestion_(suggestion: SearchSuggestion) {
    this.setSearchQuery_(suggestion.query);
    if (suggestion.query.trim().endsWith(':')) {
      this.updateSuggestions_(suggestion.query);
    } else {
      this.closeSuggestions_();
    }

    await this.updateComplete;
    this.searchField_?.getSearchInput().focus();
  }

  protected onSuggestionMousedown_(e: MouseEvent) {
    e.preventDefault();
    const target = e.currentTarget as HTMLElement;
    const index = Number(target.dataset['index']);
    const suggestion = this.searchSuggestions_[index];
    if (suggestion) {
      this.applySuggestion_(suggestion);
    }
  }

  private setSearchQuery_(newQuery: string) {
    this.searchQuery = newQuery;
    this.searchField_?.setValue(newQuery, /*noEvent=*/ true);
    this.selectedIds = new Set();
    this.currentPage_ = 0;
  }

  protected async onCopyClick_() {
    const textToCopy = this.getSelectedEntriesAsMarkdown_();
    try {
      await navigator.clipboard.writeText(textToCopy);
    } catch (err) {
      console.error('Failed to copy: ', err);
    }
  }

  protected onDownloadSelectedEntriesClick_() {
    downloadFile(
        'memory_banks_entries.md', this.getSelectedEntriesAsMarkdown_());
  }

  protected onDownloadGeminiResponseClick_() {
    downloadFile('gemini_response.md', this.geminiResponse_);
  }

  protected async onDeleteClick_() {
    await this.deleteEntries_(Array.from(this.selectedIds));
  }

  private async deleteEntries_(ids: bigint[]) {
    await browserProxyFactory.getInstance().handler.deleteMemoryBankEntries(
        ids);
    const updated = new Set(this.selectedIds);
    for (const id of ids) {
      updated.delete(id);
    }
    this.selectedIds = updated;
  }

  protected onAskGeminiClick_() {
    this.showGeminiPanel_ = !this.showGeminiPanel_;
  }

  protected onClosePanelClick_() {
    this.showGeminiPanel_ = false;
    this.geminiResponse_ = '';
  }

  protected onCloseResponseClick_() {
    this.geminiResponse_ = '';
  }

  protected async onQuickOptionClick_(e: Event) {
    const target = e.currentTarget as HTMLElement;
    const action = target.dataset['option'];
    if (!action || this.selectedIds.size === 0 || this.isAskingGemini_) {
      return;
    }
    this.isAskingGemini_ = true;
    this.geminiResponse_ = '';
    const memoryBankEntryIds = Array.from(this.selectedIds);
    try {
      const {response} =
          await browserProxyFactory.getInstance().handler.askGeminiWithContext(
              action, memoryBankEntryIds, /*save_to_history=*/ false);
      this.geminiResponse_ = response ? response.content : '';
    } catch (err) {
      console.error('Failed to ask Gemini:', err);
      this.geminiResponse_ = 'Error generating response from Gemini.';
    } finally {
      this.isAskingGemini_ = false;
    }
  }

  private getSelectedEntriesAsMarkdown_(): string {
    return this.entries.filter(entry => this.selectedIds.has(entry.id))
        .map(entry => {
          const dateStr =
              this.convertMojoTimeToDate(entry.timestamp).toLocaleString();
          const typeStr = entry.type === EntryType.kTextSelection ?
              'Saved Text Selection' :
              'Saved Tab';
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
          lines.push(`- **Saved Date:** ${dateStr}`);
          if (entry.note) {
            const formattedNote = entry.note.replace(/\r?\n/g, '\n  > ');
            lines.push(`- **Note:**\n  > ${formattedNote}`);
          }
          if (entry.selectedText) {
            const formattedContent =
                entry.selectedText.replace(/\r?\n/g, '\n  > ');
            lines.push(`- **Content:**\n  > ${formattedContent}`);
          }
          return lines.join('\n');
        })
        .join('\n\n---\n\n');
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'memory-banks': MemoryBanksElement;
  }
}

customElements.define(MemoryBanksElement.is, MemoryBanksElement);
