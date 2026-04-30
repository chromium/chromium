// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_auto_img/cr_auto_img.js';
import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';
import 'chrome://resources/cr_elements/cr_icon/cr_icon.js';
import 'chrome://resources/cr_elements/cr_toast/cr_toast.js';
import './icons.html.js';

import type {PriceTrackingBrowserProxy} from '//resources/cr_components/commerce/price_tracking_browser_proxy.js';
import {PriceTrackingBrowserProxyImpl} from '//resources/cr_components/commerce/price_tracking_browser_proxy.js';
import type {BookmarkProductInfo} from '//resources/cr_components/commerce/shared.mojom-webui.js';
import type {CrToastElement} from 'chrome://resources/cr_elements/cr_toast/cr_toast.js';
import {getFaviconForPageURL} from 'chrome://resources/js/icon.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';
import type {PropertyValues} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {ActionSource} from '../bookmarks.mojom-webui.js';
import type {BookmarksApiProxy} from '../bookmarks_api_proxy.js';
import {BookmarksApiProxyImpl} from '../bookmarks_api_proxy.js';

import {getCss} from './shopping_list.css.js';
import {getHtml} from './shopping_list.html.js';

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

export class ShoppingListElement extends CrLitElement {
  static get is() {
    return 'shopping-list';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      open_: {type: Boolean},
      untrackedItems_: {type: Array},
      productInfos: {type: Array},
    };
  }

  accessor productInfos: BookmarkProductInfo[] = [];
  private accessor untrackedItems_: BookmarkProductInfo[] = [];
  protected accessor open_: boolean = true;
  private bookmarksApi_: BookmarksApiProxy =
      BookmarksApiProxyImpl.getInstance();
  private priceTrackingProxy_: PriceTrackingBrowserProxy =
      PriceTrackingBrowserProxyImpl.getInstance();
  private listenerIds_: number[] = [];
  private retryOperationCallback_: () => void;

  override connectedCallback() {
    super.connectedCallback();

    const callbackRouter = this.priceTrackingProxy_.getCallbackRouter();
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
        id => this.priceTrackingProxy_.getCallbackRouter().removeListener(id));
  }

  override willUpdate(changedProperties: PropertyValues) {
    super.willUpdate(changedProperties as PropertyValues<this>);
    if (changedProperties.has('productInfos')) {
      this.untrackedItems_ = this.untrackedItems_.filter(
          untrackedItem => this.productInfos.includes(untrackedItem));
    }
  }

  protected getFaviconUrl_(url: string): string {
    return getFaviconForPageURL(url, false);
  }

  protected onFolderClick_(event: Event) {
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

  private getProductInfoFromEvent_(event: Event): BookmarkProductInfo {
    const target = event.currentTarget as HTMLElement;
    const index = Number(target.dataset['index']);
    return this.productInfos[index];
  }

  protected onProductAuxclick_(event: MouseEvent) {
    if (event.button !== 1) {
      // Not a middle click.
      return;
    }

    event.preventDefault();
    event.stopPropagation();
    chrome.metricsPrivate.recordUserAction(
        'Commerce.PriceTracking.SidePanel.ClickedTrackedProduct');
    const item = this.getProductInfoFromEvent_(event);
    this.bookmarksApi_.openBookmark(
        item.bookmarkId.toString(), 0, {
          middleButton: true,
          altKey: event.altKey,
          ctrlKey: event.ctrlKey,
          metaKey: event.metaKey,
          shiftKey: event.shiftKey,
        },
        ActionSource.kPriceTracking);
  }

  protected onProductClick_(event: MouseEvent) {
    event.preventDefault();
    event.stopPropagation();
    chrome.metricsPrivate.recordUserAction(
        'Commerce.PriceTracking.SidePanel.ClickedTrackedProduct');
    const item = this.getProductInfoFromEvent_(event);
    this.bookmarksApi_.openBookmark(
        item.bookmarkId.toString(), 0, {
          middleButton: false,
          altKey: event.altKey,
          ctrlKey: event.ctrlKey,
          metaKey: event.metaKey,
          shiftKey: event.shiftKey,
        },
        ActionSource.kPriceTracking);
  }

  protected onProductContextmenu_(event: MouseEvent) {
    event.preventDefault();
    event.stopPropagation();
    const item = this.getProductInfoFromEvent_(event);
    this.bookmarksApi_.showContextMenu(
        item.bookmarkId.toString(), event.clientX, event.clientY,
        ActionSource.kPriceTracking);
  }

  protected onActionButtonClick_(event: MouseEvent) {
    event.preventDefault();
    event.stopPropagation();
    const item = this.getProductInfoFromEvent_(event);
    const bookmarkId = item.bookmarkId;
    if (this.untrackedItems_.includes(item)) {
      const index = this.untrackedItems_.indexOf(item);
      this.untrackedItems_.splice(index, 1);
      this.requestUpdate();
      this.priceTrackingProxy_.trackPriceForBookmark(bookmarkId);
      chrome.metricsPrivate.recordUserAction(
          'Commerce.PriceTracking.SidePanel.Track.BellButton');
    } else {
      this.untrackedItems_.push(item);
      this.requestUpdate();
      this.priceTrackingProxy_.untrackPriceForBookmark(bookmarkId);
      chrome.metricsPrivate.recordUserAction(
          'Commerce.PriceTracking.SidePanel.Untrack.BellButton');
    }
  }

  protected getIconForItem_(item: BookmarkProductInfo): string {
    return this.untrackedItems_.includes(item) ? ACTION_BUTTON_TRACK_IMAGE :
                                                 ACTION_BUTTON_UNTRACK_IMAGE;
  }

  protected getButtonDescriptionForItem_(item: BookmarkProductInfo): string {
    return this.untrackedItems_.includes(item) ?
        loadTimeData.getString('shoppingListTrackPriceButtonDescription') :
        loadTimeData.getString('shoppingListUntrackPriceButtonDescription');
  }

  private onBookmarkPriceTracked(product: BookmarkProductInfo) {
    const productItem =
        this.productInfos.find(item => item.bookmarkId === product.bookmarkId);
    if (productItem == null) {
      this.productInfos.push(product);
      this.requestUpdate();
      return;
    }
    this.untrackedItems_ = this.untrackedItems_.filter(
        item => item.bookmarkId !== product.bookmarkId);
    this.requestUpdate();
    if (!this.isSameProduct_(productItem, product)) {
      const index = this.productInfos.indexOf(productItem);
      this.productInfos.splice(index, 1, product);
      this.requestUpdate();
    }
  }

  private onBookmarkPriceUntracked(product: BookmarkProductInfo) {
    const untrackedItem =
        this.productInfos.find(item => item.bookmarkId === product.bookmarkId);
    if (untrackedItem == null) {
      return;
    }
    if (!this.untrackedItems_.includes(untrackedItem)) {
      this.untrackedItems_.push(untrackedItem);
      this.requestUpdate();
    }
  }

  private isSameProduct_(
      itemA: BookmarkProductInfo, itemB: BookmarkProductInfo) {
    // Only compare the user-visible properties.
    if (itemA.info.title !== itemB.info.title ||
        itemA.info.imageUrl !== itemB.info.imageUrl ||
        itemA.info.currentPrice !== itemB.info.currentPrice ||
        itemA.info.previousPrice !== itemB.info.previousPrice) {
      return false;
    }
    return true;
  }

  protected onProductImageLoad_() {
    chrome.metricsPrivate.recordBoolean(
        'Commerce.PriceTracking.SidePanelImageLoad', true);
  }

  protected onImageLoadError_(event: Event) {
    const item = this.getProductInfoFromEvent_(event);
    item.info.imageUrl = '';
    this.requestUpdate();
    chrome.metricsPrivate.recordBoolean(
        'Commerce.PriceTracking.SidePanelImageLoad', false);
  }

  private onBookmarkOperationFailed(
      product: BookmarkProductInfo, attemptedTrack: boolean) {
    this.retryOperationCallback_ = () => {
      if (attemptedTrack) {
        this.priceTrackingProxy_.trackPriceForBookmark(product.bookmarkId);
      } else {
        this.priceTrackingProxy_.untrackPriceForBookmark(product.bookmarkId);
      }
    };
    this.$.errorToast.show();
  }

  protected onErrorRetryClick_() {
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
