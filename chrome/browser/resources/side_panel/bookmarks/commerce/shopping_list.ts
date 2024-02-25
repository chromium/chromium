// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';
import 'chrome://resources/cr_elements/cr_shared_vars.css.js';
import 'chrome://resources/cr_elements/mwb_element_shared_style.css.js';
import 'chrome://resources/cr_elements/cr_auto_img/cr_auto_img.js';
import 'chrome://resources/cr_elements/cr_toast/cr_toast.js';
import './icons.html.js';

import type {BrowserProxy} from '//resources/cr_components/commerce/browser_proxy.js';
import {BrowserProxyImpl} from '//resources/cr_components/commerce/browser_proxy.js';
import type {BookmarkProductInfo} from '//resources/cr_components/commerce/shopping_service.mojom-webui.js';
import type {CrToastElement} from 'chrome://resources/cr_elements/cr_toast/cr_toast.js';
import {getFaviconForPageURL} from 'chrome://resources/js/icon.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import type {DomRepeatEvent} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {ActionSource} from '../bookmarks.mojom-webui.js';
import type {BookmarksApiProxy} from '../bookmarks_api_proxy.js';
import {BookmarksApiProxyImpl} from '../bookmarks_api_proxy.js';

import {getTemplate} from './shopping_list.html.js';

export const LOCAL_STORAGE_EXPAND_STATUS_KEY = 'shoppingListExpanded';
export const ACTION_BUTTON_TRACK_IMAGE =
    'shopping-list:shopping-list-track-icon';
export const ACTION_BUTTON_UNTRACK_IMAGE =
    'shopping-list:shopping-list-untrack-icon';

export interface ShoppingListElement {
  $: {
    errorToast: CrToastElement,
  };
}

