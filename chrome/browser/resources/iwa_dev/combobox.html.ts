// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html, nothing} from '//resources/lit/v3_0/lit.rollup.js';

import type {IwaDevComboboxElement} from './combobox.js';

export function getHtml(this: IwaDevComboboxElement) {
  // clang-format off
  return html`
<div class="dropdown-container">
  <label for="input">${this.label}</label>
  <div class="input-container">
    <input id="input"
        class="dropdown-select"
        role="combobox"
        aria-autocomplete="${this.readonly ? 'none' : 'list'}"
        aria-haspopup="listbox"
        aria-controls="${this.isDropdownOpen_() ? 'suggestions' : nothing}"
        aria-expanded="${this.isDropdownOpen_() ? 'true' : 'false'}"
        aria-activedescendant="${this.getActiveDescendant_() || nothing}"
        aria-invalid="${this.errorMessage ? 'true' : nothing}"
        aria-describedby="${this.errorMessage ? 'error' : nothing}"
        .value="${this.getDisplayValue_()}"
        @input="${this.onInput_}"
        @focus="${this.onFocus_}"
        @blur="${this.onBlur_}"
        @keydown="${this.onKeydown_}"
        @pointerdown="${this.onInputPointerdown_}"
        placeholder="${this.placeholder}"
        ?disabled="${this.disabled}"
        ?readonly="${this.readonly}"
        autocomplete="off">
    ${this.clearable && this.value ? html`
      <cr-icon-button id="clearButton"
          iron-icon="cr:close"
          title="Clear"
          aria-label="Clear"
          ?disabled="${this.disabled}"
          @click="${this.onClearClick_}">
      </cr-icon-button>
    ` : nothing}
    ${this.options.length > 0 ? html`
      <cr-icon-button id="dropdownButton"
          iron-icon="cr:arrow-drop-down"
          title="${
              this.isDropdownOpen_() ? 'Hide suggestions' : 'Show suggestions'}"
          aria-hidden="true"
          tabindex="-1"
          ?disabled="${this.disabled}"
          @pointerdown="${this.onTogglePointerdown_}">
      </cr-icon-button>
      <div id="suggestions"
          class="suggestions-dropdown"
          role="listbox"
          aria-label="${this.label}"
          popover="manual"
          @pointerdown="${this.onSuggestionsPointerdown_}">
        ${this.getFilteredOptions_().map((opt, i) => html`
          <button id="suggestion-${i}"
              class="suggestion-item ${
                  i === this.highlightedIndex_ ? 'highlighted' : ''}"
              role="option"
              type="button"
              tabindex="-1"
              aria-selected="${i === this.highlightedIndex_ ? 'true' : 'false'}"
              value="${opt.value}"
              @pointerdown="${this.onOptionPointerdown_}">
            ${opt.label || opt.value}
          </button>
        `)}
      </div>
    ` : nothing}
  </div>
  ${this.errorMessage ? html`
    <div id="error" class="error-message" aria-live="polite">
      ${this.errorMessage}
    </div>
  ` : nothing}
</div>`;
  // clang-format on
}
