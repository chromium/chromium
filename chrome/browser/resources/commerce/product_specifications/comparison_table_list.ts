// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_lazy_list/cr_lazy_list.js';
import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';
import 'chrome://resources/cr_elements/cr_toolbar/cr_toolbar_selection_overlay.js';
import './images/icons.html.js';

import type {ShoppingServiceBrowserProxy} from '//resources/cr_components/commerce/shopping_service_browser_proxy.js';
import {ShoppingServiceBrowserProxyImpl} from '//resources/cr_components/commerce/shopping_service_browser_proxy.js';
import {loadTimeData} from '//resources/js/load_time_data.js';
import type {Uuid} from '//resources/mojo/mojo/public/mojom/base/uuid.mojom-webui.js';
import type {Url} from '//resources/mojo/url/mojom/url.mojom-webui.js';
import type {CrIconButtonElement} from 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';
import type {CrToolbarSelectionOverlayElement} from 'chrome://resources/cr_elements/cr_toolbar/cr_toolbar_selection_overlay.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {getCss} from './comparison_table_list.css.js';
import {getHtml} from './comparison_table_list.html.js';
import type {ComparisonTableListItemCheckboxChangeEvent} from './comparison_table_list_item.js';

export interface ComparisonTableDetails {
  name: string;
  uuid: Uuid;
  numUrls: number;
  imageUrl: Url|null;
}

export interface ComparisonTableListElement {
  $: {
    delete: CrIconButtonElement,
    edit: CrIconButtonElement,
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

  protected getSelectionLabel_(numSelected: number): string {
    return loadTimeData.getStringF('numSelected', numSelected);
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
    // Disable the delete button while sets are being deleted.
    this.deletePending_ = true;
    const promises: void[] = [];
    this.selectedUuids_.forEach(uuid => {
      promises.push(this.shoppingApi_.deleteProductSpecificationsSet(uuid));
    });
    this.deletePending_ = false;

    await Promise.all(promises);
    this.stopEditing_();

    this.fire('delete-finished-for-testing');
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
}

declare global {
  interface HTMLElementTagNameMap {
    'comparison-table-list': ComparisonTableListElement;
  }
}

customElements.define(
    ComparisonTableListElement.is, ComparisonTableListElement);
