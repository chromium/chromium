// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {MemoryBanksElement} from './memory_banks.js';
import {getHtml as getMemoryBankEntryHtml} from './memory_banks_entry.html.js';
import {getHtml as getMemoryBanksTableHtml} from './memory_banks_table.html.js';

export function getHtml(this: MemoryBanksElement) {
  return html`
    <main id="memory-banks-view">
        <section>
            <div class="header-container">
              <h1>Memory banks</h1>
              ${
      this.entries.length > 0 ?
          html`
                <div class="search-container"
                    @focusin="${this.onSearchFocusin_}"
                    @focusout="${this.onSearchFocusout_}"
                    @keydown="${this.onSearchKeydown_}">
                  <cr-search-field
                      id="search-field"
                      label="Search memory (e.g. tag:recipes, collection:Work)"
                      @search-changed="${this.onSearchChanged_}">
                  </cr-search-field>
                  ${
              this.searchSuggestions_.length > 0 ?
                  html`
                    <div class="search-suggestions" role="listbox">
                      ${
                      this.searchSuggestions_.map(
                          (suggestion, index) => html`
                        <div class="suggestion-item ${
                              index === this.highlightedSuggestionIndex_ ?
                                  'highlighted' :
                                  ''}"
                            role="option"
                            data-index="${index}"
                            aria-selected="${
                              index === this.highlightedSuggestionIndex_}"
                            @mousedown="${this.onSuggestionMousedown_}">
                          <div class="suggestion-content">
                            <span class="suggestion-label">${
                              suggestion.label}</span>
                            ${
                              suggestion.description ?
                                  html`
                              <span class="suggestion-desc">${
                                      suggestion.description}</span>
                            ` :
                                  ''}
                          </div>
                        </div>
                      `)}
                    </div>
                  ` :
                  ''}
                </div>
              ` :
          ''}
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
                      @click="${this.onDownloadSelectedEntriesClick_}">
                    Download selected
                  </cr-button>
                  <cr-button ?disabled="${this.selectedIds.size === 0}"
                      @click="${this.onDeleteClick_}">
                    Delete selected
                  </cr-button>
                  <cr-button ?disabled="${this.selectedIds.size === 0}"
                      @click="${this.onAskGeminiClick_}">
                    Ask Gemini with selected context
                  </cr-button>
                </div>
              </div>

              ${
              this.showGeminiPanel_ ? html`
                <div class="gemini-panel">
                  <div class="gemini-panel-header">
                    <h3>Select an action for Gemini</h3>
                    <cr-icon-button
                        iron-icon="cr:close"
                        title="Close"
                        @click="${this.onClosePanelClick_}">
                    </cr-icon-button>
                  </div>
                  <div class="quick-options">
                    <cr-button class="chip"
                        data-option="Summarize selected memories"
                        ?disabled="${this.isAskingGemini_}"
                        @click="${this.onQuickOptionClick_}">
                      Summarize
                    </cr-button>
                    <cr-button class="chip"
                        data-option="Compare selected memories"
                        ?disabled="${this.isAskingGemini_}"
                        @click="${this.onQuickOptionClick_}">
                      Compare
                    </cr-button>
                  </div>
                  ${
                                          this.geminiResponse_ ? html`
                    <div class="gemini-response">
                      <div class="gemini-response-header">
                        <strong>Gemini:</strong>
                        <div class="gemini-response-actions">
                          <cr-icon-button
                              id="download-gemini-response"
                              iron-icon="cr:download"
                              title="Download response"
                              @click="${this.onDownloadGeminiResponseClick_}">
                          </cr-icon-button>
                          <cr-icon-button
                              iron-icon="cr:close"
                              title="Close"
                              @click="${this.onCloseResponseClick_}">
                          </cr-icon-button>
                        </div>
                      </div>
                      <p>${this.geminiResponse_}</p>
                    </div>
                  ` :
                                                                 ''}
                </div>
              ` :
                                      ''}

              ${
              this.searchQuery ?
                  html`
                <h2>Search results (${this.filteredEntries_.length})</h2>
              ` :
                  html`
                <div class="controls-container">
                  <div class="filter-chips">
                    <div class="filter-chip-wrapper"
                        @keydown="${this.onFilterDropdownKeydown_}">
                      <button
                          class="filter-chip"
                          data-type="collections"
                          aria-haspopup="dialog"
                          aria-expanded="${
                      this.activeFilterMenu_ === 'collections'}"
                          @click="${this.onFilterChipClick_}">
                        <cr-icon icon="context-hub:folder"></cr-icon>
                        <span>Collections</span>
                        ${
                      this.isCollectionFilterActive_() ? html`
                          <span class="filter-chip-badge">
                            ${this.getSelectedCollectionCount_()}
                          </span>
                        ` :
                                                         ''}
                        <cr-icon icon="cr:arrow-drop-down"></cr-icon>
                      </button>
                      ${
                      this.activeFilterMenu_ === 'collections' ?
                          html`
                        <div class="filter-dropdown" role="dialog"
                            aria-label="Filter collections">
                          <span class="filter-dropdown-title">
                            Filter Collections
                          </span>
                          <div class="filter-dropdown-list">
                            ${
                              this.availableCollections_.length > 0 ?
                                  html`
                              <cr-checkbox
                                  ?checked="${this.isAllCollectionsSelected_()}"
                                  @change="${
                                      this.onToggleAllCollectionsChange_}">
                                All Collections
                              </cr-checkbox>
                              ${this.availableCollections_.map(c => html`
                                <cr-checkbox
                                    data-collection="${c}"
                                    ?checked="${this.isCollectionSelected_(c)}"
                                    @change="${
                                        this.onCollectionCheckboxChange_}">
                                  ${c || 'No collection'}
                                </cr-checkbox>
                              `)}
                            ` :
                                  html`
                              <span class="filter-dropdown-empty">
                                No collections
                              </span>
                            `}
                          </div>
                        </div>
                      ` :
                          ''}
                    </div>

                    <div class="filter-chip-wrapper"
                        @keydown="${this.onFilterDropdownKeydown_}">
                      <button
                          class="filter-chip"
                          data-type="tags"
                          aria-haspopup="dialog"
                          aria-expanded="${this.activeFilterMenu_ === 'tags'}"
                          @click="${this.onFilterChipClick_}">
                        <cr-icon icon="context-hub:tag"></cr-icon>
                        <span>Tags</span>
                        ${
                      this.isTagFilterActive_() ? html`
                          <span class="filter-chip-badge">
                            ${this.getSelectedTagCount_()}
                          </span>
                        ` :
                                                  ''}
                        <cr-icon icon="cr:arrow-drop-down"></cr-icon>
                      </button>
                      ${
                      this.activeFilterMenu_ === 'tags' ?
                          html`
                        <div class="filter-dropdown" role="dialog"
                            aria-label="Filter tags">
                          <span class="filter-dropdown-title">Filter Tags</span>
                          <div class="filter-dropdown-list">
                            ${
                              this.availableTags_.length > 0 ? html`
                              <cr-checkbox
                                  ?checked="${this.isAllTagsSelected_()}"
                                  @change="${this.onToggleAllTagsChange_}">
                                All Tags
                              </cr-checkbox>
                              ${this.availableTags_.map(tag => html`
                                <cr-checkbox
                                    data-tag="${tag}"
                                    ?checked="${this.isTagSelected_(tag)}"
                                    @change="${this.onTagCheckboxChange_}">
                                  ${tag || 'No tags'}
                                </cr-checkbox>
                              `)}
                            ` :
                                                               html`
                              <span class="filter-dropdown-empty">No tags</span>
                            `}
                          </div>
                        </div>
                      ` :
                          ''}
                    </div>
                  </div>

                  <div class="view-toggle-container" role="radiogroup"
                      aria-label="View mode">
                    <button class="view-toggle-btn ${
                      this.viewMode_ === 'table' ? 'active' : ''}"
                        role="radio"
                        aria-checked="${this.viewMode_ === 'table'}"
                        @click="${this.onTableViewClick_}">
                      <cr-icon icon="context-hub:table-view"></cr-icon>
                      <span>Table View</span>
                    </button>
                    <button class="view-toggle-btn ${
                      this.viewMode_ === 'card' ? 'active' : ''}"
                        role="radio"
                        aria-checked="${this.viewMode_ === 'card'}"
                        @click="${this.onCardViewClick_}">
                      <cr-icon icon="context-hub:grid-view"></cr-icon>
                      <span>Card View</span>
                    </button>
                  </div>
                </div>
              `}

              ${
              this.filteredEntries_.length === 0 ?
                  html`
                <p>${
                      this.searchQuery ? 'No results found.' :
                                         'No memories in this view.'}</p>
              ` :
                  html`
                ${
                      this.viewMode_ === 'table' ?
                          html`${getMemoryBanksTableHtml.call(this)}` :
                          html`
                  <div class="grid">
                    ${
                              this.filteredEntries_.map(
                                  (entry, index) => getMemoryBankEntryHtml.call(
                                      this, entry, index))}
                  </div>
                `}
              `}
            `}
        </section>
    </main>

    ${
      this.editingEntry_ ? html`
      <memory-banks-edit-dialog
          .entry="${this.editingEntry_}"
          .availableCollections="${this.availableCollections_.filter(Boolean)}"
          .availableTags="${this.availableTags_.filter(Boolean)}"
          @close="${this.onEditDialogClose_}"
          @entry-annotations-updated="${this.onEntryAnnotationsUpdated_}">
      </memory-banks-edit-dialog>
    ` :
                           ''}

    <cr-action-menu id="actionMenu">
      <button class="dropdown-item" @click="${this.onMenuEditClick_}">
        Edit
      </button>
      <button class="dropdown-item" @click="${this.onMenuDeleteClick_}">
        Delete
      </button>
    </cr-action-menu>
  `;
}
