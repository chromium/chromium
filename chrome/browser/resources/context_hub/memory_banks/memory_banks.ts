// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_elements/cr_icon/cr_icon.js';
import '//resources/cr_elements/cr_button/cr_button.js';
import '//resources/cr_elements/cr_checkbox/cr_checkbox.js';
import '//resources/cr_elements/cr_search_field/cr_search_field.js';

import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';

import {BrowserProxyImpl} from '../browser_proxy.js';
import {EntryType} from '../context_hub.mojom-webui.js';
import type {MemoryBankEntry} from '../context_hub.mojom-webui.js';

import {getCss} from './memory_banks.css.js';
import {getHtml} from './memory_banks.html.js';

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
    };
  }

  accessor entries: MemoryBankEntry[] = [];
  accessor selectedIds: Set<bigint> = new Set();
  accessor searchQuery: string = '';

  override connectedCallback() {
    super.connectedCallback();
    this.fetchEntries();
  }

  private async fetchEntries() {
    const {entries} =
        await BrowserProxyImpl.getInstance().handler.getAllMemoryBankEntries();
    this.entries = entries;
  }

  protected get recentlySaved_(): MemoryBankEntry[] {
    return this.entries.slice(0, 3);
  }

  protected getFilteredEntries_(): MemoryBankEntry[] {
    if (!this.searchQuery) {
      return this.entries;
    }
    const query = this.searchQuery.toLowerCase();
    return this.entries.filter(entry => {
      return entry.tabTitle.toLowerCase().includes(query) ||
          entry.url.toLowerCase().includes(query) ||
          (entry.selectedText &&
           entry.selectedText.toLowerCase().includes(query));
    });
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

  protected onSearchChanged_(e: CustomEvent<string>) {
    this.searchQuery = e.detail;
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

  protected onDownloadClick_() {
    const textToDownload = this.getSelectedEntriesAsText_();
    const blob = new Blob([textToDownload], {type: 'text/plain;charset=utf-8'});
    const url = URL.createObjectURL(blob);
    const a = document.createElement('a');
    a.href = url;
    a.download = 'memory_banks_entries.txt';
    document.body.appendChild(a);
    a.click();
    document.body.removeChild(a);
    URL.revokeObjectURL(url);
  }

  protected async onDeleteClick_() {
    const ids = Array.from(this.selectedIds);
    await BrowserProxyImpl.getInstance().handler.deleteMemoryBankEntries(ids);
    this.selectedIds = new Set();
    await this.fetchEntries();
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
