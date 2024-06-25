// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_auto_img/cr_auto_img.js';
import './product_selector.js';
import 'chrome://resources/cr_elements/cr_icons.css.js';
import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';
import 'chrome://resources/cr_elements/cr_hidden_style.css.js';

import {getFaviconForPageURL} from '//resources/js/icon.js';
import type {BrowserProxy} from 'chrome://resources/cr_components/commerce/browser_proxy.js';
import {BrowserProxyImpl} from 'chrome://resources/cr_components/commerce/browser_proxy.js';
import type {DomRepeatEvent} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import type {TableColumn} from './app.js';
import {getTemplate} from './table.html.js';

export interface TableElement {
  $: {
    table: HTMLElement,
  };
}

export class TableElement extends PolymerElement {
  static get is() {
    return 'product-specifications-table';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      columns: Array,
      hoveredColumnIndex_: {type: Number, value: -1},
    };
  }

  columns: TableColumn[];
  private hoveredColumnIndex_: number;

  private shoppingApi_: BrowserProxy = BrowserProxyImpl.getInstance();

  //   Determines the number of rows needed in the grid layout.
  //   This is the sum of:
  //   - 1 row for the product selector.
  //   - 1 row for the image container.
  //   - Number of product details.
  private getRowCount_(numProductDetails: number): number {
    return 2 + numProductDetails;
  }

  private getUrls_() {
    return this.columns.map(column => column.selectedItem.url);
  }

  private onHideOpenTabButton_() {
    this.hoveredColumnIndex_ = -1;
  }

  private onShowOpenTabButton_(e: DomRepeatEvent<TableColumn>&
                               {model: {columnIndex: number}}) {
    this.hoveredColumnIndex_ = e.model.columnIndex;
  }

  private showOpenTabButton_(columnIndex: number): boolean {
    return columnIndex === this.hoveredColumnIndex_;
  }

  private onOpenTabButtonClick_(e: DomRepeatEvent<TableColumn>&
                                {model: {columnIndex: number}}) {
    this.shoppingApi_.switchToOrOpenTab(
        {url: this.columns[e.model.columnIndex].selectedItem.url});
  }

  private onSelectedUrlChange_(
      e: DomRepeatEvent<TableColumn, CustomEvent<{url: string}>>&
      {model: {columnIndex: number}}) {
    this.dispatchEvent(new CustomEvent('url-change', {
      bubbles: true,
      composed: true,
      detail: {
        url: e.detail.url,
        index: e.model.columnIndex,
      },
    }));
  }

  private onUrlRemove_(e: DomRepeatEvent<TableColumn>&
                       {model: {columnIndex: number}}) {
    this.dispatchEvent(new CustomEvent('url-remove', {
      bubbles: true,
      composed: true,
      detail: {
        index: e.model.columnIndex,
      },
    }));
  }

  private showRow_(title: string, rowIndex: number): boolean {
    return this.showDescription_(title, rowIndex) ||
        this.showSummary_(title, rowIndex);
  }

  private showDescription_(title: string, rowIndex: number): boolean {
    const rowDetails = this.columns.map(
        column => column.productDetails && column.productDetails[rowIndex]);

    return rowDetails.some(detail => {
      return detail && detail.title === title && detail.description &&
          detail.description.length > 0 && detail.description !== 'N/A';
    });
  }

  private showSummary_(title: string, rowIndex: number): boolean {
    const rowDetails = this.columns.map(
        column => column.productDetails && column.productDetails[rowIndex]);

    return rowDetails.some(detail => {
      return detail && detail.title === title && detail.summary &&
          detail.summary.length > 0 && detail.summary !== 'N/A';
    });
  }

  // This method provides a string that is intended to be used primarily in CSS.
  private getFavicon_(url: string): string {
    return getFaviconForPageURL(url, false, '', 32);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'product-specifications-table': TableElement;
  }
}

customElements.define(TableElement.is, TableElement);
