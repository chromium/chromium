// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_lazy_list/cr_lazy_list.js';
import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';
import 'chrome://resources/cr_elements/cr_toolbar/cr_toolbar_selection_overlay.js';
import 'chrome://resources/cr_elements/cr_action_menu/cr_action_menu.js';
import 'chrome://resources/cr_elements/cr_lazy_render/cr_lazy_render_lit.js';
import 'chrome://resources/cr_elements/cr_toast/cr_toast.js';
import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import './images/icons.html.js';

import {ShowSetDisposition} from '//resources/cr_components/commerce/product_specifications.mojom-webui.js';
import type {ProductSpecificationsBrowserProxy} from '//resources/cr_components/commerce/product_specifications_browser_proxy.js';
import {ProductSpecificationsBrowserProxyImpl} from '//resources/cr_components/commerce/product_specifications_browser_proxy.js';
import type {ShoppingServiceBrowserProxy} from '//resources/cr_components/commerce/shopping_service_browser_proxy.js';
import {ShoppingServiceBrowserProxyImpl} from '//resources/cr_components/commerce/shopping_service_browser_proxy.js';
import {loadTimeData} from '//resources/js/load_time_data.js';
import type {PluralStringProxy} from '//resources/js/plural_string_proxy.js';
import {PluralStringProxyImpl} from '//resources/js/plural_string_proxy.js';
import type {Uuid} from '//resources/mojo/mojo/public/mojom/base/uuid.mojom-webui.js';
import type {Url} from '//resources/mojo/url/mojom/url.mojom-webui.js';
import type {CrActionMenuElement} from 'chrome://resources/cr_elements/cr_action_menu/cr_action_menu.js';
import type {CrIconButtonElement} from 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';
import type {CrLazyRenderLitElement} from 'chrome://resources/cr_elements/cr_lazy_render/cr_lazy_render_lit.js';
import type {CrToastElement} from 'chrome://resources/cr_elements/cr_toast/cr_toast.js';
import type {CrToolbarSelectionOverlayElement} from 'chrome://resources/cr_elements/cr_toolbar/cr_toolbar_selection_overlay.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {getCss} from './comparison_table_list.css.js';
import {getHtml} from './comparison_table_list.html.js';
import type {ComparisonTableListItemCheckboxChangeEvent, ComparisonTableListItemDeleteEvent} from './comparison_table_list_item.js';

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
    toast: CrLazyRenderLitElement<CrToastElement>,
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
      deletionToastDurationMs_: {type: Number},
      deletionToastMessage_: {type: String},
      isEditing_: {type: Boolean},
      numSelected_: {type: Number},
      tables: {type: Array},
      tablesPendingDeletion_: {type: Object},
    };
  }

  accessor tables: ComparisonTableDetails[] = [];
  protected accessor deletionToastDurationMs_: number = 5000;
  protected accessor deletionToastMessage_: string = '';
  protected accessor isEditing_: boolean = false;
  protected accessor numSelected_: number = 0;
  protected accessor tablesPendingDeletion_: Set<Uuid> = new Set();

  private deletionTimeoutId_: number|null = null;
  private pluralStringProxy_: PluralStringProxy =
      PluralStringProxyImpl.getInstance();
  private productSpecificationsProxy_: ProductSpecificationsBrowserProxy =
      ProductSpecificationsBrowserProxyImpl.getInstance();
  private selectedUuids_: Set<Uuid> = new Set();
  private shoppingApi_: ShoppingServiceBrowserProxy =
      ShoppingServiceBrowserProxyImpl.getInstance();

  override connectedCallback() {
    super.connectedCallback();

    window.addEventListener('beforeunload', () => {
      // Delete any pending sets before navigating away.
      if (this.deletionTimeoutId_) {
        this.cancelPendingDeletionTimeout_();
        this.deletePendingTables_();
      }
    });
  }

  override disconnectedCallback() {
    super.disconnectedCallback();

    this.cancelPendingDeletionTimeout_();
  }

  resetDeletionToastDurationMsForTesting() {
    this.deletionToastDurationMs_ = 0;
  }

  protected getTables_(
      tables: ComparisonTableDetails[], uuidsPendingDeletion: Set<Uuid>) {
    // Hide tables that are pending deletion.
    return tables.filter(table => !uuidsPendingDeletion.has(table.uuid));
  }

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
      // Hide the toast before showing the toolbar.
      const toast = this.$.toast.getIfExists();
      if (toast) {
        toast.hide();
      }

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

    await this.updateDeletionToastMessage_();
    this.scheduleSelectedTablesForDeletion_();
    this.stopEditing_();

    // Ensure the toolbar is hidden before showing the toast.
    await this.updateComplete;
    await this.$.toast.get().show();
  }

  protected onShowContextMenuClick_() {
    this.$.menu.get().showAt(this.$.more);
  }

  protected onOpenAllClick_() {
    this.productSpecificationsProxy_.showProductSpecificationsSetsForUuids(
        Array.from(this.selectedUuids_), ShowSetDisposition.kInNewTabs);
    this.$.menu.get().close();

    this.fire('open-all-finished-for-testing');
  }

  protected onOpenAllInNewWindowClick_() {
    this.productSpecificationsProxy_.showProductSpecificationsSetsForUuids(
        Array.from(this.selectedUuids_), ShowSetDisposition.kInNewWindow);
    this.$.menu.get().close();

    this.fire('open-all-in-new-window-finished-for-testing');
  }

  protected onUndoDeletionClick_() {
    this.cancelPendingDeletionTimeout_();
    this.tablesPendingDeletion_.clear();
    this.$.toast.get().hide();

    // Request an update since the sets pending deletion were cleared.
    this.requestUpdate();
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

  protected onItemDelete_(event: ComparisonTableListItemDeleteEvent) {
    // Deselect all items and treat as deletion of a single selected item.
    this.selectedUuids_.clear();
    this.selectedUuids_.add(event.detail.uuid);
    this.numSelected_ = 1;

    this.onDeleteClick_();
  }

  protected stopEditing_() {
    this.isEditing_ = false;
    this.selectedUuids_.clear();
    this.numSelected_ = 0;
  }

  private scheduleSelectedTablesForDeletion_() {
    // Store the deleted sets so we can undo deletion.
    for (const uuid of this.selectedUuids_) {
      this.tablesPendingDeletion_.add(uuid);
    }

    // Delete the sets after the deletion toast has disappeared.
    this.deletionTimeoutId_ = setTimeout(() => {
      this.deletionTimeoutId_ = null;
      this.deletePendingTables_();
    }, this.deletionToastDurationMs_);
  }

  private deletePendingTables_() {
    for (const uuid of this.tablesPendingDeletion_) {
      this.shoppingApi_.deleteProductSpecificationsSet(uuid);
    }
    this.tablesPendingDeletion_.clear();

    this.fire('delete-finished-for-testing');
  }

  private async updateDeletionToastMessage_() {
    this.deletionToastMessage_ = await this.pluralStringProxy_.getPluralString(
        'deletionToastMessage', this.numSelected_);
  }

  private cancelPendingDeletionTimeout_() {
    if (this.deletionTimeoutId_) {
      clearTimeout(this.deletionTimeoutId_);
      this.deletionTimeoutId_ = null;
    }
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'comparison-table-list': ComparisonTableListElement;
  }
}

customElements.define(
    ComparisonTableListElement.is, ComparisonTableListElement);
