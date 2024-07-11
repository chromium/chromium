// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_auto_img/cr_auto_img.js';
import './product_selector.js';
import 'chrome://resources/cr_elements/cr_icons.css.js';
import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';
import 'chrome://resources/cr_elements/cr_hidden_style.css.js';

import {assert} from '//resources/js/assert.js';
import {getFaviconForPageURL} from '//resources/js/icon.js';
import type {DomRepeat} from '//resources/polymer/v3_0/polymer/lib/elements/dom-repeat.js';
import type {BrowserProxy} from 'chrome://resources/cr_components/commerce/browser_proxy.js';
import {BrowserProxyImpl} from 'chrome://resources/cr_components/commerce/browser_proxy.js';
import type {DomRepeatEvent} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import type {TableColumn} from './app.js';
import {DragAndDropManager} from './drag_and_drop_manager.js';
import {getTemplate} from './table.html.js';

export interface TableElement {
  $: {
    table: HTMLElement,
    columnRepeat: DomRepeat,
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
      draggingColumn: HTMLElement,
      hoveredColumnIndex_: Number,
    };
  }

  columns: TableColumn[];
  draggingColumn: HTMLElement|null = null;
  private hoveredColumnIndex_: number|null = null;

  private dragAndDropManager_: DragAndDropManager = new DragAndDropManager();
  private shoppingApi_: BrowserProxy = BrowserProxyImpl.getInstance();

  override connectedCallback(): void {
    super.connectedCallback();
    this.dragAndDropManager_.init(this);
  }

  override disconnectedCallback(): void {
    super.disconnectedCallback();
    this.dragAndDropManager_.destroy();
  }

  // Called by |dragAndDropManager|.
  moveColumnOnDrop(fromIndex: number, dropIndex: number) {
    const columns = this.columns;
    const [draggingColumn] = columns.splice(fromIndex, 1);
    assert(draggingColumn);
    columns.splice(dropIndex, 0, draggingColumn);
    this.notifySplices('columns', [
      {
        index: fromIndex,
        removed: [draggingColumn],
        addedCount: 0,
        object: columns,
        type: 'splice',
      },
      {
        index: dropIndex,
        removed: [],
        addedCount: 1,
        object: columns,
        type: 'splice',
      },
    ]);

    this.dispatchEvent(new Event('url-order-update'));
  }

  // |this.draggingColumn| is set by |dragAndDropManager|.
  private isDragging_(columnIndex: number): boolean {
    return !!this.draggingColumn &&
        columnIndex ===
        (this.$.columnRepeat.modelForElement(this.draggingColumn) as unknown as
         {
           columnIndex: number,
         }).columnIndex;
  }

  private isFirstColumn_(columnIndex: number): boolean|undefined {
    if (!this.draggingColumn) {
      return columnIndex === 0;
    }
    // While dragging, |dragAndDropManager| toggles this attribute, as the first
    // column shown in the table may have a non-zero `columnIndex`.
    return undefined;
  }

  // Determines the number of rows needed in the grid layout.
  // This is the sum of:
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
    this.hoveredColumnIndex_ = null;
  }

  private onShowOpenTabButton_(e: DomRepeatEvent<TableColumn>&
                               {model: {columnIndex: number}}) {
    this.hoveredColumnIndex_ = e.model.columnIndex;
  }

  private showOpenTabButton_(columnIndex: number): boolean {
    return !this.draggingColumn && this.hoveredColumnIndex_ !== null &&
        this.hoveredColumnIndex_ === columnIndex;
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
