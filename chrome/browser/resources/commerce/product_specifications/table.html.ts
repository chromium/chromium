// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {TableElement} from './table.js';

// clang-format off
export function getHtml(this: TableElement) {
  return html`<!--_html_template_start_-->
  <div id="table" @pointerleave="${this.onHideOpenTabButton_}">
    ${this.columns.map((column, columnIndex) => html`
      <div class="col"
          style="grid-row: span ${this.getRowCount_(column.productDetails?.length || 0)};
              scroll-snap-align: ${this.getScrollSnapAlign_()};"
          data-index="${columnIndex}"
          @pointerenter="${this.onShowOpenTabButton_}"
          ?is-dragging="${this.isDragging_(columnIndex)}"
          ?is-first-column="${this.isFirstColumn_(columnIndex)}">
        <product-selector
            draggable="true"
            .selectedItem="${column.selectedItem}"
            .excludedUrls="${this.getUrls_()}"
            @selected-url-change="${this.onSelectedUrlChange_}"
            data-index="${columnIndex}"
            @remove-url="${this.onUrlRemove_}">
        </product-selector>
        <div class="img-container"
            draggable="true"
            data-index="${columnIndex}"
            @click="${this.onOpenTabButtonClick_}">
          ${column.selectedItem.imageUrl.length ? html`
            <img class="main-image" is="cr-auto-img"
                auto-src="${column.selectedItem.imageUrl}"
                draggable="false">
          ` : html`
            <div class="main-image favicon"
                style="background-image:
                    ${this.getFavicon_(column.selectedItem.url)}">
            </div>
          `}
          <cr-icon-button class="open-tab-button icon-external"
              ?hidden="${!this.showOpenTabButton_(columnIndex)}"
              title="$i18n{openProductPage}">
          </cr-icon-button>
        </div>
        ${column.productDetails?.map((detail, rowIndex) => html`
          <div class="detail-container"
              ?hidden="${!this.showRow_(detail.title || '', rowIndex)}">

            ${detail.title ? html`
              <div class="detail-title">
                <span>${detail.title}</span>
              </div>
            ` : ''}
            ${this.contentIsString_(detail.content) ? html`
              <div class="detail-text">${detail.content}</div>
            ` : ''}
            ${this.contentIsProductDescription_(detail.content)? html`
              <description-section
                  .description="${this.filterProductDescription_(detail.content,
                      detail.title || '', rowIndex)}"
                  product-name="${column.selectedItem.title}">
              </description-section>
            ` : ''}
            ${this.contentIsBuyingOptions_(detail.content) ? html`
              <buying-options-section .price="${detail.content.price}"
                  .jackpotUrl="${detail.content.jackpotUrl}">
              </buying-options-section>
            ` : ''}
            ${detail.content === null ? html`
              <empty-section></empty-section>
            `: ''}
          </div>
        `)}
      </div>
    `)}
    <div id="newColumnSelectorContainer">
      <slot name="selectorGradient"></slot>
      <slot name="newColumnSelector"></slot>
    </div>
  </div>
  <!--_html_template_end_-->`;
}
// clang-format on
