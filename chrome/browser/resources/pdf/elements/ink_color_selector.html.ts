// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_grid/cr_grid.js';

import {html} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import type {InkColorSelectorElement} from './ink_color_selector.js';

export function getHtml(this: InkColorSelectorElement) {
  return html`<!--_html_template_start_-->
    <cr-grid role="radiogroup" columns="5" focus-selector=".color-chip"
        aria-label="${this.label}"
        @cr-grid-focus-changed="${this.onCrGridFocusChanged_}">
      ${this.colors.map(item => html`
        <label class="color-item">
          <input type="radio" class="color-chip ${this.getBlendedClass_(item)}"
              name="color" .value="${item.color}"
              .style="--item-color: ${item.color}"
              aria-label="${this.i18n(item.label)}"
              tabindex="${this.getTabIndex_(item.color)}"
              title="${this.i18n(item.label)}"
              @click="${this.onColorClick_}"
              ?checked="${this.isCurrentColor_(item.color)}">
        </label>`)}
    </cr-grid>
  <!--_html_template_end_-->`;
}
