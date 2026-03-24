// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import type {SkillsEmojiPickerElement} from './skills_emoji_picker.js';

export function getHtml(this: SkillsEmojiPickerElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
<div id="container" @keydown="${this.onKeydown_}" role="dialog"
    aria-label="$i18n{emojiPickerAriaLabel}">
  <div id="searchContainer">
    <cr-input id="searchInput"
        placeholder="$i18n{emojiSearchPlaceholder}"
        .value="${this.searchQuery_}"
        @value-changed="${this.onSearchInputValueChanged_}"
        clear-button-visible>
      <cr-icon slot="inline-prefix" icon="cr:search"></cr-icon>
    </cr-input>
  </div>
  <div id="emojiGrid" role="grid">
    ${this.getEmojiCategories_().map(category => html`
      <div class="category-section" role="rowgroup">
        <div class="category-title" role="heading" aria-level="3">
          ${category}
        </div>
        <div class="emoji-row">
          ${this.getEmojisByCategory_(category).map(emoji => html`
            <button class="emoji-button" title="${emoji.name}"
                aria-label="${emoji.name}"
                data-emoji="${emoji.emoji}"
                @click="${this.onEmojiClick_}"
                role="gridcell">
              ${emoji.emoji}
            </button>
          `)}
        </div>
      </div>
    `)}
  </div>
</div>
<!--_html_template_end_-->`;
  // clang-format on
}
