// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {SmartSearchElement} from './smart_search.js';

export function getHtml(this: SmartSearchElement) {
  return html`
    <main id="smart-search-view">
      <section>
        <h1>JumpStart</h1>

        <!-- Search Bar -->
        <div class="search-container">
          <div class="search-bar-wrapper">
            <cr-icon icon="cr:search"></cr-icon>
            <input
                class="search-input"
                type="text"
                placeholder="Describe what you're looking for"
                .value="${this.searchQuery_}"
                ?disabled="${!this.smartSearchEnabled_ || this.isSearching_}"
                @input="${this.onSearchInput_}"
                @keydown="${this.onSearchInputKeydown_}" />
            <cr-button class="action-button search-button"
                ?disabled="${
  !this.smartSearchEnabled_ || this.isSearching_ || !this.searchQuery_.trim()}"
                @click="${this.onSearchClick_}">
              ${
      this.isSearching_ ?
      'Searching...' :
      'Search'}
            </cr-button>
          </div>
        </div>

        ${
      this.hasSearched_ ?
      html`
          <!-- Multi-select Action Bar -->
          ${
          this.selectedIds_.size > 0 ?
          html`
            <div class="action-bar">
              <span class="selection-count">
                ${this.selectedIds_.size} items selected
              </span>
              <div class="action-buttons">
                <cr-button class="action-button"
                    ?disabled="${
  !this.smartSearchEnabled_ || this.selectedIds_.size === 0}"
                    @click="${this.onOpenSelectedClick_}">
                  Open All
                </cr-button>
              </div>
            </div>
          `: ''}

          <!-- Results List -->
          ${
      this.isSearching_ ?
      html`
            <div class="loading-container">
              <div class="spinner"></div>
              <span>Searching...</span>
            </div>
          ` :
      html`
            ${
          this.getFilteredResults_()
              .length ===
          0 ? html`
              <div class="zero-state">
                <cr-icon class="zero-state-icon" icon="cr:search"></cr-icon>
                <h3>No results found</h3>
                <p>Try searching for a different keyword.</p>
              </div>
            ` :
              html`
              <div class="results-grid">
                ${
                  this.getFilteredResults_()
                      .map(item => html`
                  <smart-search-card
                      .result="${item}"
                      ?selected="${this.selectedIds_.has(item.id)}"
                      @selection-change="${this.onCardSelectionChange_}">
                  </smart-search-card>
                `)}
              </div>
            `}
          `}
        `: html`
          <!-- Initial Zero State -->
          <div class="zero-state">
            <cr-icon class="zero-state-icon" icon="cr:search"></cr-icon>
            <h3>Search across your work context</h3>
            <p>
              Enter a search query to find relevant files and context.
            </p>
          </div>
        `}
      </section>
    </main>
  `;
}
