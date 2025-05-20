// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './ink_color_selector.js';
import './text_alignment_selector.js';
import './text_styles_selector.js';

import {html} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import type {ViewerTextSidePanelElement} from './viewer_text_side_panel.js';

export function getHtml(this: ViewerTextSidePanelElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
    <div class="side-panel-content">
      <h2>$i18n{ink2TextFont}</h2>
      <select class="md-select" @change="${this.onTypefaceSelected}"
          aria-label="$i18n{ink2TextFont}">
        ${this.fontNames.map(typeface => html`
          <option value="${typeface}"
              ?selected="${this.isSelectedTypeface(typeface)}">
            ${this.i18n(this.getLabelForTypeface(typeface))}
          </option>`)}
      </select>
      <select class="md-select" @change="${this.onSizeSelected}"
          aria-label="$i18n{ink2TextFontSize}">
        ${this.sizes.map(size => html`
          <option value="${size}" ?selected="${this.isSelectedSize(size)}">
            ${size}
          </option>`)}
      </select>
    </div>
    <div class="side-panel-content">
      <h2>$i18n{ink2TextStyles}</h2>
      <text-styles-selector></text-styles-selector>
      <text-alignment-selector></text-alignment-selector>
    </div>
    <div class="side-panel-content">
      <h2>$i18n{ink2TextColor}</h2>
      <ink-color-selector label="$i18n{ink2TextColor}"
          .colors="${this.colors}" .currentColor="${this.currentColor}"
          @current-color-changed="${this.onCurrentColorChanged}">
      </ink-color-selector>
    </div>
  <!--_html_template_end_-->`;
  // clang-format on
}
