// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
import {html, nothing} from 'chrome://resources/lit/v3_0/lit.rollup.js';
import {IconType} from 'chrome://resources/mojo/components/omnibox/browser/suggest_template_info.mojom-webui.js';

import type {ActionChip} from '../action_chips.mojom-webui.js';

import {type ActionChipsElement} from './action_chips.js';

export function getHtml(this: ActionChipsElement) {
  return html`
<!--_html_template_start_-->
<div class="action-chips-wrapper">
  ${this.actionChips_.length ? html`
    <div class="action-chips-container">
      ${this.actionChips_.map((item: ActionChip, index: number) => html`
        <div class="chip-button-wrapper">
          <button class="action-chip" data-index="${index}"
              title="${this.getChipTitle_(item)}" @click="${this.onClick_}"
              @auxclick="${this.onAuxclick_}"
              @contextmenu="${this.disablementContextMenuEnabled_ ?
                  this.onContextmenu_ :
                  nothing}">
            <div
                class="action-chip-icon-container ${
                    this.getAdditionalIconClasses_(item)}">
              ${item.suggestTemplateInfo.typeIcon === IconType.kFavicon ?
                  html`
                <img class="action-chip-recent-tab-favicon"
                    src="${this.getMostRecentTabFaviconUrl_(item)}">
              ` : ''}
              ${this.getIcon_(item) ? html`
                <cr-icon icon="${this.getIcon_(item)}"></cr-icon>
              ` : ''}
            </div>
            <div class="action-chip-text-container" dir="auto">
              ${item.suggestTemplateInfo.primaryText.text ? html`
                <span class="chip-title">
                  ${item.suggestTemplateInfo.primaryText.text}
                </span>
              ` : ''}
              ${!this.smallChipsEnabled_ && this.getChipSubtitle_(item) ?
                  html`
                <span title="${this.getChipTitle_(item)}" class="chip-body">
                  ${this.getChipSubtitle_(item)}
                </span>
              ` : ''}
            </div>
            ${this.showDismissalUI_ ? html`
              <cr-icon-button class="chip-remove-button" data-index="${index}"
                  @click="${this.onRemoveClick_}">
              </cr-icon-button>
            ` : nothing}
          </button>
        </div>
      `)}
    </div>
  ` : nothing}
</div>
<cr-action-menu id="actionMenu">
  <button class="dropdown-item" @click="${this.onDisableSuggestionClick_}">
    ${this.i18n('disableSuggestion')}
  </button>
</cr-action-menu>
<!--_html_template_end_-->`;
}