export class ShoppingListElement extends PolymerElement {
  static get is() {
    return 'shopping-list';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      open_: {
        type: Boolean,
        value: true,
      },

      untrackedItems_: {
        type: Array,
        value: () => [],
      },

      productInfos: {
        type: Array,
        value: () => [],
        observer: 'onProductInfoChanged_',
      },
    };
  }

  productInfos: BookmarkProductInfo[];
  private untrackedItems_: BookmarkProductInfo[];
  private open_: boolean;
  private bookmarksApi_: BookmarksApiProxy =
      BookmarksApiProxyImpl.getInstance();
  private shoppingServiceApi_: BrowserProxy = BrowserProxyImpl.getInstance();
  private listenerIds_: number[] = [];
  private retryOperationCallback_: () => void;

  override connectedCallback() {
    super.connectedCallback();

    const callbackRouter = this.shoppingServiceApi_.getCallbackRouter();
    this.listenerIds_.push(
        callbackRouter.priceTrackedForBookmark.addListener(
            (product: BookmarkProductInfo) =>
                this.onBookmarkPriceTracked(product)),
        callbackRouter.priceUntrackedForBookmark.addListener(
            (product: BookmarkProductInfo) =>
                this.onBookmarkPriceUntracked(product)),
        callbackRouter.operationFailedForBookmark.addListener(
            (product: BookmarkProductInfo, attemptedTrack: boolean) =>
                this.onBookmarkOperationFailed(product, attemptedTrack)),
    );
    try {
      this.open_ =
          JSON.parse(window.localStorage[LOCAL_STORAGE_EXPAND_STATUS_KEY]);
    } catch (e) {
      this.open_ = true;
      window.localStorage[LOCAL_STORAGE_EXPAND_STATUS_KEY] = this.open_;
    }
  }

  override disconnectedCallback() {
    super.disconnectedCallback();

    this.listenerIds_.forEach(
        id => this.shoppingServiceApi_.getCallbackRouter().removeListener(id));
  }

  private getFaviconUrl_(url: string): string {
    return getFaviconForPageURL(url, false);
  }

  private onFolderClick_(event: Event) {
    event.preventDefault();
    event.stopPropagation();

    this.open_ = !this.open_;
    window.localStorage[LOCAL_STORAGE_EXPAND_STATUS_KEY] = this.open_;
    if (this.open_) {
      chrome.metricsPrivate.recordUserAction(
          'Commerce.PriceTracking.SidePanel.TrackedProductsExpanded');
    } else {
      chrome.metricsPrivate.recordUserAction(
          'Commerce.PriceTracking.SidePanel.TrackedProductsCollapsed');
    }
  }

  private onProductAuxClick_(
      event: DomRepeatEvent<BookmarkProductInfo, MouseEvent>) {
    if (event.button !== 1) {
      // Not a middle click.
      return;
    }

    event.preventDefault();
    event.stopPropagation();
    chrome.metricsPrivate.recordUserAction(
        'Commerce.PriceTracking.SidePanel.ClickedTrackedProduct');
    this.bookmarksApi_.openBookmark(
        event.model.item.bookmarkId!.toString(), 0, {
          middleButton: true,
          altKey: event.altKey,
          ctrlKey: event.ctrlKey,
          metaKey: event.metaKey,
          shiftKey: event.shiftKey,
        },
        ActionSource.kPriceTracking);
  }

  private onProductClick_(event:
                              DomRepeatEvent<BookmarkProductInfo, MouseEvent>) {
    event.preventDefault();
    event.stopPropagation();
    chrome.metricsPrivate.recordUserAction(
        'Commerce.PriceTracking.SidePanel.ClickedTrackedProduct');
    this.bookmarksApi_.openBookmark(
        event.model.item.bookmarkId!.toString(), 0, {
          middleButton: false,
          altKey: event.altKey,
          ctrlKey: event.ctrlKey,
          metaKey: event.metaKey,
          shiftKey: event.shiftKey,
        },
        ActionSource.kPriceTracking);
  }

  private onProductContextMenu_(
      event: DomRepeatEvent<BookmarkProductInfo, MouseEvent>) {
    event.preventDefault();
    event.stopPropagation();
    this.bookmarksApi_.showContextMenu(
        event.model.item.bookmarkId!.toString(), event.clientX, event.clientY,
        ActionSource.kPriceTracking);
  }

  private onActionButtonClick_(
      event: DomRepeatEvent<BookmarkProductInfo, MouseEvent>) {
    event.preventDefault();
    event.stopPropagation();
    const bookmarkId = event.model.item.bookmarkId!;
    if (this.untrackedItems_.includes(event.model.item)) {
      const index = this.untrackedItems_.indexOf(event.model.item);
      this.splice('untrackedItems_', index, 1);
      this.shoppingServiceApi_.trackPriceForBookmark(bookmarkId);
      chrome.metricsPrivate.recordUserAction(
          'Commerce.PriceTracking.SidePanel.Track.BellButton');
    } else {
      this.push('untrackedItems_', event.model.item);
      this.shoppingServiceApi_.untrackPriceForBookmark(bookmarkId);
      chrome.metricsPrivate.recordUserAction(
          'Commerce.PriceTracking.SidePanel.Untrack.BellButton');
    }
  }

  private getIconForItem_(item: BookmarkProductInfo): string {
    return this.untrackedItems_.includes(item) ? ACTION_BUTTON_TRACK_IMAGE :
                                                 ACTION_BUTTON_UNTRACK_IMAGE;
  }

  private getButtonDescriptionForItem_(item: BookmarkProductInfo): string {
    return this.untrackedItems_.includes(item) ?
        loadTimeData.getString('shoppingListTrackPriceButtonDescription') :
        loadTimeData.getString('shoppingListUntrackPriceButtonDescription');
  }

  private onBookmarkPriceTracked(product: BookmarkProductInfo) {
    const productItem =
        this.productInfos.find(item => item.bookmarkId === product.bookmarkId);
    if (productItem == null) {
      this.push('productInfos', product);
      return;
    }
    this.untrackedItems_ = this.untrackedItems_.filter(
        item => item.bookmarkId !== product.bookmarkId);
    if (!this.isSameProduct_(productItem, product)) {
      const index = this.productInfos.indexOf(productItem);
      this.splice('productInfos', index, 1);
      this.splice('productInfos', index, 0, product);
    }
  }

  private onBookmarkPriceUntracked(product: BookmarkProductInfo) {
    const untrackedItem =
        this.productInfos.find(item => item.bookmarkId === product.bookmarkId);
    if (untrackedItem == null) {
      return;
    }
    if (!this.untrackedItems_.includes(untrackedItem)) {
      this.push('untrackedItems_', untrackedItem);
    }
  }

  private isSameProduct_(
      itemA: BookmarkProductInfo, itemB: BookmarkProductInfo) {
    // Only compare the user-visible properties.
    if (itemA.info.title !== itemB.info.title ||
        itemA.info.imageUrl.url !== itemB.info.imageUrl.url ||
        itemA.info.currentPrice !== itemB.info.currentPrice ||
        itemA.info.previousPrice !== itemB.info.previousPrice) {
      return false;
    }
    return true;
  }

  private onProductInfoChanged_() {
    this.untrackedItems_ = this.untrackedItems_.filter(
        untrackedItem => this.productInfos.includes(untrackedItem));
  }

  private onImageLoadSuccess_() {
    chrome.metricsPrivate.recordBoolean(
        'Commerce.PriceTracking.SidePanelImageLoad', true);
  }

  private onImageLoadError_(event: DomRepeatEvent<BookmarkProductInfo>) {
    this.set('productInfos.' + event.model.index + '.info.imageUrl.url', '');
    chrome.metricsPrivate.recordBoolean(
        'Commerce.PriceTracking.SidePanelImageLoad', false);
  }

  private onBookmarkOperationFailed(
      product: BookmarkProductInfo, attemptedTrack: boolean) {
    this.retryOperationCallback_ = () => {
      if (attemptedTrack) {
        this.shoppingServiceApi_.trackPriceForBookmark(product.bookmarkId);
      } else {
        this.shoppingServiceApi_.untrackPriceForBookmark(product.bookmarkId);
      }
    };
    this.$.errorToast.show();
  }

  private onErrorRetryClicked_() {
    if (this.retryOperationCallback_ == null) {
      return;
    }
    this.retryOperationCallback_();
    this.$.errorToast.hide();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'shopping-list': ShoppingListElement;
  }
}

customElements.define(ShoppingListElement.is, ShoppingListElement);
