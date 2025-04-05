// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_icon/cr_icon.js';
import './icons.html.js';
import './ink_color_selector.js';
import './text_alignment_selector.js';
import './text_styles_selector.js';
import './viewer_bottom_toolbar_dropdown.js';

import {html} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import type {ViewerTextBottomToolbarElement} from './viewer_text_bottom_toolbar.js';

export function getHtml(this: ViewerTextBottomToolbarElement) {
  // clang-format off
  return html`
      <select class="md-select" @change="${this.onFontSelected}"=>
        ${this.fonts.map(font => html`
          <option value="${font}" ?selected="${this.isSelectedFont(font)}">
            ${font}
          </option>`)}
      </select>
      <select class="md-select size-select" @change="${this.onSizeSelected}">
        ${this.sizes.map(size => html`
          <option value="${size}" ?selected="${this.isSelectedSize(size)}">
            ${size}
          </option>`)}
      </select>
      <text-styles-selector></text-styles-selector>
      <viewer-bottom-toolbar-dropdown id="alignment" .buttonTitle="Alignment">
        <cr-icon slot="icon" icon="${this.getAlignmentIcon_()}"></cr-icon>
        <text-alignment-selector slot="menu"></text-alignment-selector>
      </viewer-bottom-toolbar-dropdown>
      <viewer-bottom-toolbar-dropdown id="color" .buttonTitle="Text Color">
        <div slot="icon" class="color-chip"></div>
        <ink-color-selector slot="menu" .colors="${this.colors}"
            .currentColor="${this.currentColor}"
            @current-color-changed="${this.onCurrentColorChanged}">
        </ink-color-selector>
      </viewer-bottom-toolbar-dropdown>
  `;
  // clang-format on
}
