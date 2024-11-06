// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_button/cr_button.js';

import {html} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import type {ViewerBottomToolbarDropdownElement} from './viewer_bottom_toolbar_dropdown.js';

export function getHtml(this: ViewerBottomToolbarDropdownElement) {
  return html`
    <cr-button @click="${this.toggleDropdown_}"
        data-selected="${this.showDropdown_}">
      <slot name="icon"></slot>
    </cr-button>
    <div>
      ${this.showDropdown_ ? html`<slot name="menu"></slot>` : ''}
    </div>
  `;
}
