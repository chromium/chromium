// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_auto_img/cr_auto_img.js';
import './description_section.js';
import './product_selector.js';
import './buying_options_section.js';
import './empty_section.js';
import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_icons.css.js';
import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';
import 'chrome://resources/cr_elements/cr_hidden_style.css.js';
import 'chrome://resources/cr_elements/cr_tooltip/cr_tooltip.js';
import './shared_vars.css.js';

import {assert} from '//resources/js/assert.js';
import {getFaviconForPageURL} from '//resources/js/icon.js';
import type {ShoppingServiceBrowserProxy} from 'chrome://resources/cr_components/commerce/shopping_service_browser_proxy.js';
import {ShoppingServiceBrowserProxyImpl} from 'chrome://resources/cr_components/commerce/shopping_service_browser_proxy.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';
import type {PropertyValues} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import type {BuyingOptions} from './buying_options_section.js';
import type {ProductDescription} from './description_section.js';
import {DragAndDropManager} from './drag_and_drop_manager.js';
import type {SectionType} from './product_selection_menu.js';
import {getCss} from './table.css.js';
import {getHtml} from './table.html.js';
import type {UrlListEntry} from './utils.js';
import {WindowProxy} from './window_proxy.js';

export type ContentOrNull = string|ProductDescription|BuyingOptions|null;

export interface ProductDetail {
  title: string|null;
  content: ContentOrNull;
}

export interface TableColumn {
  selectedItem: UrlListEntry;
  productDetails: ProductDetail[]|null;
}

export interface TableElement {
  $: {
    table: HTMLElement,
  };
}

