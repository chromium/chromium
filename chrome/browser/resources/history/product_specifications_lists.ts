// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
import './product_specifications_item.js';
import 'chrome://resources/cr_elements/cr_lazy_render/cr_lazy_render.js';
import 'chrome://resources/cr_elements/cr_shared_style.css.js';
import './shared_style.css.js';

import type {BrowserProxy} from '//resources/cr_components/commerce/browser_proxy.js';
import {BrowserProxyImpl} from '//resources/cr_components/commerce/browser_proxy.js';
import type {DomRepeat} from '//resources/polymer/v3_0/polymer/lib/elements/dom-repeat.js';
import type {PageCallbackRouter, ProductSpecificationsSet} from 'chrome://resources/cr_components/commerce/shopping_service.mojom-webui.js';
import type {CrActionMenuElement} from 'chrome://resources/cr_elements/cr_action_menu/cr_action_menu.js';
import type {CrDialogElement} from 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
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
    'deleteItemDialog': CrLazyRenderElement<CrDialogElement>,
    'allItemsList': DomRepeat,
  };
}

declare global {
  interface HTMLElementEventMap {
    'product-spec-item-select': ItemCheckboxSelectEvent;
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
  private callbackRouter_: PageCallbackRouter;
  private listenerIds_: number[] = [];

  constructor() {
    super();
    this.callbackRouter_ = this.shoppingApi_.getCallbackRouter();
  }

  override async connectedCallback() {
    super.connectedCallback();
    this.focusGrid_ = new FocusGrid();

    this.listenerIds_.push(
        this.callbackRouter_.onProductSpecificationsSetAdded.addListener(
            (set: ProductSpecificationsSet) => this.onSetAdded_(set)),
        this.callbackRouter_.onProductSpecificationsSetUpdated.addListener(
            (set: ProductSpecificationsSet) => this.onSetUpdated_(set)),
        this.callbackRouter_.onProductSpecificationsSetRemoved.addListener(
            (uuid: Uuid) => this.onSetRemoved_(uuid)),
    );

    const {sets} = await this.shoppingApi_.getAllProductSpecificationsSets();
    if (!sets) {
      return;
    }
    this.allItems_ = sets;
  }

  override disconnectedCallback() {
    super.disconnectedCallback();
    this.listenerIds_.forEach(id => this.callbackRouter_.removeListener(id));
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

  getSelectedItemCount() {
    return this.selectedItems.size;
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

  private deleteItems_(items: Set<string>): Promise<void[]> {
    const promises: void[] = [];
    for (const uuid of items) {
      promises.push(
          this.shoppingApi_.deleteProductSpecificationsSet({value: uuid}));
    }
    return Promise.all(promises);
  }

  private onRemoveItemClick_() {
    if (this.uuidOfOpenMenu_ !== null) {
      this.deleteItems_(new Set([this.uuidOfOpenMenu_.value]));
      // TODO: b/335670350 - remove items from the UI.
    }
  }

  getFocusGridForTesting() {
    return this.focusGrid_;
  }

  /**
   * Deletes selected items via the toolbar, which opens up a dialog.
   */
  deleteSelectedWithPrompt() {
    // TODO: b/335670350 - add check for deleting history
    this.$.deleteItemDialog.get().showModal();
    const button =
        this.shadowRoot!.querySelector<HTMLElement>('.action-button');
    assert(button);
    button.focus();
  }

  private onDialogConfirmClick_() {
    this.deleteItems_(this.selectedItems);

    // TODO: b/335670350 - set deleting state in progress.
    const deleteItemDialog = this.$.deleteItemDialog.getIfExists();
    assert(deleteItemDialog);
    deleteItemDialog.close();
  }

  private onDialogCancelClick_() {
    const deleteItemDialog = this.$.deleteItemDialog.getIfExists();
    assert(deleteItemDialog);
    deleteItemDialog.close();
  }

  /**
   * Finds index of element with given uuid.
   * Returns -1 if not found.
   */
  private findIndexForSet_(uuid: Uuid): number {
    return this.allItems_.findIndex(existingSet => {
      return existingSet.uuid.value === uuid.value;
    });
  }

  private onSetUpdated_(set: ProductSpecificationsSet) {
    const setIndex = this.findIndexForSet_(set.uuid);
    if (setIndex < 0) {
      return;
    }
    this.splice('allItems_', setIndex, 1, set);
  }

  private onSetAdded_(set: ProductSpecificationsSet) {
    this.push('allItems_', set);
  }

  private onSetRemoved_(id: Uuid) {
    const setIndex = this.findIndexForSet_(id);
    if (setIndex < 0) {
      return;
    }
    this.splice('allItems_', setIndex, 1);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'product-specifications-lists': ProductSpecificationsListsElement;
  }
}

customElements.define(
    ProductSpecificationsListsElement.is, ProductSpecificationsListsElement);
