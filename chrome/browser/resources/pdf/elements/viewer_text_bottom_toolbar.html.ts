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
  return html`<!--_html_template_start_-->
      <select class="md-select" @change="${this.onTypefaceSelected}"
          aria-label="$i18n{ink2TextFont}">
        ${this.fontNames.map(typeface => html`
          <option value="${typeface}"
              ?selected="${this.isSelectedTypeface(typeface)}">
            ${this.i18n(this.getLabelForTypeface(typeface))}
          </option>`)}
      </select>
      <select class="md-select size-select" @change="${this.onSizeSelected}"
          aria-label="$i18n{ink2TextFontSize}">
        ${this.sizes.map(size => html`
          <option value="${size}" ?selected="${this.isSelectedSize(size)}">
            ${size}
          </option>`)}
      </select>
      <text-styles-selector class="toolbar-icon"></text-styles-selector>
      <viewer-bottom-toolbar-dropdown id="alignment" class="toolbar-icon"
          button-title="$i18n{ink2TextAlignment}">
        <cr-icon slot="icon" icon="${this.getAlignmentIcon_()}"></cr-icon>
        <text-alignment-selector slot="menu"></text-alignment-selector>
      </viewer-bottom-toolbar-dropdown>
      <viewer-bottom-toolbar-dropdown id="color" class="toolbar-icon"
          button-title="$i18n{ink2TextColor}">
        <div slot="icon" class="color-chip"></div>
        <ink-color-selector slot="menu" label="$i18n{ink2TextColor}"
            .colors="${this.colors}" .currentColor="${this.currentColor}"
            @current-color-changed="${this.onCurrentColorChanged}">
        </ink-color-selector>
      </viewer-bottom-toolbar-dropdown>
  <!--_html_template_end_-->`;
  // clang-format on
}
