// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '../strings.m.js';
import './commerce/shopping_list.js';
import './power_bookmark_chip.js';
import './power_bookmark_row.js';
import '//resources/cr_elements/icons.html.js';

import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {PluralStringProxyImpl} from 'chrome://resources/js/plural_string_proxy.js';
import {listenOnce} from 'chrome://resources/js/util.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {ActionSource} from './bookmarks.mojom-webui.js';
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

      activeFolderPath_: {
        type: Array,
        value: () => [],
      },

      showPriceTracking_: {
        type: Boolean,
        value: false,
      },

      activeSortIndex_: {
        type: Number,
        value: 0,
      },

      sortTypes_: {
        type: Array,
        value: () =>
            [loadTimeData.getString('sortNewest'),
             loadTimeData.getString('sortOldest')],
      },
    };
  }

  private topLevelBookmarks_: chrome.bookmarks.BookmarkTreeNode[];
  private bookmarksApi_: BookmarksApiProxy =
      BookmarksApiProxyImpl.getInstance();
  private shoppingListApi_: ShoppingListApiProxy =
      ShoppingListApiProxyImpl.getInstance();
  private productInfos_ = new Map<string, BookmarkProductInfo>();
  private shoppingListenerIds_: number[] = [];
  private compact_: boolean;
  private activeFolderPath_: chrome.bookmarks.BookmarkTreeNode[];
  private descriptions_ = new Map<string, string>();
  private showPriceTracking_: boolean;
  private activeSortIndex_: number;
  private sortTypes_: string[];

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
      res.productInfos.forEach(
          product =>
              this.productInfos_.set(product.bookmarkId.toString(), product));
      if (this.productInfos_.size > 0) {
        this.showPriceTracking_ = true;
        chrome.metricsPrivate.recordUserAction(
            'Commerce.PriceTracking.SidePanel.TrackedProductsShown');
      }
    });
    const callbackRouter = this.shoppingListApi_.getCallbackRouter();
    this.shoppingListenerIds_.push(
        callbackRouter.priceTrackedForBookmark.addListener(
            () => this.onBookmarkPriceTracked()),
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

  private getFolderSortLabel_(): string {
    let folderName;
    if (this.activeFolderPath_.length) {
      const activeFolder =
          this.activeFolderPath_[this.activeFolderPath_.length - 1];
      folderName = activeFolder!.title;
    } else {
      folderName = loadTimeData.getString('allBookmarks');
    }
    return loadTimeData.getStringF(
        'folderSort', folderName, this.sortTypes_[this.activeSortIndex_]!);
  }

  private getProductInfos_(): BookmarkProductInfo[] {
    return Array.from(this.productInfos_.values());
  }

  private isPriceTracked_(bookmark: chrome.bookmarks.BookmarkTreeNode):
      boolean {
    return this.productInfos_.has(bookmark.id);
  }

  /**
   * Returns a list of bookmarks and folders to display to the user.
   */
  private getShownBookmarks_(): chrome.bookmarks.BookmarkTreeNode[] {
    let shownBookmarks;
    const activeFolder =
        this.activeFolderPath_[this.activeFolderPath_.length - 1];
    if (activeFolder) {
      shownBookmarks = activeFolder.children!;
    } else {
      shownBookmarks = this.topLevelBookmarks_;
    }
    this.sortBookmarks_(shownBookmarks);
    return shownBookmarks;
  }

  private sortBookmarks_(bookmarks: chrome.bookmarks.BookmarkTreeNode[]) {
    const activeSortIndex = this.activeSortIndex_;
    bookmarks.sort(function(
        a: chrome.bookmarks.BookmarkTreeNode,
        b: chrome.bookmarks.BookmarkTreeNode) {
      // Always sort by folders first
      if (a.children && !b.children) {
        return -1;
      } else if (!a.children && b.children) {
        return 1;
      } else {
        if (activeSortIndex === 0) {
          // Newest first
          return b.dateAdded! - a.dateAdded!;
        } else {
          // Oldest first
          return a.dateAdded! - b.dateAdded!;
        }
      }
    });
  }

  /**
   * Invoked when the user clicks a power bookmarks row. This will either
   * display children in the case of a folder row, or open the URL in the case
   * of a bookmark row.
   */
  private onRowClicked_(
      event: CustomEvent<
          {bookmark: chrome.bookmarks.BookmarkTreeNode, event: MouseEvent}>) {
    event.preventDefault();
    event.stopPropagation();
    if (event.detail.bookmark.children) {
      this.push('activeFolderPath_', event.detail.bookmark);
    } else {
      this.bookmarksApi_.openBookmark(
          event.detail.bookmark.id, this.activeFolderPath_.length, {
            middleButton: false,
            altKey: event.detail.event.altKey,
            ctrlKey: event.detail.event.ctrlKey,
            metaKey: event.detail.event.metaKey,
            shiftKey: event.detail.event.shiftKey,
          },
          ActionSource.kBookmark);
    }
  }

  /**
   * Moves the displayed folders up one level when the back button is clicked.
   */
  private onBackClicked_() {
    this.pop('activeFolderPath_');
  }

  /**
   * Whether the given price-tracked bookmark should display as if discounted.
   */
  private showDiscountedPrice_(bookmark: chrome.bookmarks.BookmarkTreeNode):
      boolean {
    const bookmarkProductInfo = this.productInfos_.get(bookmark.id);
    if (bookmarkProductInfo) {
      return bookmarkProductInfo.info.previousPrice.length > 0;
    }
    return false;
  }

  private getCurrentPrice_(bookmark: chrome.bookmarks.BookmarkTreeNode):
      string {
    const bookmarkProductInfo = this.productInfos_.get(bookmark.id);
    if (bookmarkProductInfo) {
      return bookmarkProductInfo.info.currentPrice;
    } else {
      return '';
    }
  }

  private getPreviousPrice_(bookmark: chrome.bookmarks.BookmarkTreeNode):
      string {
    const bookmarkProductInfo = this.productInfos_.get(bookmark.id);
    if (bookmarkProductInfo) {
      return bookmarkProductInfo.info.previousPrice;
    } else {
      return '';
    }
  }

  private onBookmarkPriceTracked() {
    // Here we only control the visibility of ShoppingListElement. The same
    // signal will also be handled in ShoppingListElement to update shopping
    // list.
    if (this.productInfos_.size > 0) {
      return;
    }
    this.showPriceTracking_ = true;
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
