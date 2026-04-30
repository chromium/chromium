// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {ShoppingListElement} from './shopping_list.js';

export function getHtml(this: ShoppingListElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
<div id="container" role="treeitem" aria-expanded="${this.open_}">
  <button class="row" title="$i18n{shoppingListFolderTitle}"
      draggable="false" @click="${this.onFolderClick_}">
    <div id="arrow">
      <cr-icon-button
          id="arrowIcon" iron-icon="cr:arrow-drop-down"
          ?open="${this.open_}" tabindex="-1">
      </cr-icon-button>
    </div>
    <cr-icon class="icon" icon="shopping-list:shopping-list-icon"></cr-icon>
    <div class="title">$i18n{shoppingListFolderTitle}</div>
  </button>
  ${this.open_ ? html`
    ${this.productInfos.map((item, index) => html`
      <button class="product-item" role="treeitem"
          data-index="${index}"
          aria-labelledby="productInfo-${index}"
          @click="${this.onProductClick_}"
          @auxclick="${this.onProductAuxclick_}"
          @contextmenu="${this.onProductContextmenu_}">
        ${item.info.imageUrl.length ? html`
          <div class="product-image-container item-image">
              <img class="product-image" is="cr-auto-img"
                  auto-src="${item.info.imageUrl}"
                  data-index="${index}"
                  @load="${this.onProductImageLoad_}"
                  @error="${this.onImageLoadError_}"></img>
          </div>
        ` : html`
          <div class="favicon-image item-image"
              style="background-image:
                  ${this.getFaviconUrl_(item.info.productUrl)}">
          </div>
        `}
        <div class="product-info" id="productInfo-${index}">
          <span class="product-title">${item.info.title}</span>
          <span class="product-domain">${item.info.domain}</span>
          ${!item.info.previousPrice ? html`
            <span class="price">${item.info.currentPrice}</span>
          ` : html`
            <div class="price-container">
              <span class="price new-price">${item.info.currentPrice}</span>
              <span class="price old-price">${item.info.previousPrice}</span>
            </div>
          `}
        </div>
        <cr-icon-button class="action-button" data-index="${index}"
            @click="${this.onActionButtonClick_}"
            iron-icon="${this.getIconForItem_(item)}"
            title="${this.getButtonDescriptionForItem_(item)}">
        </cr-icon-button>
      </button>
    `)}
  ` : ''}
  <cr-toast id="errorToast" duration="5000">
    <div>$i18n{shoppingListErrorMessage}</div>
    <cr-button @click="${this.onErrorRetryClick_}">
      $i18n{shoppingListErrorButton}
    </cr-button>
  </cr-toast>
</div>
<!--_html_template_end_-->`;
  // clang-format on
}
