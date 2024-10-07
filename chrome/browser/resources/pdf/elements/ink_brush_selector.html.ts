// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';

import {html} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {AnnotationBrushType} from '../constants.js';

import type {InkBrushSelectorElement} from './ink_brush_selector.js';

export function getHtml(this: InkBrushSelectorElement) {
  // clang-format off
  return html`
    <div role="listbox">
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
  `;
  // clang-format on
}
