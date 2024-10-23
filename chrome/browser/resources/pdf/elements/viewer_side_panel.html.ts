// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import type {ViewerSidePanelElement} from './viewer_side_panel.js';

export function getHtml(this: ViewerSidePanelElement) {
  // clang-format off
  return html`
    <ink-brush-selector .currentType="${this.currentType}">
    </ink-brush-selector>
    <div id="brush-options">
      <h2>Size</h2>
      <ink-size-selector .currentSize="${this.currentSize}"
          .currentType="${this.currentType}"></ink-size-selector>
      ${this.shouldShowColorOptions_() ? html`
        <h2>Color</h2>
        <ink-color-selector .currentColor="${this.currentColor}"
            .currentType="${this.currentType}">
        </ink-color-selector>` : ''}
    </div>
  `;
  // clang-format on
}
