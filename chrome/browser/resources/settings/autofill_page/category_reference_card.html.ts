// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import type {CategoryReferenceCardElement} from './category_reference_card.js';

export function getHtml(this: CategoryReferenceCardElement) {
  return html`<!--_html_template_start_-->
<cr-link-row label="${this.cardTitle}" ?external="${this.isExternal}"
    @click="${this.onDataCategoryClick_}">
</cr-link-row>
<hr>
<div class="chips-container">
  ${this.chips.map((item, index) => html`
    <cr-button data-index="${index}" @click="${this.onDataChipClick_}">
      <div class="button-content">
        <cr-icon icon="${item.icon}"></cr-icon>
        <span>${item.label}</span>
        <span class="counter" ?hidden="${!item.count}">
          (${item.count})
        </span>
      </div>
    </cr-button>
  `)}
</div>
<!--_html_template_end_-->`;
}
