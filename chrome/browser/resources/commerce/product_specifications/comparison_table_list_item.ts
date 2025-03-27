// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '/strings.m.js';
import 'chrome://resources/cr_elements/cr_url_list_item/cr_url_list_item.js';
import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';
import 'chrome://resources/cr_elements/cr_action_menu/cr_action_menu.js';
import 'chrome://resources/cr_elements/cr_lazy_render/cr_lazy_render_lit.js';
import 'chrome://resources/cr_elements/icons.html.js';
import 'chrome://resources/cr_elements/cr_input/cr_input.js';
import 'chrome://resources/cr_elements/cr_checkbox/cr_checkbox.js';

import {ShowSetDisposition} from '//resources/cr_components/commerce/product_specifications.mojom-webui.js';
import {ShoppingServiceBrowserProxyImpl} from '//resources/cr_components/commerce/shopping_service_browser_proxy.js';
import type {ShoppingServiceBrowserProxy} from '//resources/cr_components/commerce/shopping_service_browser_proxy.js';
import type {CrActionMenuElement} from '//resources/cr_elements/cr_action_menu/cr_action_menu.js';
import type {CrIconButtonElement} from '//resources/cr_elements/cr_icon_button/cr_icon_button.js';
import type {CrLazyRenderLitElement} from '//resources/cr_elements/cr_lazy_render/cr_lazy_render_lit.js';
import type {CrUrlListItemElement} from '//resources/cr_elements/cr_url_list_item/cr_url_list_item.js';
import {assert} from '//resources/js/assert.js';
import type {PluralStringProxy} from '//resources/js/plural_string_proxy.js';
import {PluralStringProxyImpl} from '//resources/js/plural_string_proxy.js';
import type {Uuid} from '//resources/mojo/mojo/public/mojom/base/uuid.mojom-webui.js';
import type {ProductSpecificationsBrowserProxy} from 'chrome://resources/cr_components/commerce/product_specifications_browser_proxy.js';
import {ProductSpecificationsBrowserProxyImpl} from 'chrome://resources/cr_components/commerce/product_specifications_browser_proxy.js';
import type {CrCheckboxElement} from 'chrome://resources/cr_elements/cr_checkbox/cr_checkbox.js';
import type {CrInputElement} from 'chrome://resources/cr_elements/cr_input/cr_input.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import type {PropertyValues} from 'chrome://resources/lit/v3_0/lit.rollup.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';
import type {Url} from 'chrome://resources/mojo/url/mojom/url.mojom-webui.js';

import {getCss} from './comparison_table_list_item.css.js';
import {getHtml} from './comparison_table_list_item.html.js';
import {$$} from './utils.js';

export type ComparisonTableListItemClickEvent = CustomEvent<{uuid: Uuid}>;
export type ComparisonTableListItemRenameEvent = CustomEvent<{
  uuid: Uuid,
  name: string,
}>;
export type ComparisonTableListItemDeleteEvent = CustomEvent<{uuid: Uuid}>;
export type ComparisonTableListItemCheckboxChangeEvent = CustomEvent<{
  uuid: Uuid,
  checked: boolean,
}>;

export interface ComparisonTableListItemElement {
  $: {
    item: CrUrlListItemElement,
    menu: CrLazyRenderLitElement<CrActionMenuElement>,
    numItems: HTMLElement,
  };
}

