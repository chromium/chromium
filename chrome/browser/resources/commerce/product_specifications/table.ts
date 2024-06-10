// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_auto_img/cr_auto_img.js';
import './product_selector.js';
import 'chrome://resources/cr_elements/cr_icons.css.js';
import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';
import 'chrome://resources/cr_elements/cr_hidden_style.css.js';

import type {BrowserProxy} from 'chrome://resources/cr_components/commerce/browser_proxy.js';
import {BrowserProxyImpl} from 'chrome://resources/cr_components/commerce/browser_proxy.js';
import type {DomRepeatEvent} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './table.html.js';
import type {UrlListEntry} from './utils.js';

/** Describes a row in a ProductSpecs table. */
export interface TableRow {
  title: string;
  descriptions: string[];
  summaries: string[];
}
/** Describes a column in a ProductSpecs table. */
export interface TableColumn {
  selectedItem: UrlListEntry;
}

/** Element for rendering a ProductSpecs table. */
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
      rows: Array,
      hoveredColumnIndex_: {type: Number, value: -1},
    };
  }

  columns: TableColumn[];
  rows: TableRow[];
  private hoveredColumnIndex_: number;

  private shoppingApi_: BrowserProxy = BrowserProxyImpl.getInstance();

  private getUrls_() {
    return this.columns.map(column => column.selectedItem.url);
  }

  private onHideOpenTabButton_() {
    this.hoveredColumnIndex_ = -1;
  }

  private onShowOpenTabButton_(e: DomRepeatEvent<TableColumn|TableRow>) {
    this.hoveredColumnIndex_ = e.model.index;
  }

  private shouldShowOpenTabButton_(columnIndex: number): boolean {
    return columnIndex === this.hoveredColumnIndex_;
  }

  private onOpenTabButtonClick_(e: DomRepeatEvent<TableColumn, CustomEvent>) {
    this.shoppingApi_.switchToOrOpenTab(
        {url: this.columns[e.model.index].selectedItem.url});
  }

  private onSelectedUrlChange_(
      e: DomRepeatEvent<TableColumn, CustomEvent<{url: string}>>) {
    this.dispatchEvent(new CustomEvent('url-change', {
      bubbles: true,
      composed: true,
      detail: {
        url: e.detail.url,
        index: e.model.index,
      },
    }));
  }

  private onUrlRemove_(e: DomRepeatEvent<TableColumn>) {
    this.dispatchEvent(new CustomEvent('url-remove', {
      bubbles: true,
      composed: true,
      detail: {
        index: e.model.index,
      },
    }));
  }

  private shouldShowRow_(row: TableRow): boolean {
    return this.shouldShowDescriptionRow_(row) ||
        this.shouldShowSummaryRow_(row);
  }

  private shouldShowDescriptionRow_(row: TableRow): boolean {
    return row.descriptions.some(
        (description) => (description.length > 0 && description !== 'N/A'));
  }

  private shouldShowSummaryRow_(row: TableRow): boolean {
    return row.summaries.some(
        (summary) => (summary.length > 0 && summary !== 'N/A'));
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'product-specifications-table': TableElement;
  }
}

customElements.define(TableElement.is, TableElement);
