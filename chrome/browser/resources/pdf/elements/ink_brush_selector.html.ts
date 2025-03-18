// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';

import {html} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {BRUSH_TYPES} from './ink_brush_selector.js';
import type {InkBrushSelectorElement} from './ink_brush_selector.js';

export function getHtml(this: InkBrushSelectorElement) {
  // clang-format off
  return html`
    <div>
      ${BRUSH_TYPES.map(brush =>  html`
        <cr-icon-button id="${brush}"
            iron-icon="${this.getIcon_(brush)}"
            data-brush="${brush}"
            data-selected="${this.isCurrentType_(brush)}"
            aria-pressed="${this.isCurrentType_(brush)}"
            aria-label="${this.getLabel_(brush)}"
            title="${this.getLabel_(brush)}"
            @click="${this.onBrushClick_}">
        </cr-icon-button>`)}
    </div>
  `;
  // clang-format on
}
