// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {MemoryBanksEditDialogElement} from './memory_banks_edit_dialog.js';

export function getHtml(this: MemoryBanksEditDialogElement) {
  return html`
    <cr-dialog id="dialog" show-on-attach @close="${this.onDialogClose_}">
      <div slot="title">Edit memory</div>
      <div slot="body" class="edit-dialog-sections">
        <!-- Collection Section -->
        <div class="edit-form-section">
          <div class="edit-section-header">
            <span class="edit-section-title">Collection</span>
            <cr-icon-button class="add-btn" iron-icon="cr:add"
                @click="${this.onAddCollectionClick_}">
            </cr-icon-button>
          </div>
          ${
      this.isAddingCollection_ ? html`
            <cr-input class="collection-input" autofocus
                placeholder="New collection..."
                .value="${this.newCollectionInput_}"
                @value-changed="${this.onNewCollectionInputValueChanged_}"
                @keydown="${this.onNewCollectionInputKeydown_}">
            </cr-input>
          ` :
                                 html`
            <select class="md-select" .value="${this.editCollection_}"
                @change="${this.onEditCollectionChange_}">
              <option value="" ?selected="${!this.editCollection_}">
                None
              </option>
              ${this.getEditDialogCollections_().map(col => html`
                <option value="${col}"
                    ?selected="${this.editCollection_ === col}">
                  ${col}
                </option>
              `)}
            </select>
          `}
        </div>

        <!-- Tags Section -->
        <div class="edit-form-section">
          <div class="edit-section-header">
            <span class="edit-section-title">Tags</span>
            <cr-icon-button class="add-btn" iron-icon="cr:add"
                @click="${this.onAddTagClick_}">
            </cr-icon-button>
          </div>
          <div class="filter-chips-row">
            ${this.editTags_.map(tag => html`
              <div class="filter-chip">
                <span class="chip-text">${tag}</span>
                <cr-icon-button class="chip-close-btn" iron-icon="cr:close"
                    data-tag="${tag}" @click="${this.onRemoveTagClick_}">
                </cr-icon-button>
              </div>
            `)}
            ${
      this.isCreatingTag_ ?
          html`
              <div class="tag-input-container">
                <cr-input class="tag-input" autofocus placeholder="Add tag..."
                    .value="${this.newTagInput_}"
                    @value-changed="${this.onNewTagInputValueChanged_}"
                    @blur="${this.onNewTagBlur_}"
                    @keydown="${this.onNewTagInputKeydown_}">
                </cr-input>
                ${
              this.getFilteredTags_().length > 0 ?
                  html`
                  <div class="suggestions-dropdown tag-suggestions"
                      role="listbox">
                    ${
                      this.getFilteredTags_().map(
                          (tag, index) => html`
                      <div class="suggestion-item ${
                              index === this.highlightedTagIndex_ ?
                                  'highlighted' :
                                  ''}"
                          role="option"
                          data-tag="${tag}"
                          title="${tag}"
                          aria-selected="${index === this.highlightedTagIndex_}"
                          @mousedown="${this.onTagSuggestionMousedown_}">
                        ${tag}
                      </div>
                    `)}
                  </div>
                ` :
                  ''}
              </div>
            ` :
          ''}
          </div>
        </div>

        <!-- Note Section -->
        <div class="edit-form-section">
          <div class="edit-section-header">
            <span class="edit-section-title">Note</span>
          </div>
          <cr-textarea class="text-area-box" placeholder="Add a note..."
              rows="3" .value="${this.editNote_}"
              @value-changed="${this.onEditNoteValueChanged_}">
          </cr-textarea>
        </div>
      </div>
      <div slot="button-container">
        <cr-button class="cancel-button" @click="${this.onCancelClick_}">
          Cancel
        </cr-button>
        <cr-button class="action-button" @click="${this.onSaveClick_}">
          Save
        </cr-button>
      </div>
    </cr-dialog>
  `;
}
