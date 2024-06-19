// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
import './product_specifications_item.js';
import 'chrome://resources/cr_elements/cr_shared_style.css.js';
import './shared_style.css.js';

import type {BrowserProxy} from '//resources/cr_components/commerce/browser_proxy.js';
import {BrowserProxyImpl} from '//resources/cr_components/commerce/browser_proxy.js';
import type {ProductSpecificationsSet} from 'chrome://resources/cr_components/commerce/shopping_service.mojom-webui.js';
import type {CrActionMenuElement} from 'chrome://resources/cr_elements/cr_action_menu/cr_action_menu.js';
import type {CrLazyRenderElement} from 'chrome://resources/cr_elements/cr_lazy_render/cr_lazy_render.js';
import {assert} from 'chrome://resources/js/assert.js';
import {FocusGrid} from 'chrome://resources/js/focus_grid.js';
import type {Uuid} from 'chrome://resources/mojo/mojo/public/mojom/base/uuid.mojom-webui.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import type {ItemCheckboxSelectEvent, ItemMenuOpenEvent} from './product_specifications_item.js';
import {getTemplate} from './product_specifications_lists.html.js';

export interface ProductSpecificationsListsElement {
  $: {
    'sharedMenu': CrLazyRenderElement<CrActionMenuElement>,
  };
}

declare global {
  interface HTMLElementEventMap {
    'item-checkbox-select': ItemCheckboxSelectEvent;
    'item-menu-open': ItemMenuOpenEvent;
  }
}
export class ProductSpecificationsListsElement extends PolymerElement {
  static get is() {
    return 'product-specifications-lists';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      selectedItems: Object,
      allItems_: Array,
      uuidOfOpenMenu_: Object,
    };
  }

  selectedItems: Set<string> = new Set();

  private shoppingApi_: BrowserProxy = BrowserProxyImpl.getInstance();
  private allItems_: ProductSpecificationsSet[] = [];
  private focusGrid_: FocusGrid|null = null;
  private uuidOfOpenMenu_: Uuid|null = null;

  override async connectedCallback() {
    super.connectedCallback();
    this.focusGrid_ = new FocusGrid();
    const {sets} = await this.shoppingApi_.getAllProductSpecificationsSets();
    if (!sets) {
      return;
    }
    this.allItems_ = sets;
  }

  override disconnectedCallback() {
    super.disconnectedCallback();
    if (this.focusGrid_) {
      this.focusGrid_!.destroy();
    }
  }

  private updateFocusGrid_() {
    assert(this.focusGrid_);
    const items =
        this.shadowRoot!.querySelectorAll('product-specifications-item');
    for (const el of items) {
      const row = el.createFocusRow();
      this.focusGrid_!.addRow(row);
    }
    this.focusGrid_!.ensureRowActive(0);
  }

  private onItemSelected_(e: ItemCheckboxSelectEvent) {
    if (!this.selectedItems.has(e.detail.uuid)) {
      this.selectedItems.add(e.detail.uuid);
    } else {
      this.selectedItems.delete(e.detail.uuid);
    }
  }

  private onOpenMenu_(e: ItemMenuOpenEvent) {
    this.$.sharedMenu.get().showAt(e.detail.target);
    this.uuidOfOpenMenu_ = e.detail.uuid;
  }

  // TODO: b/335670350 - add checkbox support for deleting multiple items.
  private deleteItems_(items: string[]): Promise<void[]> {
    const promises: void[] = [];
    for (const uuid of items) {
      promises.push(
          this.shoppingApi_.deleteProductSpecificationsSet({value: uuid}));
    }
    return Promise.all(promises);
  }

  private onRemoveItemClick_() {
    if (this.uuidOfOpenMenu_ !== null) {
      this.deleteItems_([this.uuidOfOpenMenu_.value]);
    }
  }

  getFocusGridForTesting() {
    return this.focusGrid_;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'product-specifications-lists': ProductSpecificationsListsElement;
  }
}

customElements.define(
    ProductSpecificationsListsElement.is, ProductSpecificationsListsElement);
