// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '../strings.m.js';
import './icons.html.js';
import '//resources/cr_elements/cr_action_menu/cr_action_menu.js';
import '//resources/cr_elements/cr_icon_button/cr_icon_button.js';
import '//resources/cr_elements/icons.html.js';

import {CrActionMenuElement} from '//resources/cr_elements/cr_action_menu/cr_action_menu.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {DomRepeatEvent, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {ActionSource} from './bookmarks.mojom-webui.js';
import {BookmarksApiProxy, BookmarksApiProxyImpl} from './bookmarks_api_proxy.js';
import {ShoppingListApiProxy, ShoppingListApiProxyImpl} from './commerce/shopping_list_api_proxy.js';
import {getTemplate} from './power_bookmarks_context_menu.html.js';
import {editingDisabledByPolicy} from './power_bookmarks_service.js';

export interface PowerBookmarksContextMenuElement {
  $: {
    menu: CrActionMenuElement,
  };
}

export enum MenuItemId {
  OPEN_NEW_TAB = 0,
  OPEN_NEW_WINDOW = 1,
  OPEN_INCOGNITO = 2,
  EDIT = 3,
  ADD_TO_BOOKMARKS_BAR = 4,
  REMOVE_FROM_BOOKMARKS_BAR = 5,
  TRACK_PRICE = 6,
  RENAME = 7,
  DELETE = 8,
  DIVIDER = 9,
}

export interface MenuItem {
  id: MenuItemId;
  label?: string;
}

