// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {AnnotationBrushType} from '../constants.js';

import type {ViewerSidePanelElement} from './viewer_side_panel.js';

export function getHtml(this: ViewerSidePanelElement) {
  // clang-format off
  return html`
    <div id="brush-selector" role="listbox">
      <!-- TODO(crbug.com/351868764): Set production icon and aria. -->
      <cr-icon-button id="pen" role="option"
          iron-icon="${this.getIcon_(AnnotationBrushType.PEN)}"
          data-brush="${AnnotationBrushType.PEN}"
          data-selected="${this.isCurrentType_(AnnotationBrushType.PEN)}"
          aria-selected="${this.isCurrentType_(AnnotationBrushType.PEN)}"
          @click="${this.onBrushClick_}">
      </cr-icon-button>
      <cr-icon-button id="highlighter" role="option"
          iron-icon="${this.getIcon_(AnnotationBrushType.HIGHLIGHTER)}"
          data-brush="${AnnotationBrushType.HIGHLIGHTER}"
          data-selected="${
            this.isCurrentType_(AnnotationBrushType.HIGHLIGHTER)}"
          aria-selected="${
            this.isCurrentType_(AnnotationBrushType.HIGHLIGHTER)}"
          @click="${this.onBrushClick_}">
      </cr-icon-button>
      <cr-icon-button id="eraser" role="option"
          iron-icon="${this.getIcon_(AnnotationBrushType.ERASER)}"
          data-brush="${AnnotationBrushType.ERASER}"
          data-selected="${this.isCurrentType_(AnnotationBrushType.ERASER)}"
          aria-selected="${this.isCurrentType_(AnnotationBrushType.ERASER)}"
          @click="${this.onBrushClick_}">
      </cr-icon-button>
    </div>
    <div id="brush-options">
      <h2>Size</h2>
      <div id="sizes" role="listbox">
        ${this.getCurrentBrushSizes_().map(item => html`
          <cr-icon-button iron-icon="pdf:${item.icon}" role="option"
              data-size="${item.size}"
              data-selected="${this.isCurrentSize_(item.size)}"
              aria-selected="${this.isCurrentSize_(item.size)}"
              @click="${this.onSizeClick_}"></cr-icon-button>`)}
      </div>
      ${this.shouldShowColorOptions_() ? html`
        <h2>Color</h2>
        <div id="colors">
          ${this.getCurrentBrushColors_().map(item => html`
          <label class="color-item">
            <input type="radio" class="color-chip"
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
