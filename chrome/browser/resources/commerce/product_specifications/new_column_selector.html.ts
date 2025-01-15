// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {NewColumnSelectorElement} from './new_column_selector.js';

export function getHtml(this: NewColumnSelectorElement) {
  return html`<!--_html_template_start_-->
  <div id="button"
      @click="${this.showMenu_}"
      @keydown="${this.onButtonKeyDown_}"
      tabindex="0"
      title="$i18n{addNewColumn}"
      aria-label="$i18n{addNewColumn}">
    <cr-icon icon="cr:add"></cr-icon>
    <div id="hoverLayer"></div>
  </div>

  <product-selection-menu id="productSelectionMenu"
      .excludedUrls="${this.excludedUrls}"
      .isTableFull="${this.isTableFull}"
      .forNewColumn="true"
      @close-menu="${this.onCloseMenu_}">
  </product-selection-menu>
  <!--_html_template_end_-->`;
}
