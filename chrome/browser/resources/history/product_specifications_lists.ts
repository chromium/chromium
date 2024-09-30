// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
import './product_specifications_item.js';
import 'chrome://resources/cr_elements/cr_lazy_render/cr_lazy_render.js';
import 'chrome://resources/cr_elements/cr_shared_style.css.js';
import 'chrome://resources/cr_elements/cr_shared_vars.css.js';
import './shared_style.css.js';
import 'chrome://resources/cr_elements/cr_button/cr_button.js';

import {BrowserProxyImpl} from '//resources/cr_components/commerce/browser_proxy.js';
import type {BrowserProxy} from '//resources/cr_components/commerce/browser_proxy.js';
import type {DomRepeat} from '//resources/polymer/v3_0/polymer/lib/elements/dom-repeat.js';
import type {PageCallbackRouter, ProductSpecificationsFeatureState, ProductSpecificationsSet} from 'chrome://resources/cr_components/commerce/shopping_service.mojom-webui.js';
import type {CrActionMenuElement} from 'chrome://resources/cr_elements/cr_action_menu/cr_action_menu.js';
import type {CrDialogElement} from 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import type {CrLazyRenderElement} from 'chrome://resources/cr_elements/cr_lazy_render/cr_lazy_render.js';
import {assert} from 'chrome://resources/js/assert.js';
import {FocusGrid} from 'chrome://resources/js/focus_grid.js';
import {OpenWindowProxyImpl} from 'chrome://resources/js/open_window_proxy.js';
import type {Uuid} from 'chrome://resources/mojo/mojo/public/mojom/base/uuid.mojom-webui.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import type {ItemCheckboxSelectEvent, ItemMenuOpenEvent, ProductSpecificationsItemElement} from './product_specifications_item.js';
import {getTemplate} from './product_specifications_lists.html.js';

export interface ProductSpecificationsListsElement {
  $: {
    'sharedMenu': CrLazyRenderElement<CrActionMenuElement>,
    'deleteItemDialog': CrLazyRenderElement<CrDialogElement>,
    'displayedItemsList': DomRepeat,
  };
}

declare global {
  interface HTMLElementEventMap {
    'product-spec-item-select': ItemCheckboxSelectEvent;
    'item-menu-open': ItemMenuOpenEvent;
  }
}

