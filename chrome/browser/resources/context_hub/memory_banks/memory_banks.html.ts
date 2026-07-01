// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {MemoryBanksElement} from './memory_banks.js';
import {getHtml as getMemoryBankEntryHtml} from './memory_banks_entry.html.js';

export function getHtml(this: MemoryBanksElement) {
  return html`
    <main id="memory-banks-view">
        <section>
            <div class="header-container">
              <h1>Memory banks</h1>
              ${this.entries.length === 0 ? '' : html`
                <cr-search-field
                    id="search-field"
                    label="Search memory"
                    @search-changed="${this.onSearchChanged_}">
                </cr-search-field>
              `}
            </div>

            ${
      this.entries.length === 0 ?
          html`
              <p>No saved memories yet.</p>
            ` :
          html`
              <div class="action-bar">
                <cr-checkbox
                    ?checked="${this.isAllSelected_()}"
                    ?indeterminate="${this.isSomeSelected_()}"
                    @change="${this.onSelectAllChange_}">
                  Select all
                </cr-checkbox>
                <div class="action-buttons">
                  <cr-button ?disabled="${this.selectedIds.size === 0}"
                      @click="${this.onCopyClick_}">
                    Copy selected
                  </cr-button>
                  <cr-button ?disabled="${this.selectedIds.size === 0}"
                      @click="${this.onDownloadClick_}">
                    Download selected
                  </cr-button>
                  <cr-button ?disabled="${this.selectedIds.size === 0}"
                      @click="${this.onDeleteClick_}">
                    Delete selected
                  </cr-button>
                </div>
              </div>

              ${
              this.searchQuery ?
                  html`
                <h2>Search results</h2>
                ${
                      this.getFilteredEntries_().length === 0 ?
                          html`
                  <p>No results found.</p>
                ` :
                          html`
                  <div class="grid">
                    ${
                              this.getFilteredEntries_().map(
                                  entry =>
                                      getMemoryBankEntryHtml.call(this, entry))}
                  </div>
                `}
              ` :
                  html`
                <h2>Recently saved</h2>
                <div class="grid">
                  ${
                      this.recentlySaved_.map(
                          entry => getMemoryBankEntryHtml.call(this, entry))}
                </div>

                <h2>All saved</h2>
                <div class="grid">
                  ${
                      this.entries.map(
                          entry => getMemoryBankEntryHtml.call(this, entry))}
                </div>
              `}
            `}
        </section>
    </main>
  `;
}
