// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_elements/cr_action_menu/cr_action_menu.js';
import '//resources/cr_elements/cr_button/cr_button.js';
import '//resources/cr_elements/cr_checkbox/cr_checkbox.js';
import '//resources/cr_elements/cr_icon/cr_icon.js';
import '//resources/cr_elements/cr_icon_button/cr_icon_button.js';
import '//resources/cr_elements/cr_search_field/cr_search_field.js';
import '//resources/cr_elements/cr_tabs/cr_tabs.js';
import '//resources/cr_elements/icons.html.js';
import './memory_banks_edit_dialog.js';

import type {CrActionMenuElement} from '//resources/cr_elements/cr_action_menu/cr_action_menu.js';
import type {CrSearchFieldElement} from '//resources/cr_elements/cr_search_field/cr_search_field.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';
import type {PropertyValues} from '//resources/lit/v3_0/lit.rollup.js';

import {browserProxyFactory, EntryType} from '../context_hub.mojom-webui.js';
import type {MemoryBankEntry} from '../context_hub.mojom-webui.js';

import {getCss} from './memory_banks.css.js';
import {getHtml} from './memory_banks.html.js';
import type {EntryAnnotationsUpdatedDetail} from './memory_banks_edit_dialog.js';
import {computeSuggestions, matchesMemoryBankEntry, parseSearchQuery} from './memory_banks_search.js';
import type {SearchSuggestion} from './memory_banks_search.js';

function downloadFile(filename: string, content: string) {
  if (!content) {
    return;
  }
  const blob = new Blob([content], {type: 'text/plain;charset=utf-8'});
  const url = URL.createObjectURL(blob);
  const a = document.createElement('a');
  a.href = url;
  a.download = filename;
  document.body.appendChild(a);
  a.click();
  document.body.removeChild(a);
  URL.revokeObjectURL(url);
}

