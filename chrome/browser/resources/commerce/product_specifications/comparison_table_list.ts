// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_lazy_list/cr_lazy_list.js';
import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';
import 'chrome://resources/cr_elements/cr_toolbar/cr_toolbar_selection_overlay.js';
import 'chrome://resources/cr_elements/cr_action_menu/cr_action_menu.js';
import 'chrome://resources/cr_elements/cr_lazy_render/cr_lazy_render_lit.js';
import './images/icons.html.js';

import {ShowSetDisposition} from '//resources/cr_components/commerce/product_specifications.mojom-webui.js';
import type {ProductSpecificationsBrowserProxy} from '//resources/cr_components/commerce/product_specifications_browser_proxy.js';
import {ProductSpecificationsBrowserProxyImpl} from '//resources/cr_components/commerce/product_specifications_browser_proxy.js';
import type {ShoppingServiceBrowserProxy} from '//resources/cr_components/commerce/shopping_service_browser_proxy.js';
import {ShoppingServiceBrowserProxyImpl} from '//resources/cr_components/commerce/shopping_service_browser_proxy.js';
import {loadTimeData} from '//resources/js/load_time_data.js';
import type {Uuid} from '//resources/mojo/mojo/public/mojom/base/uuid.mojom-webui.js';
import type {Url} from '//resources/mojo/url/mojom/url.mojom-webui.js';
import type {CrActionMenuElement} from 'chrome://resources/cr_elements/cr_action_menu/cr_action_menu.js';
import type {CrIconButtonElement} from 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';
import type {CrLazyRenderLitElement} from 'chrome://resources/cr_elements/cr_lazy_render/cr_lazy_render_lit.js';
import type {CrToolbarSelectionOverlayElement} from 'chrome://resources/cr_elements/cr_toolbar/cr_toolbar_selection_overlay.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {getCss} from './comparison_table_list.css.js';
import {getHtml} from './comparison_table_list.html.js';
import type {ComparisonTableListItemCheckboxChangeEvent} from './comparison_table_list_item.js';

export interface ComparisonTableDetails {
  name: string;
  uuid: Uuid;
  urls: Url[];
}

export interface ComparisonTableListElement {
  $: {
    delete: CrIconButtonElement,
    edit: CrIconButtonElement,
    menu: CrLazyRenderLitElement<CrActionMenuElement>,
    more: CrIconButtonElement,
    toolbar: CrToolbarSelectionOverlayElement,
  };
}

export class ComparisonTableListElement extends CrLitElement {
  static get is() {
    return 'comparison-table-list';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      tables: {type: Array},
      isEditing_: {type: Boolean},
      numSelected_: {type: Number},
      deletePending_: {type: Boolean},
    };
  }

  tables: ComparisonTableDetails[] = [];

  protected isEditing_: boolean = false;
  protected numSelected_: number = 0;
  protected deletePending_: boolean = false;
  private selectedUuids_: Set<Uuid> = new Set();
  private shoppingApi_: ShoppingServiceBrowserProxy =
      ShoppingServiceBrowserProxyImpl.getInstance();
  private productSpecificationsProxy_: ProductSpecificationsBrowserProxy =
      ProductSpecificationsBrowserProxyImpl.getInstance();

  protected getSelectionLabel_(numSelected: number): string {
    return loadTimeData.getStringF('numSelected', numSelected);
  }

  protected getOpenAllString_(numSelected: number): string {
    return loadTimeData.getStringF('menuOpenAll', numSelected);
  }

  protected getOpenAllInNewWindowString_(numSelected: number): string {
    return loadTimeData.getStringF('menuOpenAllInNewWindow', numSelected);
  }

  protected onEditClick_() {
    if (!this.isEditing_) {
      this.isEditing_ = true;
      return;
    }

    this.stopEditing_();
  }

  protected onClearClick_() {
    this.stopEditing_();
  }

  protected async onDeleteClick_() {
    // Close the menu only if it has been rendered.
    const menu = this.$.menu.getIfExists();
    if (menu) {
      menu.close();
    }

    this.deleteSelectedTables_();
    this.stopEditing_();
  }

  protected async onShowContextMenuClick_() {
    this.$.menu.get().showAt(this.$.more);
  }

  protected async onOpenAllClick_() {
    this.productSpecificationsProxy_.showProductSpecificationsSetsForUuids(
        Array.from(this.selectedUuids_), ShowSetDisposition.kInNewTabs);
    this.$.menu.get().close();

    this.fire('open-all-finished-for-testing');
  }

  protected async onOpenAllInNewWindowClick_() {
    this.productSpecificationsProxy_.showProductSpecificationsSetsForUuids(
        Array.from(this.selectedUuids_), ShowSetDisposition.kInNewWindow);
    this.$.menu.get().close();

    this.fire('open-all-in-new-window-finished-for-testing');
  }

  protected onCheckboxChange_(event:
                                  ComparisonTableListItemCheckboxChangeEvent) {
    if (event.detail.checked) {
      this.selectedUuids_.add(event.detail.uuid);
    } else {
      this.selectedUuids_.delete(event.detail.uuid);
    }

    this.numSelected_ = this.selectedUuids_.size;
  }

  protected stopEditing_() {
    this.isEditing_ = false;
    this.selectedUuids_.clear();
    this.numSelected_ = 0;
  }

  private async deleteSelectedTables_() {
    // Disable the delete button while sets are being deleted.
    this.deletePending_ = true;
    const promises: void[] = [];
    this.selectedUuids_.forEach(uuid => {
      promises.push(this.shoppingApi_.deleteProductSpecificationsSet(uuid));
    });
    await Promise.all(promises);
    this.deletePending_ = false;

    this.fire('delete-finished-for-testing');
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'comparison-table-list': ComparisonTableListElement;
  }
}

customElements.define(
    ComparisonTableListElement.is, ComparisonTableListElement);
