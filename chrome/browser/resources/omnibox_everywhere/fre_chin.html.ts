// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {FreChinElement} from './fre_chin.js';

export function getHtml(this: FreChinElement) {
  return html`
    <div class="chin-container">
      <div class="left-section">
        <div class="keyboard-icon" aria-hidden="true"></div>

        ${this.isSetupMode_() ? html`
          <div class="text-container">
            <div class="dropdown-wrapper">
              <button class="dropdown-trigger"
                  aria-label="${this.getDropdownAriaLabel_()}"
                  aria-haspopup="menu"
                  aria-expanded="${this.dropdownOpen ? 'true' : 'false'}"
                  @click="${this.onDropdownClick_}">
                ${this.hasHotkeyTokens_() ? html`
                  <div class="keys-container">
                    ${this.hotkeyTokens.map(token => html`
                      <span class="key-badge">${token}</span>
                    `)}
                  </div>
                ` : html`
                  <span class="select-shortcut-label">
                    ${this.i18n('loomniboxFreSelectShortcut')}
                  </span>
                `}
                <cr-icon class="dropdown-caret"
                    aria-hidden="true"
                    icon="cr:arrow-drop-down">
                </cr-icon>
              </button>
            </div>

            <span class="to-search-label"
                @click="${this.onLabelClick_}"
                .innerHTML="${this.getToSearchLabelHtml_()}">
            </span>
          </div>
        ` : html`
          <div class="text-container">
            <div class="hotkey-container">
              <div class="keys-container">
                ${this.hotkeyTokens.map(token => html`
                  <span class="key-badge">${token}</span>
                `)}
              </div>
            </div>

            <span class="to-search-label">
              ${this.i18n('loomniboxFreReminderToSearch')}
            </span>

            <span class="change-shortcut-label"
                @click="${this.onLabelClick_}"
                .innerHTML="${this.getChangeShortcutLabelHtml_()}">
            </span>
          </div>
        `}
      </div>

      <button class="close-button"
          aria-label="${this.i18n('loomniboxFreCloseButtonAria')}"
          @click="${this.onCloseClick_}">
        <cr-icon icon="cr:close" aria-hidden="true"></cr-icon>
      </button>
    </div>
  `;
}
