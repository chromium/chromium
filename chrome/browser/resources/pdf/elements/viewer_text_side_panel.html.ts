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
      <h2>Font</h2>
      <select class="md-select" style="font-family: '${this.currentFont_}';"
          @change="${this.onFontChange_}"=>
        ${this.fonts_.map(font => html`
          <option value="${font}" ?selected="${this.isSelectedFont_(font)}">
            ${font}
          </option>`)}
      </select>
      <select class="md-select" @change="${this.onSizeChange_}">
        ${this.sizes_.map(size => html`
          <option value="${size}" ?selected="${this.isSelectedSize_(size)}">
            ${size}
          </option>`)}
      </select>
    </div>
    <div class="side-panel-content">
      <h2>Styles</h2>
      <text-styles-selector></text-styles-selector>
      <text-alignment-selector></text-alignment-selector>
    </div>
    <div class="side-panel-content">
      <h2>Text color</h2>
      <ink-color-selector .colors="${this.colors_}"
          .currentColor="${this.currentColor_}"
          @current-color-changed="${this.onCurrentColorChanged_}">
      </ink-color-selector>
    </div>
  <!--_html_template_end_-->`;
  // clang-format on
}