function areStatesEqual(
    firstState: ProductSpecificationsFeatureState|null,
    secondState: ProductSpecificationsFeatureState|null) {
  if (firstState === null) {
    return secondState === null;
  }
  if (secondState === null) {
    return false;
  }
  return firstState.isSyncingTabCompare === secondState.isSyncingTabCompare &&
      firstState.canLoadFullPageUi === secondState.canLoadFullPageUi &&
      firstState.canManageSets === secondState.canManageSets &&
      firstState.canFetchData === secondState.canFetchData &&
      firstState.isAllowedForEnterprise ===
      secondState.isAllowedForEnterprise &&
      firstState.isSignedIn === secondState.isSignedIn;
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
      searchTerm: String,
      pendingDelete_: {
        notify: true,
        type: Boolean,
      },
      lastSelectedIndex_: Number,
      allItems_: Array,
      displayedItems_: {
        type: Array,
        computed: 'computeDisplayedItems_(allItems_.*, searchTerm)',
      },
      uuidOfOpenMenu_: Object,
      productSpecificationsFeatureState_: Object,
    };
  }

  selectedItems: Set<string> = new Set();
  searchTerm: string = '';
  private pendingDelete_: boolean = false;
  private lastSelectedIndex_: number|undefined = undefined;
  private shoppingApi_: BrowserProxy = BrowserProxyImpl.getInstance();
  private allItems_: ProductSpecificationsSet[] = [];
  private displayedItems_: ProductSpecificationsSet[] = [];

  private focusGrid_: FocusGrid|null = null;
  private uuidOfOpenMenu_: Uuid|null = null;
  private callbackRouter_: PageCallbackRouter;
  private listenerIds_: number[] = [];
  private productSpecificationsFeatureState_: ProductSpecificationsFeatureState|
      null = null;
  private boundFocusCallback_: () => void;

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

    // TODO(358131415): use listeners to update. Temporary workaround uses
    // window focus to update the feature state, to check signin.
    this.boundFocusCallback_ = this.getFeatureStateAndLoadSets_.bind(this);
    window.addEventListener('focus', this.boundFocusCallback_);
    this.getFeatureStateAndLoadSets_();
  }

  private async getFeatureStateAndLoadSets_() {
    const previousState = this.productSpecificationsFeatureState_;
    const {state} =
        await this.shoppingApi_.getProductSpecificationsFeatureState();
    if (!state || areStatesEqual(previousState, state)) {
      return;
    }
    // If the state changes, reload all sets.
    const {sets} = await this.shoppingApi_.getAllProductSpecificationsSets();
    if (!sets) {
      return;
    }
    this.allItems_ = sets;
    this.productSpecificationsFeatureState_ = state;
  }

  override disconnectedCallback() {
    super.disconnectedCallback();
    this.listenerIds_.forEach(id => this.callbackRouter_.removeListener(id));
    if (this.focusGrid_) {
      this.focusGrid_!.destroy();
    }
    if (this.boundFocusCallback_) {
      window.removeEventListener('focus', this.boundFocusCallback_);
    }
  }

  private hideSyncScreen_() {
    // The sync screen shows up if the user is not syncing.
    if (this.productSpecificationsFeatureState_) {
      return this.productSpecificationsFeatureState_.isSyncingTabCompare;
    }
    return true;
  }

  private hideErrorMessage_() {
    return !this.hideSyncScreen_() || !this.hideHistoryList_();
  }

  private hideHistoryList_() {
    if (this.productSpecificationsFeatureState_) {
      // If not sync'd, not able to manage, or not in experiment,
      // then we can't even show the history list.
      return !this.productSpecificationsFeatureState_.isSyncingTabCompare ||
          !this.productSpecificationsFeatureState_.canManageSets ||
          !this.productSpecificationsFeatureState_.canLoadFullPageUi;
    }
    return true;
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

  private hasResults_(): boolean {
    return this.displayedItems_.length > 0;
  }

  private showSyncSetupFlow_() {
    assert(this.productSpecificationsFeatureState_);
    assert(!this.productSpecificationsFeatureState_.isSyncingTabCompare);

    // If signed in at the account level, then user needs to turn on sync
    // from settings.
    if (this.productSpecificationsFeatureState_.isSignedIn) {
      OpenWindowProxyImpl.getInstance().openUrl(
          'chrome://settings/syncSetup/advanced');
      return;
    }
    this.shoppingApi_.showSyncSetupFlow();
  }

  getSelectedItemCount() {
    return this.selectedItems.size;
  }

  private onItemSelected_(e: ItemCheckboxSelectEvent) {
    const index = e.detail.index;
    const itemElements =
        this.shadowRoot!.querySelectorAll('product-specifications-item');
    const toSelect =
        !this.selectedItems.has(itemElements[index].item.uuid.value);

    if (this.lastSelectedIndex_ === undefined || !e.detail.shiftKey) {
      this.changeSelection_(index, toSelect, itemElements);
      this.lastSelectedIndex_ = index;
      return;
    }

    // Handle shift selection. Change the selection state of all items between
    // |index| and |lastSelected| to the selection state of |item|.
    for (let i = Math.min(index, this.lastSelectedIndex_);
         i <= Math.min(
                  Math.max(index, this.lastSelectedIndex_),
                  this.displayedItems_.length - 1);
         i++) {
      this.changeSelection_(i, toSelect, itemElements);
    }
    this.lastSelectedIndex_ = index;
  }

  private changeSelection_(
      index: number, toSelect: boolean,
      itemElements: NodeListOf<ProductSpecificationsItemElement>) {
    if (toSelect) {
      this.selectedItems.add(this.displayedItems_[index].uuid.value);
      itemElements[index].checked = true;
    } else {
      this.selectedItems.delete(this.displayedItems_[index].uuid.value);
      itemElements[index].checked = false;
    }
  }

  /**
   * Deselect each item in |selectedItems|.
   */
  unselectAllItems() {
    this.selectedItems.clear();
    const items =
        this.shadowRoot!.querySelectorAll('product-specifications-item');
    for (const el of items) {
      el.checked = false;
    }
  }

  private onOpenMenu_(e: ItemMenuOpenEvent) {
    this.$.sharedMenu.get().showAt(e.detail.target);
    this.uuidOfOpenMenu_ = e.detail.uuid;
  }

  /**
   * Helper method to delete multiple items.
   */
  private deleteItems_(items: Set<string>): Promise<void[]> {
    // pendingDelete_ disables the delete button while a delete call
    // is being made. It waits for the call to the proxy.
    assert(!this.pendingDelete_);
    const promises: void[] = [];
    for (const uuid of items) {
      promises.push(
          this.shoppingApi_.deleteProductSpecificationsSet({value: uuid}));
    }
    this.pendingDelete_ = true;
    return Promise.all(promises);
  }

  private resetAfterDelete_() {
    this.pendingDelete_ = false;
    this.fire_('unselect-all');
  }

  private fire_(eventName: string, detail?: any) {
    this.dispatchEvent(
        new CustomEvent(eventName, {bubbles: true, composed: true, detail}));
  }

  private onRemoveItemClick_() {
    if (this.uuidOfOpenMenu_ !== null) {
      this.deleteItems_(new Set([this.uuidOfOpenMenu_.value]));
      this.closeMenu_();
      this.pendingDelete_ = false;
      this.fire_('unselect-all');
    }
  }

  /**
   * Closes the overflow menu.
   */
  private closeMenu_() {
    const menu = this.$.sharedMenu.getIfExists();
    if (menu && menu.open) {
      this.uuidOfOpenMenu_ = null;
      menu.close();
    }
  }

  getFocusGridForTesting() {
    return this.focusGrid_;
  }

  /**
   * Opens up a delete dialog from toolbar.
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
    const deleteItemDialog = this.$.deleteItemDialog.getIfExists();
    assert(deleteItemDialog);
    this.deleteItems_(this.selectedItems);
    deleteItemDialog.close();
    this.pendingDelete_ = false;
    this.fire_('unselect-all');
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

  private computeDisplayedItems_() {
    const searchText = this.searchTerm.toLowerCase().trim();
    return this.allItems_.filter((item) => {
      return item.name.trim().toLowerCase().includes(searchText);
    });
  }

  selectOrUnselectAll() {
    if (this.displayedItems_.length === this.getSelectedItemCount()) {
      this.unselectAllItems();
    } else {
      this.selectAllItems();
    }
  }

  selectAllItems() {
    const items =
        this.shadowRoot!.querySelectorAll('product-specifications-item');
    items.forEach((item) => {
      item.checked = true;
      this.selectedItems.add(item.item.uuid.value);
    });
    assert(this.selectedItems.size === this.displayedItems_.length);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'product-specifications-lists': ProductSpecificationsListsElement;
  }
}

customElements.define(
    ProductSpecificationsListsElement.is, ProductSpecificationsListsElement);
