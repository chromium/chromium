// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {MemoryBanksPaletteSettingsElement} from './memory_banks_palette_settings.js';

export function getHtml(this: MemoryBanksPaletteSettingsElement) {
  return html`
    <div id="settingsHeader">
      <cr-icon-button id="backButton"
          iron-icon="cr:arrow-back"
          aria-label="Back to Memory Banks"
          title="Back"
          @click="${this.onBackButtonClick}">
      </cr-icon-button>
      <span>Settings</span>
    </div>

    <div id="shortcutSection">
      <div id="shortcutLabel">Keyboard shortcut</div>
      <div id="shortcutDescription">
        Open Memory Banks from any tab. Use a letter or number with
        ${this.getModifierHint()}.
      </div>
      <div id="shortcutRow">
        <div id="shortcutField"
            class="${this.getFieldClass()}"
            tabindex="${this.isRecording ? 0 : -1}"
            role="textbox"
            aria-labelledby="shortcutLabel"
            aria-describedby="${this.getFieldDescribedBy()}"
            aria-readonly="${!this.isRecording}"
            aria-invalid="${!!this.error}"
            @click="${this.onShortcutFieldClick}"
            @keydown="${this.onShortcutFieldKeydown}"
            @keyup="${this.onShortcutFieldKeyup}"
            @blur="${this.onShortcutFieldBlur}">
          ${this.getFieldText()}
        </div>
        ${!this.isRecording ? html`
          <cr-button id="editButton"
              aria-label="${this.getEditButtonAriaLabel()}"
              @click="${this.onEditButtonClick}">
            ${this.currentShortcutDisplayText ? 'Change' : 'Set shortcut'}
          </cr-button>
        ` : ''}
        ${!this.isRecording && this.currentShortcutDisplayText ? html`
          <cr-icon-button id="removeButton"
              iron-icon="cr:delete"
              aria-label="Remove shortcut"
              title="Remove shortcut"
              @click="${this.onRemoveButtonClick}">
          </cr-icon-button>
        ` : ''}
      </div>
      ${this.shouldShowSuggestion() ? html`
        <div id="suggestion">
          <span>
            Suggested:
            <kbd id="suggestedShortcutKey"
                >${this.suggestedShortcutDisplayText}</kbd>
          </span>
          <button id="useSuggestedButton" type="button"
              @click="${this.onUseSuggestedButtonClick}">
            Use suggested
          </button>
        </div>
      ` : ''}
      ${this.error ? html`
        <div id="error" role="alert">${this.error}</div>
      ` : ''}
    </div>
  `;
}
