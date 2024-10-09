// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './icons.html.js';
import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';

import {html} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import type {InkSizeSelectorElement} from './ink_size_selector.js';

export function getHtml(this: InkSizeSelectorElement) {
  return html`
    <div role="listbox" @keydown="${this.onSizeKeydown_}">
      ${this.getCurrentBrushSizes_().map((item, index) => html`
        <cr-icon-button iron-icon="pdf:${item.icon}" role="option"
            data-index="${index}" data-size="${item.size}"
            data-selected="${this.isCurrentSize_(item.size)}"
            aria-selected="${this.isCurrentSize_(item.size)}"
            @click="${this.onSizeClick_}"></cr-icon-button>
      `)}
    </div>
  `;
}
