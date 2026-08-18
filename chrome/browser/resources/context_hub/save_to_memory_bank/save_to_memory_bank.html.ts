// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {getFaviconForPageURL} from '//resources/js/icon.js';
import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {SaveToMemoryBankElement} from './save_to_memory_bank.js';

export function getHtml(this: SaveToMemoryBankElement) {
  return html`
    <div class="dialog-container">
      <div class="content-area">
        <!-- Page title -->
        <div class="page-title">Add to Memory</div>

        <!-- Card Preview -->
        <div class="card-preview">
          <!-- Snippet -->
          ${
      this.snippet ? html`
            <div class="card-snippet">
              <div class="snippet-text">${this.snippet}</div>
            </div>
          ` :
                     ''}

          <!-- Header -->
          <div class="card-header">
            <div class="header-content">
              <div class="favicon-group">
                <div class="favicon"
                    style="background-image: ${
      getFaviconForPageURL(this.getDisplayUrl() || 'about:blank', false)}">
                </div>
              </div>
              <div class="header-text">
                <div class="header-title" title="${this.title}">
                  ${this.title}
                </div>
                <div class="header-url" title="${this.getDisplayUrl()}">
                  ${this.getDisplayUrl()}
                </div>
              </div>
            </div>
          </div>
        </div>

        <!-- Form Sections -->
        <div class="form-sections">
          <!-- Collection Section -->
          <div class="form-section section-collection">
            <div class="section-header">
              <span class="section-title">Collection</span>
              <cr-icon-button class="add-btn" iron-icon="cr:add"
                  aria-label="Add collection"
                  @click="${this.onAddCollectionClick}">
              </cr-icon-button>
            </div>
            <div class="combobox-container">
              ${
      this.isAddingCollection ? html`
                <cr-input class="collection-input"
                    placeholder="New collection..."
                    .value="${this.newCollectionInput}"
                    @value-changed="${this.onNewCollectionValueChanged}"
                    @keydown="${this.onNewCollectionKeydown}">
                </cr-input>
              ` :
                                html`
                <select class="md-select" .value="${this.collection}"
                    @change="${this.onCollectionChange}">
                  ${this.collections.map(col => html`
                    <option value="${col}"
                        ?selected="${col === this.collection}">
                      ${col}
                    </option>
                  `)}
                </select>
              `}
            </div>
          </div>

          <!-- Tags Section -->
          <div class="form-section section-tags">
            <div class="section-header">
              <span class="section-title">Tags</span>
              <cr-icon-button class="add-btn" iron-icon="cr:add"
                  aria-label="Add tag"
                  @click="${this.onAddTagClick}">
              </cr-icon-button>
            </div>
            <div class="filter-chips-row">
              ${this.tags.map(tag => html`
                <div class="filter-chip">
                  <span class="chip-text">${tag}</span>
                  <cr-icon-button class="chip-close-btn" iron-icon="cr:close"
                      data-tag="${tag}"
                      aria-label="${'Remove tag ' + tag}"
                      @click="${this.onRemoveTagClick}">
                  </cr-icon-button>
                </div>
              `)}
              ${
      this.isCreatingCustomTag ? html`
                <cr-input class="tag-input" placeholder="Add tag..."
                    .value="${this.newTagInput}"
                    @value-changed="${this.onNewTagValueChanged}"
                    @keydown="${this.onNewTagKeydown}">
                </cr-input>
              ` :
                                 ''}
            </div>
          </div>

          <!-- Note Section -->
          <div class="form-section section-note">
            <div class="section-header">
              <span class="section-title">Note</span>
            </div>
            <cr-textarea class="text-area-box"
                placeholder="Add a note..."
                rows="3"
                .value="${this.note}"
                @value-changed="${this.onNoteValueChanged}">
            </cr-textarea>
          </div>
        </div>

        <!-- Button Row -->
        <div class="button-row">
          <cr-button class="cancel-button"
              @click="${this.onCancelClick}">Cancel</cr-button>
          <cr-button class="action-button"
              @click="${this.onSaveClick}">Save</cr-button>
        </div>
      </div>
    </div>
  `;
}
