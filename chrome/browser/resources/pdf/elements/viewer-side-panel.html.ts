// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {AnnotationBrushType} from '../constants.js';

import type {ViewerSidePanelElement} from './viewer-side-panel.js';

export function getHtml(this: ViewerSidePanelElement) {
  // clang-format off
  return html`
    <div id="brush-selector">
      <!-- TODO(crbug.com/351868764): Set production icon and aria. -->
      <cr-icon-button id="pen" iron-icon="cr:create"
          data-brush="${AnnotationBrushType.PEN}"
          @click="${this.onBrushClick_}">
      </cr-icon-button>
      <cr-icon-button id="highlighter" iron-icon="pdf:highlighter"
          data-brush="${AnnotationBrushType.HIGHLIGHTER}"
          @click="${this.onBrushClick_}">
      </cr-icon-button>
      <cr-icon-button id="eraser" iron-icon="pdf:eraser"
          data-brush="${AnnotationBrushType.ERASER}"
          @click="${this.onBrushClick_}">
      </cr-icon-button>
    </div>
    <div id="brush-options">
      <h2>Size</h2>
      <div id="sizes">
        ${this.getCurrentBrushSizes_().map(item => html`
          <cr-icon-button iron-icon="cr20:menu" data-size="${item.size}"
              @click="${this.onSizeClick_}"></cr-icon-button>`)}
      </div>
      ${this.shouldShowColorOptions_() ? html`
        <h2>Color</h2>
        <div id="colors">
          ${this.getCurrentBrushColors_().map(item => html`
          <label class="color-item">
            <input type="radio" class="color-chip"
                name="${this.getColorName_()}" .value="${item.color}"
                .style="--item-color: ${item.color}"
                @click="${this.onColorClick_}"
                ?checked="${this.isCurrentColor_(item.color)}">
          </label>`)}
        </div>` : ''}
    </div>
  `;
  // clang-format on
}
