// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';

import {html} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import type {ViewerBottomToolbarDropdownElement} from './viewer_bottom_toolbar_dropdown.js';

export function getHtml(this: ViewerBottomToolbarDropdownElement) {
  return html`
    <cr-icon-button iron-icon="${this.buttonIcon}"
        @click="${this.toggleDropdown_}"
        data-selected="${this.showDropdown_}"></cr-icon-button>
    <div>
      ${this.showDropdown_ ? html`<slot></slot>` : ''}
    </div>
  `;
}
