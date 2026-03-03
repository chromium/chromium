// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
import {html, nothing} from '//resources/lit/v3_0/lit.rollup.js';

import {type ActionChip, IconType} from '../action_chips.mojom-webui.js';

import {type ActionChipsElement} from './action_chips.js';

export function getHtml(this: ActionChipsElement) {
  // clang-format off
  return html`
  <!--_html_template_start_-->
  <div class="action-chips-wrapper">
    ${
      this.actionChips_.length ?
      html`
      <div class="action-chips-container">
      ${
          this.actionChips_.map(
              (chip: ActionChip, index: number) => html`
        <div class="chip-button-wrapper">
          <button id="${this.getId_(chip, index) || nothing}"
            class="action-chip ${
                    this.isDeepDiveChip_(chip) ? 'deep-dive-chip' : ''}"
            data-index="${index}"
            title="${this.getChipTitle_(chip)}"
            @click="${this.handleClick_}">
            <div class="action-chip-icon-container ${
                    this.getAdditionalIconClasses_(chip)}">
              ${
                    chip.suggestTemplateInfo?.typeIcon === IconType.kFavicon ?
                        html`<img class='action-chip-recent-tab-favicon'
                    src="${this.getMostRecentTabFaviconUrl_(chip)}">` :
                        ''}
            </div>
            <div class="action-chip-text-container">
              ${!this.isDeepDiveChip_(chip) ?
                  html`
                  <span class="chip-title">
                    ${chip.suggestTemplateInfo?.primaryText?.text ?? ''}
                  </span>` :
                  ''}
              <span
                title="${this.getChipTitle_(chip)}"
                class="chip-body">
                ${this.getChipSubtitle_(chip)}
              </span>
            </div>
            ${this.showDismissalUI_ ? html`
              <cr-icon-button
                class="chip-remove-button" data-index="${index}"
                @click="${this.removeChip_}">
              </cr-icon-button>
                ` : nothing}
          </button>
        </div>`)}
      </div>
      ` : nothing}
  </div>
  <!--_html_template_end_-->`;
  // clang-format on
}