export class ComparisonTableListItemElement extends CrLitElement {
  static get is() {
    return 'comparison-table-list-item';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      hasCheckbox: {type: Boolean},
      imageUrl_: {type: Object},
      isMenuOpen_: {type: Boolean},
      isRenaming_: {type: Boolean},
      name: {type: String},
      numItemsString_: {type: String},
      tableUrl_: {type: Object},
      urls: {type: Array},
      uuid: {type: Object},
    };
  }

  accessor hasCheckbox: boolean = false;
  accessor name: string = '';
  accessor urls: Url[] = [];
  accessor uuid: Uuid = {value: ''};
  protected accessor imageUrl_: Url|null = null;
  protected accessor isMenuOpen_: boolean = false;
  protected accessor isRenaming_: boolean = false;
  protected accessor numItemsString_: string = '';
  protected accessor tableUrl_: Url = {url: ''};

  private pluralStringProxy_: PluralStringProxy =
      PluralStringProxyImpl.getInstance();
  private productSpecificationsProxy_: ProductSpecificationsBrowserProxy =
      ProductSpecificationsBrowserProxyImpl.getInstance();
  private shoppingApi_: ShoppingServiceBrowserProxy =
      ShoppingServiceBrowserProxyImpl.getInstance();

  override async connectedCallback() {
    super.connectedCallback();

    this.addEventListener('close', () => {
      this.isMenuOpen_ = false;
    });

    const {url} =
        await this.productSpecificationsProxy_.getComparisonTableUrlForUuid(
            this.uuid);
    this.tableUrl_ = url;

    this.updateImage_();
    this.updateNumItemsString_();
  }

  override updated(changedProperties: PropertyValues<this>) {
    super.updated(changedProperties);

    if (changedProperties.has('urls')) {
      this.updateImage_();
      this.updateNumItemsString_();
    }
  }

  protected getTitle_() {
    return loadTimeData.getStringF('tableListItemTitle', this.name);
  }

  protected getFaviconUrl_() {
    // Display the favicon for the first product if no product images are
    // available. If there are no URLs, display the Compare favicon.
    if (this.urls.length > 0 && this.urls[0]) {
      return this.urls[0].url;
    }
    return this.tableUrl_.url;
  }

  protected async updateNumItemsString_() {
    this.numItemsString_ = await this.pluralStringProxy_.getPluralString(
        'numItems', this.urls.length);
    this.fire('num-items-updated-for-testing');
  }

  protected async updateImage_() {
    // Hide the currently displayed image.
    this.imageUrl_ = null;

    // Find the first product with an image to use as the item's image.
    let imageUrl = null;
    for (let i = 0; i < this.urls.length; i++) {
      const url = this.urls[i];
      assert(url);
      const {productInfo} = await this.shoppingApi_.getProductInfoForUrl(url);

      if (productInfo.imageUrl.url) {
        imageUrl = productInfo.imageUrl;
        break;
      }
    }

    this.imageUrl_ = imageUrl;
    this.fire('image-updated-for-testing');
  }

  protected onClick_(event: MouseEvent) {
    // Treat the item click as a checkbox click if it has checkbox.
    if (this.hasCheckbox) {
      event.preventDefault();
      event.stopPropagation();

      this.checkbox_.checked = !this.checkbox_.checked;
      return;
    }

    if (!this.isRenaming_) {
      this.fire('item-click', {
        uuid: this.uuid,
      });
    }
  }

  protected onContextMenu_(event: MouseEvent) {
    event.preventDefault();
    event.stopPropagation();

    this.isMenuOpen_ = true;
    this.$.menu.get().showAtPosition({
      top: event.clientY,
      left: event.clientX,
    });
  }

  protected onShowContextMenuClick_(event: MouseEvent) {
    event.preventDefault();
    event.stopPropagation();

    this.isMenuOpen_ = true;
    this.$.menu.get().showAt(this.trailingIconButton_);
  }

  protected onOpenInNewTabClick_() {
    this.$.menu.get().close();
    this.productSpecificationsProxy_.showProductSpecificationsSetForUuid(
        this.uuid, true);
  }

  protected onOpenInNewWindowClick_() {
    this.$.menu.get().close();
    this.productSpecificationsProxy_.showProductSpecificationsSetsForUuids(
        [this.uuid], ShowSetDisposition.kInNewWindow);
  }

  protected async onRenameClick_() {
    this.$.menu.get().close();
    this.isRenaming_ = true;

    // Focus the input once it is rendered.
    await this.updateComplete;
    this.input_.focus();
  }

  protected onDeleteClick_() {
    this.$.menu.get().close();
    this.fire('delete-table', {
      uuid: this.uuid,
    });
  }

  protected onRenameInputBlur_() {
    if (this.input_.value.length !== 0) {
      this.fire('rename-table', {
        uuid: this.uuid,
        name: this.input_.value,
      });
    }

    this.isRenaming_ = false;
  }

  protected onRenameInputKeyDown_(event: KeyboardEvent) {
    if (event.key === 'Enter') {
      event.stopPropagation();
      this.input_.blur();
    }
  }

  protected onCheckboxChange_(event: Event) {
    event.preventDefault();
    event.stopPropagation();

    this.fire('checkbox-change', {
      uuid: this.uuid,
      checked: (event.target as CrCheckboxElement).checked,
    });
  }

  private get trailingIconButton_() {
    const trailingIconButton =
        $$<CrIconButtonElement>(this, '#trailingIconButton');
    assert(trailingIconButton);
    return trailingIconButton;
  }

  private get input_() {
    const input = $$<CrInputElement>(this, '#renameInput');
    assert(input);
    return input;
  }

  private get checkbox_(): CrCheckboxElement {
    const checkbox = $$<CrCheckboxElement>(this, '#checkbox');
    assert(checkbox);
    return checkbox;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'comparison-table-list-item': ComparisonTableListItemElement;
  }
}

customElements.define(
    ComparisonTableListItemElement.is, ComparisonTableListItemElement);
