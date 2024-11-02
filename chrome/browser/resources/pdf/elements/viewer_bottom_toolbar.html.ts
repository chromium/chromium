// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './icons.html.js';
import './ink_brush_selector.js';
import './ink_color_selector.js';
import './ink_size_selector.js';
import './viewer_bottom_toolbar_dropdown.js';

import {html} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import type {ViewerBottomToolbarElement} from './viewer_bottom_toolbar.js';

export function getHtml(this: ViewerBottomToolbarElement) {
  // clang-format off
  return html`
    <ink-brush-selector .currentType="${this.currentType}">
    </ink-brush-selector>
    <span id="vertical-separator"></span>
    <viewer-bottom-toolbar-dropdown id="size"
        button-icon="${this.getSizeIcon_()}">
      <ink-size-selector .currentSize="${this.currentSize}"
          .currentType="${this.currentType}"></ink-size-selector>
    </viewer-bottom-toolbar-dropdown>
    ${this.shouldShowColorOptions_() ? html`
      <!-- TODO(crbug.com/369653190): Use actual button icons. -->
      <viewer-bottom-toolbar-dropdown id="color" button-icon="pdf:pen-size-3">
        <ink-color-selector .currentColor="${this.currentColor}"
            .currentType="${this.currentType}"></ink-color-selector>
      </viewer-bottom-toolbar-dropdown>` : ''}
  `;
  // clang-format on
}
