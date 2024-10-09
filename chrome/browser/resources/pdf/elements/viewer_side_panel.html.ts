// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import type {ViewerSidePanelElement} from './viewer_side_panel.js';

export function getHtml(this: ViewerSidePanelElement) {
  // clang-format off
  return html`
    <ink-brush-selector @ink-brush-change="${this.onBrushChange_}">
    </ink-brush-selector>
    <div id="brush-options">
      <h2>Size</h2>
      <ink-size-selector .currentSize="${this.getCurrentSize_()}"
          .currentType="${this.currentType_}"
          @current-size-changed="${this.onSizeChange_}"></ink-size-selector>
      ${this.shouldShowColorOptions_() ? html`
        <h2>Color</h2>
        <div id="colors" @keydown="${this.onColorKeydown_}">
          ${this.getCurrentBrushColors_().map((item, index) => html`
            <label class="color-item">
              <input type="radio" class="color-chip" data-index="${index}"
                  name="${this.getColorName_()}" .value="${item.color}"
                  .style="--item-color: ${this.getVisibleColor_(item.color)}"
                  @click="${this.onColorClick_}"
                  ?checked="${this.isCurrentColor_(item.color)}">
            </label>`)}
        </div>` : ''}
    </div>
  `;
  // clang-format on
}
