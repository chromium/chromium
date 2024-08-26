// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {ComboboxGroup, ComboboxItem, CustomizeChromeComboboxElement} from './customize_chrome_combobox.js';

export function getHtml(this: CustomizeChromeComboboxElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
<button id="input" class="md-select"
    role="combobox" tabindex="0"
    aria-controls="dropdown"
    aria-expanded="${this.expanded_}"
    aria-haspopup="listbox"
    aria-activedescendant="${this.getAriaActiveDescendant_()}"
    @click="${this.onInputClick_}"
    @focusout="${this.onInputFocusout_}">
  <div>${this.getInputLabel_()}</div>
</button>
<div id="dropdownContainer">
  <div id="dropdown" role="listbox" @click="${this.onDropdownClick_}"
      @pointerdown="${this.onDropdownPointerdown_}"
      @pointermove="${this.onDropdownPointermove_}"
      @pointerover="${this.onDropdownPointerover_}">
    <div id="defaultOption" class="item" role="option"
        aria-selected="${this.getDefaultItemAriaSelected_()}">
      ${this.defaultOptionLabel}
    </div>
    ${this.items.map((item, index) => html`
      ${this.isGroup_(item) ? html`
        <div class="group" role="group">
          <label role="button" class="group-item"
              data-index="${index}" @click="${this.onGroupClick_}"
              aria-expanded="${this.getGroupAriaExpanded_(index)}">
            ${item.label}
            <cr-icon icon="${this.getGroupIcon_(index)}"
                aria-hidden="true">
            </cr-icon>
          </label>
          ${this.isGroupExpanded_(index) ? html`
            ${(item as ComboboxGroup).items.map(subitem => html`
              <div class="item" role="option" .value="${subitem.key}"
                  aria-selected="${this.isItemSelected_(subitem)}">
                <cr-icon icon="cr:check" aria-hidden="true"></cr-icon>
                <span title=${subitem.label}>${subitem.label}</span>
              </div>
            `)}
          `: ''}
        </div>
      ` : ''}
      ${!this.isGroup_(item) ? html`
        <div class="item" role="option" .value="${item.key}"
            aria-selected="${this.isItemSelected_(item)}">
          ${(item as ComboboxItem).imagePath ? html`
            <customize-chrome-check-mark-wrapper
                ?checked="${this.isItemSelected_(item)}">
              <img is="cr-auto-img"
                  .autoSrc="${(item as ComboboxItem).imagePath}">
            </customize-chrome-check-mark-wrapper>
          `: ''}
          <cr-icon icon="cr:check"
              ?hidden="${!!(item as ComboboxItem).imagePath}"></cr-icon>
          <span>${item.label}</span>
        </div>
      `: ''}
    `)}
  </div>
</div><!--_html_template_end_-->`;
  // clang-format on
}
