// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {ProductSelectorElement} from './product_selector.js';

// clang-format off
export function getHtml(this: ProductSelectorElement) {
  return html`<!--_html_template_start_-->
  <div id="currentProductContainer"
      @click="${this.showMenu_}"
      @keydown="${this.onCurrentProductContainerKeyDown_}"
      tabindex="0">
    ${this.selectedItem ? html`
        <cr-url-list-item id="currentProduct" size="medium"
          url="${this.selectedItem?.url || ''}"
          title="${this.selectedItem?.title || ''}"
          description="${this.getUrl_(this.selectedItem)}" tabindex="-1">
        </cr-url-list-item>
    ` : html`
        <div id="emptyState">$i18n{emptyProductSelector}</div>
    `}
    <cr-icon icon="cr:expand-more"></cr-icon>
    <div id="hoverLayer"></div>
  </div>

  <product-selection-menu id="productSelectionMenu"
      .selectedUrl="${this.getSelectedUrl_()}"
      .excludedUrls="${this.excludedUrls}"
      @close-menu="${this.onCloseMenu_}">
  </product-selection-menu>
  <!--_html_template_end_-->`;
}
// clang-format on
