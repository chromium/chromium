// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {getFaviconForPageURL} from '//resources/js/icon.js';
import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {
  DisplayItem,
  MemoryBanksPaletteElement,
} from './memory_banks_palette.js';

export function getHtml(this: MemoryBanksPaletteElement) {
  return html`
    <div class="palette-container">
      <div class="header">
        <div class="header-title">
          <cr-icon icon="context-hub:auto-awesome" class="header-icon">
          </cr-icon>
          <span>Memory Banks</span>
        </div>
        <div class="header-actions">
          <cr-icon-button
              class="settings-btn"
              iron-icon="cr:settings-filled"
              aria-label="Settings"
              title="Open Context Hub (chrome://context-hub)"
              @click="${this.onSettingsClick}">
          </cr-icon-button>
          <cr-icon-button
              iron-icon="cr:close"
              aria-label="Close"
              @click="${this.onCloseClick}">
          </cr-icon-button>
        </div>
      </div>

      <div class="search-container">
        <cr-search-field
            class="search-input"
            label="Search your memories, #tags, or collections..."
            clear-label="Clear search"
            @search-changed="${this.onSearchChanged}"
            autofocus>
        </cr-search-field>
      </div>

      <div class="filter-bar">
        <div
            class="filter-chip ${this.filterType === 'all' ? 'active' : ''}"
            @click="${this.onFilterAllClick}">
          All
        </div>
        <div
            class="filter-chip ${
                this.filterType === 'collections' ? 'active' : ''}"
            @click="${this.onFilterCollectionsClick}">
          Collections (${this.collections.length})
        </div>
        <div
            class="filter-chip ${this.filterType === 'tags' ? 'active' : ''}"
            @click="${this.onFilterTagsClick}">
          Tags (${this.tags.length})
        </div>
        <div
            class="filter-chip ${
                this.filterType === 'memories' ? 'active' : ''}"
            @click="${this.onFilterMemoriesClick}">
          Memories (${this.entries.length})
        </div>
      </div>

      <div class="results-list">
        ${this.displayedItems.length === 0 ? html`
          <div class="empty-state">
            <div class="empty-sparkle-container">
              <cr-icon icon="context-hub:auto-awesome" class="empty-sparkle">
              </cr-icon>
            </div>
            <div class="empty-title">${this.getEmptyStateTitle()}</div>
            <div class="empty-subtitle">${this.getEmptyStateSubtitle()}</div>
          </div>
        ` : html`
          ${this.displayedItems.map((item: DisplayItem, index: number) => html`
            <div class="item-card">
              <div class="item-header">
                <div class="item-title-wrapper">
                  ${item.url ? html`
                    <div class="item-favicon"
                        style="background-image:
                            ${getFaviconForPageURL(item.url, true)}">
                    </div>
                  ` : html`
                    <cr-icon
                        icon="context-hub:auto-awesome"
                        class="item-sparkle-icon">
                    </cr-icon>
                  `}
                  <span class="item-title" title="${item.title}">
                    ${item.title}
                  </span>
                </div>
                <div class="card-actions">
                  ${this.isHttpOrHttpsUrl(item.url) ? html`
                    <cr-icon-button
                        class="open-source-btn"
                        iron-icon="cr:open-in-new"
                        title="Open source page in browser"
                        aria-label="Open source page in browser"
                        data-url="${item.url}"
                        @click="${this.onOpenSourceClick}">
                    </cr-icon-button>
                  ` : ''}
                  <cr-icon-button
                      class="copy-btn icon-copy-content"
                      title="Copy text to clipboard"
                      aria-label="Copy text to clipboard"
                      data-index="${index}"
                      @click="${this.onCopyClick}">
                  </cr-icon-button>
                  <cr-button
                      class="attach-btn ${
                          this.isItemAttached(item.id) ? 'attached' : ''}"
                      title="${this.isItemAttached(item.id)
                          ? 'Attached as file (Click to remove)'
                          : `Attach as file (${
                              this.getPasteShortcut()} in page to paste)`}"
                      aria-label="${this.isItemAttached(item.id)
                          ? 'Remove attached file'
                          : 'Attach as file'}"
                      aria-pressed="${this.isItemAttached(item.id)}"
                      data-index="${index}"
                      @click="${this.onAttachClick}">
                    ${this.isItemAttached(item.id) ? '✓ Attached' : 'Attach'}
                  </cr-button>
                </div>
              </div>

              ${item.snippet ? html`
                <div class="item-body ${
                    this.expandedIndex === index ? 'expanded' : ''}">
                  ${item.snippet}
                </div>
                ${this.shouldShowPreviewToggle(item.snippet) ? html`
                  <button
                      class="preview-toggle-btn"
                      data-index="${index}"
                      @click="${this.onTogglePreviewClick}">
                    ${this.expandedIndex === index
                        ? 'Collapse preview ▲'
                        : 'Preview full context ▼'}
                  </button>
                ` : ''}
              ` : ''}

              <div class="item-footer">
                ${item.collection ? html`
                  <span class="collection-badge">${item.collection}</span>
                ` : ''}
                ${item.tags.map((tag: string) => html`
                  <span class="tag-pill">#${tag}</span>
                `)}
                ${this.isHttpOrHttpsUrl(item.url) ? html`
                  <span
                      class="source-url"
                      title="Open source page: ${item.url}"
                      data-url="${item.url}"
                      @click="${this.onOpenSourceClick}">
                    ${this.getHostname(item.url)} ↗
                  </span>
                ` : ''}
                ${item.relativeTime ? html`
                  <span class="relative-time">${item.relativeTime}</span>
                ` : ''}
              </div>
            </div>
          `)}
        `}
      </div>

      <cr-toast id="toast" .duration="${this.toastDuration}">
        ${!this.toastIsError ? html`
          <span class="toast-check" aria-hidden="true">✓</span>
        ` : ''}
        <div class="toast-content">
          <div class="toast-title">${this.toastMessage}</div>
          ${this.toastSubtitle ? html`
            <div class="toast-subtitle">${this.toastSubtitle}</div>
          ` : ''}
        </div>
      </cr-toast>
    </div>
  `;
}