export class PowerBookmarksContextMenuElement extends PolymerElement {
  static get is() {
    return 'power-bookmarks-context-menu';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      bookmarks_: Array,
    };
  }

  private bookmarksApi_: BookmarksApiProxy =
      BookmarksApiProxyImpl.getInstance();
  private shoppingListApi_: ShoppingListApiProxy =
      ShoppingListApiProxyImpl.getInstance();
  private bookmarks_: chrome.bookmarks.BookmarkTreeNode[] = [];
  private priceTracked_: boolean;
  private priceTrackingEligible_: boolean;

  showAt(
      event: MouseEvent, bookmarks: chrome.bookmarks.BookmarkTreeNode[],
      priceTracked: boolean, priceTrackingEligible: boolean) {
    this.bookmarks_ = bookmarks;
    this.priceTracked_ = priceTracked;
    this.priceTrackingEligible_ = priceTrackingEligible;
    this.$.menu.showAt(event.target as HTMLElement);
  }

  showAtPosition(
      event: MouseEvent, bookmarks: chrome.bookmarks.BookmarkTreeNode[],
      priceTracked: boolean, priceTrackingEligible: boolean) {
    this.bookmarks_ = bookmarks;
    this.priceTracked_ = priceTracked;
    this.priceTrackingEligible_ = priceTrackingEligible;
    this.$.menu.showAtPosition({top: event.clientY, left: event.clientX});
  }

  private getMenuItemsForBookmarks_(): MenuItem[] {
    const menuItems: MenuItem[] = [
      {
        id: MenuItemId.OPEN_NEW_TAB,
        label: this.bookmarks_.length === 1 ?
            loadTimeData.getString('menuOpenNewTab') :
            loadTimeData.getStringF(
                'menuOpenNewTabWithCount', this.bookmarks_.length),
      },
      {
        id: MenuItemId.OPEN_NEW_WINDOW,
        label: this.bookmarks_.length === 1 ?
            loadTimeData.getString('menuOpenNewWindow') :
            loadTimeData.getStringF(
                'menuOpenNewWindowWithCount', this.bookmarks_.length),
      },
    ];

    if (!loadTimeData.getBoolean('incognitoMode')) {
      menuItems.push({
        id: MenuItemId.OPEN_INCOGNITO,
        label: this.bookmarks_.length === 1 ?
            loadTimeData.getString('menuOpenIncognito') :
            loadTimeData.getStringF(
                'menuOpenIncognitoWithCount', this.bookmarks_.length),
      });
    }

    if (this.bookmarks_.length !== 1 ||
        this.bookmarks_[0]!.id === loadTimeData.getString('bookmarksBarId')) {
      return menuItems;
    }

    if (this.bookmarks_[0]!.url) {
      menuItems.push({
        id: MenuItemId.EDIT,
        label: loadTimeData.getString('menuEdit'),
      });
    }

    if (this.bookmarks_[0]!.parentId ===
        loadTimeData.getString('bookmarksBarId')) {
      menuItems.push({id: MenuItemId.DIVIDER}, {
        id: MenuItemId.REMOVE_FROM_BOOKMARKS_BAR,
        label: loadTimeData.getString('menuMoveToAllBookmarks'),
      });
    } else if (
        this.bookmarks_[0]!.parentId ===
            loadTimeData.getString('otherBookmarksId') ||
        this.bookmarks_[0]!.parentId ===
            loadTimeData.getString('mobileBookmarksId')) {
      menuItems.push({id: MenuItemId.DIVIDER}, {
        id: MenuItemId.ADD_TO_BOOKMARKS_BAR,
        label: loadTimeData.getString('menuMoveToBookmarksBar'),
      });
    }

    if (this.priceTrackingEligible_) {
      menuItems.push(
          {id: MenuItemId.DIVIDER},
          {
            id: MenuItemId.TRACK_PRICE,
            label: this.priceTracked_ ?
                loadTimeData.getString('menuUntrackPrice') :
                loadTimeData.getString('menuTrackPrice'),
          },
      );
    }

    menuItems.push({id: MenuItemId.DIVIDER});

    if (!this.bookmarks_[0]!.url) {
      menuItems.push(
          {
            id: MenuItemId.RENAME,
            label: loadTimeData.getString('menuRename'),
          },
      );
    }

    menuItems.push(
        {
          id: MenuItemId.DELETE,
          label: loadTimeData.getString('tooltipDelete'),
        },
    );

    return menuItems;
  }

  private showDivider_(menuItem: MenuItem): boolean {
    return menuItem.id === MenuItemId.DIVIDER;
  }

  private dispatchDisabledFeatureEvent_() {
    this.dispatchEvent(new CustomEvent('disabled-feature'));
  }

  private onMenuItemClicked_(event: DomRepeatEvent<MenuItem>) {
    event.preventDefault();
    event.stopPropagation();
    switch (event.model.item.id) {
      case MenuItemId.OPEN_NEW_TAB:
        this.bookmarksApi_.contextMenuOpenBookmarkInNewTab(
            this.bookmarks_.map(bookmark => bookmark.id),
            ActionSource.kBookmark);
        break;
      case MenuItemId.OPEN_NEW_WINDOW:
        this.bookmarksApi_.contextMenuOpenBookmarkInNewWindow(
            this.bookmarks_.map(bookmark => bookmark.id),
            ActionSource.kBookmark);
        break;
      case MenuItemId.OPEN_INCOGNITO:
        this.bookmarksApi_.contextMenuOpenBookmarkInIncognitoWindow(
            this.bookmarks_.map(bookmark => bookmark.id),
            ActionSource.kBookmark);
        break;
      // Everything below is not expected to ever be called when
      // this.bookmarks_ has more than one entry.
      case MenuItemId.ADD_TO_BOOKMARKS_BAR:
        if (editingDisabledByPolicy(this.bookmarks_)) {
          this.dispatchDisabledFeatureEvent_();
        } else {
          this.bookmarksApi_.contextMenuAddToBookmarksBar(
              this.bookmarks_[0]!.id, ActionSource.kBookmark);
        }
        break;
      case MenuItemId.REMOVE_FROM_BOOKMARKS_BAR:
        if (editingDisabledByPolicy(this.bookmarks_)) {
          this.dispatchDisabledFeatureEvent_();
        } else {
          this.bookmarksApi_.contextMenuRemoveFromBookmarksBar(
              this.bookmarks_[0]!.id, ActionSource.kBookmark);
        }
        break;
      case MenuItemId.TRACK_PRICE:
        if (editingDisabledByPolicy(this.bookmarks_)) {
          this.dispatchDisabledFeatureEvent_();
        } else {
          if (this.priceTracked_) {
            this.shoppingListApi_.untrackPriceForBookmark(
                BigInt(this.bookmarks_[0]!.id));
            chrome.metricsPrivate.recordUserAction(
                'Commerce.PriceTracking.SidePanel.Untrack.ContextMenu');
          } else {
            this.shoppingListApi_.trackPriceForBookmark(
                BigInt(this.bookmarks_[0]!.id));
            chrome.metricsPrivate.recordUserAction(
                'Commerce.PriceTracking.SidePanel.Track.ContextMenu');
          }
        }
        break;
      case MenuItemId.EDIT:
        this.dispatchEvent(new CustomEvent('edit-clicked', {
          bubbles: true,
          composed: true,
          detail: {
            id: this.bookmarks_[0]!.id,
          },
        }));
        break;
      case MenuItemId.RENAME:
        if (editingDisabledByPolicy(this.bookmarks_)) {
          this.dispatchDisabledFeatureEvent_();
        } else {
          this.dispatchEvent(new CustomEvent('rename-clicked', {
            bubbles: true,
            composed: true,
            detail: {
              id: this.bookmarks_[0]!.id,
            },
          }));
        }
        break;
      case MenuItemId.DELETE:
        if (editingDisabledByPolicy(this.bookmarks_)) {
          this.dispatchDisabledFeatureEvent_();
        } else {
          this.bookmarksApi_.contextMenuDelete(
              this.bookmarks_[0]!.id, ActionSource.kBookmark);
          this.dispatchEvent(new CustomEvent('delete-clicked', {
            bubbles: true,
            composed: true,
            detail: {
              id: this.bookmarks_[0]!.id,
            },
          }));
        }
        break;
    }
    this.$.menu.close();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'power-bookmarks-context-menu': PowerBookmarksContextMenuElement;
  }
}

customElements.define(
    PowerBookmarksContextMenuElement.is, PowerBookmarksContextMenuElement);
