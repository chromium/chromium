// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
import {html, nothing} from '//resources/lit/v3_0/lit.rollup.js';

import {type ActionChip} from '../action_chips.mojom-webui.js';

import {type ActionChipsElement} from './action_chips.js';

export function getHtml(this: ActionChipsElement) {
  return html`
  <div class="action-chips-wrapper">
    ${
      this.actionChips_.length ?
      html`
      <div class="action-chips-container">
      ${
          this.actionChips_.map(
              (chip: ActionChip, index: number) => html`
        <button id="${this.getId_(chip, index) || nothing}"
          class="action-chip ${
                  this.isDeepDiveChip_(chip) ? 'deep-dive-chip' : ''}"
          title="${this.getChipTitle_(chip)}"
          @click="${() => this.handleClick_(chip)}">
          <div class="action-chip-icon-container ${
                  this.getAdditionalIconClasses_(chip)}">
            ${
                  this.isRecentTabChip_(chip) ?
                      html`<img class='action-chip-recent-tab-favicon'
                  src="${this.getMostRecentTabFaviconUrl_(chip)}">` :
                      ''}
          </div>
          <div class="action-chip-text-container">
            ${
                !this.isDeepDiveChip_(chip) ?
                    html`<span class="chip-title">${chip.title}</span>` :
                    ''}
            <span
              title="${this.getChipTitle_(chip)}"
              class="chip-body">
                ${this.showDashSimplifiedUI_(chip)
                    ? ' - '
                    : ''}${chip.suggestion}
            </span>
          </div>
          ${this.showDismissalUI_ ? html`
            <cr-icon-button
              class="chip-remove-button"
              @click="${(e: MouseEvent) => this.removeChip_(chip, e)}">
            </cr-icon-button>
              ` : nothing}
        </button>`)}
      </div>
      ` : nothing}
  </div>`;
}