export class TableElement extends CrLitElement {
  static get is() {
    return 'product-specifications-table';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      columns: {type: Array},
      draggingColumn: {type: Object},
      hoveredColumnIndex_: {type: Number},
    };
  }

  accessor columns: TableColumn[] = [];
  accessor draggingColumn: HTMLElement|null = null;
  private accessor hoveredColumnIndex_: number|null = null;

  private dragAndDropManager_: DragAndDropManager;
  private shoppingApi_: ShoppingServiceBrowserProxy =
      ShoppingServiceBrowserProxyImpl.getInstance();

  constructor() {
    super();
    this.dragAndDropManager_ = new DragAndDropManager(this);
  }

  getDragAndDropManager(): DragAndDropManager {
    return this.dragAndDropManager_;
  }

  override updated(changedProperties: PropertyValues<this>) {
    super.updated(changedProperties);

    if (changedProperties.has('columns')) {
      this.style.setProperty('--num-columns', String(this.columns.length));
    }

    this.dragAndDropManager_.tablePropertiesUpdated();
  }

  override connectedCallback(): void {
    super.connectedCallback();
    // Prevent cursor from switching to not-allowed on Windows during drag and
    // drop.
    this.$.table.addEventListener(
        'dragenter', (e: DragEvent) => e.preventDefault());
    this.$.table.addEventListener(
        'dragleave', (e: DragEvent) => e.preventDefault());
  }

  override disconnectedCallback(): void {
    super.disconnectedCallback();
    this.dragAndDropManager_.destroy();
  }

  // Called by |dragAndDropManager|.
  async moveColumnOnDrop(fromIndex: number, dropIndex: number) {
    const columns = this.columns;
    const [draggingColumn] = columns.splice(fromIndex, 1);
    assert(draggingColumn);

    columns.splice(dropIndex, 0, draggingColumn);
    this.requestUpdate();
    await this.updateComplete;

    this.fire('url-order-update');
  }

  closeAllProductSelectionMenus() {
    const productSelectors =
        this.shadowRoot.querySelectorAll('product-selector');
    productSelectors.forEach(productSelector => productSelector.closeMenu());
  }

  // |this.draggingColumn| is set by |dragAndDropManager|.
  protected isDragging_(columnIndex: number): boolean {
    return !!this.draggingColumn &&
        columnIndex === Number(this.draggingColumn.dataset['index']);
  }

  protected isFirstColumn_(columnIndex: number): boolean|undefined {
    if (!this.draggingColumn) {
      return columnIndex === 0;
    }
    // While dragging, |dragAndDropManager| toggles this attribute, as the first
    // column shown in the table may have a non-zero `columnIndex`.
    return undefined;
  }

  protected getScrollSnapAlign_(): string {
    return !this.draggingColumn ? 'start' : 'none';
  }

  // Determines the number of rows needed in the grid layout.
  // This is the sum of:
  //   - 1 row for the product selector.
  //   - 1 row for the image container.
  //   - Number of product details.
  protected getRowCount_(numProductDetails: number): number {
    return 2 + numProductDetails;
  }

  protected getUrls_() {
    return this.columns.map(column => column.selectedItem.url);
  }

  protected onHideOpenTabButton_() {
    this.hoveredColumnIndex_ = null;
  }

  protected onShowOpenTabButton_(e: Event) {
    const currentTarget = e.currentTarget as HTMLElement;
    this.hoveredColumnIndex_ = Number(currentTarget.dataset['index']);
  }

  protected showOpenTabButton_(columnIndex: number): boolean {
    return !this.draggingColumn && this.hoveredColumnIndex_ !== null &&
        this.hoveredColumnIndex_ === columnIndex;
  }

  protected onOpenTabButtonClick_(e: Event) {
    if (!WindowProxy.getInstance().onLine) {
      this.dispatchEvent(new Event('unavailable-action-attempted'));
      return;
    }
    const currentTarget = e.currentTarget as HTMLElement;
    const columnIndex = Number(currentTarget.dataset['index']);
    this.shoppingApi_.switchToOrOpenTab(
        {url: this.columns[columnIndex]?.selectedItem.url || ''});
    chrome.metricsPrivate.recordUserAction(
        'Commerce.Compare.ReopenedProductPage');
  }

  protected onSelectedUrlChange_(
      e: CustomEvent<{url: string, urlSection: SectionType}>) {
    const currentTarget = e.currentTarget as HTMLElement;
    this.dispatchEvent(new CustomEvent('url-change', {
      bubbles: true,
      composed: true,
      detail: {
        url: e.detail.url,
        urlSection: e.detail.urlSection,
        index: Number(currentTarget.dataset['index']),
      },
    }));
  }

  protected onUrlRemove_(e: Event) {
    const currentTarget = e.currentTarget as HTMLElement;
    this.dispatchEvent(new CustomEvent('url-remove', {
      bubbles: true,
      composed: true,
      detail: {
        index: Number(currentTarget.dataset['index']),
      },
    }));
  }

  protected showRow_(title: string, rowIndex: number): boolean {
    return this.rowHasNonEmptyAttributes_(title, rowIndex) ||
        this.rowHasNonEmptySummary_(title, rowIndex) ||
        this.rowHasText_(title, rowIndex) ||
        this.rowHasBuyingOptions_(rowIndex);
  }

  private rowHasText_(title: string, rowIndex: number): boolean {
    const rowDetails = this.columns.map(
        column => column.productDetails && column.productDetails[rowIndex]);

    return rowDetails.some(
        detail => detail && detail.title === title &&
            this.contentIsString_(detail.content));
  }

  private rowHasNonEmptyAttributes_(title: string, rowIndex: number): boolean {
    const rowDetails = this.columns.map(
        column => column.productDetails && column.productDetails[rowIndex]);

    return rowDetails.some(detail => {
      if (!detail || detail.title !== title ||
          !this.contentIsProductDescription_(detail.content)) {
        return false;
      }
      return detail.content && detail.content.attributes.some(attr => {
        return attr.value.length > 0 && attr.value !== 'N/A';
      });
    });
  }

  private rowHasNonEmptySummary_(title: string, rowIndex: number): boolean {
    const rowDetails = this.columns.map(
        column => column.productDetails && column.productDetails[rowIndex]);

    return rowDetails.some(detail => {
      return detail && detail.title === title && detail.content &&
          this.contentIsProductDescription_(detail.content) &&
          detail.content.summary.length > 0 &&
          detail.content.summary.some(
              (summaryObj) => summaryObj.text !== 'N/A');
    });
  }

  private rowHasBuyingOptions_(rowIndex: number): boolean {
    const rowDetails = this.columns.map(
        column => column.productDetails && column.productDetails[rowIndex]);

    return rowDetails.some(
        detail => detail && this.contentIsBuyingOptions_(detail.content));
  }

  protected filterProductDescription_(
      productDesc: ProductDescription, title: string,
      rowIndex: number): ProductDescription {
    // Hide product descriptions when all attributes/summaries in this row are
    // missing or marked "N/A".
    return {
      attributes: this.rowHasNonEmptyAttributes_(title, rowIndex) ?
          productDesc.attributes :
          [],
      summary: this.rowHasNonEmptySummary_(title, rowIndex) ?
          productDesc.summary :
          [],
    };
  }

  protected contentIsString_(content: ContentOrNull): content is string {
    return (content && typeof content === 'string') as boolean;
  }

  protected contentIsProductDescription_(content: ContentOrNull):
      content is ProductDescription {
    if (content) {
      const description = content as ProductDescription;
      return Array.isArray(description.attributes) &&
          Array.isArray(description.summary);
    }
    return false;
  }

  protected contentIsBuyingOptions_(content: ContentOrNull):
      content is BuyingOptions {
    if (content) {
      const buyingOptions = content as BuyingOptions;
      return typeof buyingOptions.price === 'string' &&
          typeof buyingOptions.jackpotUrl === 'string' &&
          buyingOptions.price.length > 0;
    }
    return false;
  }

  // This method provides a string that is intended to be used primarily in CSS.
  protected getFavicon_(url: string): string {
    return getFaviconForPageURL(url, false, '', 32);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'product-specifications-table': TableElement;
  }
}

customElements.define(TableElement.is, TableElement);
