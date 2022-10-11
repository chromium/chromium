// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './commerce/shopping_list.js';
import './power_bookmark_chip.js';
import './power_bookmark_row.js';

import {PluralStringProxyImpl} from 'chrome://resources/js/plural_string_proxy.js';
import {listenOnce} from 'chrome://resources/js/util.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {BookmarksApiProxy, BookmarksApiProxyImpl} from './bookmarks_api_proxy.js';
import {BookmarkProductInfo} from './commerce/shopping_list.mojom-webui.js';
import {ShoppingListApiProxy, ShoppingListApiProxyImpl} from './commerce/shopping_list_api_proxy.js';
import {getTemplate} from './power_bookmarks_list.html.js';

export interface PowerBookmarksListElement {
  $: {
    powerBookmarksContainer: HTMLElement,
  };
}

export class PowerBookmarksListElement extends PolymerElement {
  static get is() {
    return 'power-bookmarks-list';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      topLevelBookmarks_: {
        type: Array,
        value: () => [],
      },

      compact_: {
        type: Boolean,
        value: true,
      },
    };
  }

  private topLevelBookmarks_: chrome.bookmarks.BookmarkTreeNode[];
  private bookmarksApi_: BookmarksApiProxy =
      BookmarksApiProxyImpl.getInstance();
  private shoppingListApi_: ShoppingListApiProxy =
      ShoppingListApiProxyImpl.getInstance();
  private productInfos_: BookmarkProductInfo[];
  private shoppingListenerIds_: number[] = [];
  private compact_: boolean;
  private descriptions_ = new Map<string, string>();

  override connectedCallback() {
    super.connectedCallback();
    this.setAttribute('role', 'tree');
    listenOnce(this.$.powerBookmarksContainer, 'dom-change', () => {
      setTimeout(() => this.bookmarksApi_.showUI(), 0);
    });
    this.bookmarksApi_.getTopLevelBookmarks().then(topLevelBookmarks => {
      this.topLevelBookmarks_ = topLevelBookmarks;
      this.topLevelBookmarks_.forEach(bookmark => {
        this.findBookmarkDescriptions_(bookmark);
      });
    });
    this.shoppingListApi_.getAllPriceTrackedBookmarkProductInfo().then(res => {
      this.productInfos_ = res.productInfos;
      if (this.productInfos_.length > 0) {
        chrome.metricsPrivate.recordUserAction(
            'Commerce.PriceTracking.SidePanel.TrackedProductsShown');
      }
    });
    const callbackRouter = this.shoppingListApi_.getCallbackRouter();
    this.shoppingListenerIds_.push(
        callbackRouter.priceTrackedForBookmark.addListener(
            (product: BookmarkProductInfo) =>
                this.onBookmarkPriceTracked(product)),
    );
  }

  override disconnectedCallback() {
    this.shoppingListenerIds_.forEach(
        id => this.shoppingListApi_.getCallbackRouter().removeListener(id));
  }

  /**
   * Assigns a text description for the given bookmark and all descendants, to
   * be displayed following the bookmark title.
   */
  private findBookmarkDescriptions_(bookmark:
                                        chrome.bookmarks.BookmarkTreeNode) {
    if (bookmark.children && this.compact_) {
      PluralStringProxyImpl.getInstance()
          .getPluralString('bookmarkFolderChildCount', bookmark.children.length)
          .then(pluralString => {
            this.set(`descriptions_.${bookmark.id}`, pluralString);
          });
    } else if (bookmark.url && !this.compact_) {
      const url = new URL(bookmark.url);
      // Show chrome:// if it's a chrome internal url
      if (url.protocol === 'chrome:') {
        this.set(`descriptions_.${bookmark.id}`, 'chrome://' + url.hostname);
      }
      this.set(`descriptions_.${bookmark.id}`, url.hostname);
    } else {
      this.set(`descriptions_.${bookmark.id}`, '');
    }
    if (bookmark.children) {
      bookmark.children.forEach(child => this.findBookmarkDescriptions_(child));
    }
  }

  private getBookmarkDescription_(bookmark: chrome.bookmarks.BookmarkTreeNode) {
    return this.get(`descriptions_.${bookmark.id}`);
  }

  /**
   * Whether the given price-tracked bookmark should display as if discounted.
   */
  private showDiscountedPrice_(): boolean {
    // TODO: Incorporate actual price tracking data here
    return true;
  }

  private onBookmarkPriceTracked(product: BookmarkProductInfo) {
    // Here we only control the visibility of ShoppingListElement. The same
    // signal will also be handled in ShoppingListElement to update shopping
    // list.
    if (this.productInfos_.length > 0) {
      return;
    }
    this.push('productInfos_', product);
    chrome.metricsPrivate.recordUserAction(
        'Commerce.PriceTracking.SidePanel.TrackedProductsShown');
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'power-bookmarks-list': PowerBookmarksListElement;
  }
}

customElements.define(PowerBookmarksListElement.is, PowerBookmarksListElement);