const FOLDER_TAB_ICON: string =
    'chrome://resources/images/icon_folder_open.svg';

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
      selectedCollection: {type: String},
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
  accessor selectedCollection: string = '';
  protected accessor geminiResponse_: string = '';
  protected accessor isAskingGemini_: boolean = false;
  protected accessor showGeminiPanel_: boolean = false;
  protected accessor editingEntry_: MemoryBankEntry|null = null;
  protected accessor searchSuggestions_: SearchSuggestion[] = [];
  protected accessor highlightedSuggestionIndex_: number = -1;
  private activeMenuEntry_: MemoryBankEntry|null = null;
  private availableCollections_: string[] = [];
  private availableTags_: string[] = [];

  override connectedCallback() {
    super.connectedCallback();
    this.fetchEntries();
  }

  override willUpdate(changedProperties: PropertyValues<this>) {
    super.willUpdate(changedProperties);
    if (changedProperties.has('entries')) {
      this.availableCollections_ = this.computeAvailableCollections_();
      this.availableTags_ = this.computeAvailableTags_();
    }
  }

  private async fetchEntries() {
    const {entries} = await browserProxyFactory.getInstance()
                          .handler.getAllMemoryBankEntries();
    this.entries = entries;
  }

  protected getAvailableCollections_(): string[] {
    return this.availableCollections_;
  }

  protected getAvailableTags_(): string[] {
    return this.availableTags_;
  }

  private computeAvailableCollections_(): string[] {
    const set = new Set<string>();
    for (const entry of this.entries) {
      if (entry.collection) {
        set.add(entry.collection);
      }
    }
    return Array.from(set).sort();
  }

  private computeAvailableTags_(): string[] {
    const set = new Set<string>();
    for (const entry of this.entries) {
      if (entry.tags) {
        for (const tag of entry.tags) {
          if (tag) {
            set.add(tag);
          }
        }
      }
    }
    return Array.from(set).sort((a, b) => a.localeCompare(b));
  }

  protected getRecentlySaved_(): MemoryBankEntry[] {
    return this.entries.slice(0, 3);
  }

  protected getFilteredEntries_(): MemoryBankEntry[] {
    const query = this.searchQuery.trim();
    if (query) {
      const parsed = parseSearchQuery(query);
      return this.entries.filter(
          entry => matchesMemoryBankEntry(entry, parsed));
    }

    if (this.selectedCollection) {
      return this.entries.filter(e => e.collection === this.selectedCollection);
    }

    return this.entries;
  }

  protected getTabNames_(): string[] {
    return ['All', ...this.getAvailableCollections_()];
  }

  protected getTabIcons_(): string[] {
    return this.getTabNames_().map(() => FOLDER_TAB_ICON);
  }

  protected getSelectedTabIndex_(): number {
    if (!this.selectedCollection) {
      return 0;
    }
    const index =
        this.getAvailableCollections_().indexOf(this.selectedCollection);
    return index === -1 ? 0 : index + 1;
  }

  protected onTabsSelectedChanged_(e: CustomEvent<{value: number}>) {
    const index = e.detail.value;
    if (index === 0) {
      this.selectedCollection = '';
    } else {
      this.selectedCollection =
          this.getAvailableCollections_()[index - 1] || '';
    }
    this.selectedIds = new Set();
  }

  protected onMoreActionsClick_(entry: MemoryBankEntry, e: MouseEvent) {
    e.preventDefault();
    e.stopPropagation();
    this.activeMenuEntry_ = entry;
    const target = e.currentTarget as HTMLElement;
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

  protected onEntryAnnotationsUpdated_(
      e: CustomEvent<EntryAnnotationsUpdatedDetail>) {
    const {id, collection, note, tags} = e.detail;
    this.entries = this.entries.map(
        entry => entry.id === id ? {...entry, collection, note, tags} : entry);
    if (this.selectedCollection &&
        !this.getAvailableCollections_().includes(this.selectedCollection)) {
      this.selectedCollection = '';
    }
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
    const filtered = this.getFilteredEntries_();
    return filtered.length > 0 &&
        filtered.every(entry => this.selectedIds.has(entry.id));
  }

  protected isSomeSelected_(): boolean {
    if (this.selectedIds.size === 0) {
      return false;
    }
    const filtered = this.getFilteredEntries_();
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
    if (checkbox.checked) {
      this.selectedIds.add(id);
    } else {
      this.selectedIds.delete(id);
    }
    this.selectedIds = new Set(this.selectedIds);
  }

  protected onSelectAllChange_(e: Event) {
    const checkbox = e.target as HTMLElement & {checked: boolean};
    if (checkbox.checked) {
      this.selectedIds =
          new Set(this.getFilteredEntries_().map(entry => entry.id));
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
        input, this.getAvailableTags_(), this.getAvailableCollections_());
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
  }

  protected async onCopyClick_() {
    const textToCopy = this.getSelectedEntriesAsText_();
    try {
      await navigator.clipboard.writeText(textToCopy);
    } catch (err) {
      console.error('Failed to copy: ', err);
    }
  }

  protected onDownloadSelectedEntriesClick_() {
    downloadFile('memory_banks_entries.txt', this.getSelectedEntriesAsText_());
  }

  protected onDownloadGeminiResponseClick_() {
    downloadFile('gemini_response.txt', this.geminiResponse_);
  }

  protected async onDeleteClick_() {
    await this.deleteEntries_(Array.from(this.selectedIds));
  }

  private async deleteEntries_(ids: bigint[]) {
    await browserProxyFactory.getInstance().handler.deleteMemoryBankEntries(
        ids);
    for (const id of ids) {
      this.selectedIds.delete(id);
    }
    this.selectedIds = new Set(this.selectedIds);
    await this.fetchEntries();
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
              action, memoryBankEntryIds);
      this.geminiResponse_ = response ? response.content : '';
    } catch (err) {
      console.error('Failed to ask Gemini:', err);
      this.geminiResponse_ = 'Error generating response from Gemini.';
    } finally {
      this.isAskingGemini_ = false;
    }
  }

  private getSelectedEntriesAsText_(): string {
    return this.entries.filter(entry => this.selectedIds.has(entry.id))
        .map(entry => {
          const dateStr =
              this.convertMojoTimeToDate(entry.timestamp).toLocaleString();
          const typeStr = entry.type === EntryType.kTextSelection ?
              'Saved Text' :
              'Saved Tab';
          const lines = [
            `[${typeStr}]`,
            `Title: ${entry.tabTitle}`,
            `URL: ${entry.url}`,
          ];
          if (entry.collection) {
            lines.push(`Collection: ${entry.collection}`);
          }
          if (entry.tags && entry.tags.length > 0) {
            lines.push(`Tags: ${entry.tags.join(', ')}`);
          }
          if (entry.note) {
            lines.push(`Note: "${entry.note}"`);
          }
          if (entry.selectedText) {
            lines.push(`Content: "${entry.selectedText}"`);
          }
          lines.push(`Saved Date: ${dateStr}`);
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
