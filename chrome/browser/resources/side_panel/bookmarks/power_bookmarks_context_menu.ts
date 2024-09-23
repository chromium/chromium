// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '../strings.m.js';
import './icons.html.js';
import '//bookmarks-side-panel.top-chrome/shared/sp_shared_style.css.js';
import '//resources/cr_elements/cr_action_menu/cr_action_menu.js';
import '//resources/cr_elements/cr_icon_button/cr_icon_button.js';
import '//resources/cr_elements/icons_lit.html.js';

import type {BrowserProxy} from '//resources/cr_components/commerce/browser_proxy.js';
import {BrowserProxyImpl} from '//resources/cr_components/commerce/browser_proxy.js';
import type {CrActionMenuElement} from '//resources/cr_elements/cr_action_menu/cr_action_menu.js';
import {assert} from 'chrome://resources/js/assert.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import type {DomRepeatEvent} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {afterNextRender, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {ActionSource} from './bookmarks.mojom-webui.js';
import type {BookmarksApiProxy} from './bookmarks_api_proxy.js';
import {BookmarksApiProxyImpl} from './bookmarks_api_proxy.js';
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
  OPEN_NEW_TAB_GROUP = 3,
  EDIT = 4,
  ADD_TO_BOOKMARKS_BAR = 5,
  REMOVE_FROM_BOOKMARKS_BAR = 6,
  TRACK_PRICE = 7,
  RENAME = 8,
  DELETE = 9,
  DIVIDER = 10,
}

export interface MenuItem {
  id: MenuItemId;
  label?: string;
  disabled?: boolean;
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
  private shoppingServiceApi_: BrowserProxy = BrowserProxyImpl.getInstance();
  private bookmarks_: chrome.bookmarks.BookmarkTreeNode[] = [];
  private priceTracked_: boolean;
  private priceTrackingEligible_: boolean;

  showAt(
      event: MouseEvent, bookmarks: chrome.bookmarks.BookmarkTreeNode[],
      priceTracked: boolean, priceTrackingEligible: boolean,
      onShown: Function = () => {}) {
    this.bookmarks_ = bookmarks;
    this.priceTracked_ = priceTracked;
    this.priceTrackingEligible_ = priceTrackingEligible;
    const target = event.target as HTMLElement;
    afterNextRender(this, () => {
      this.$.menu.showAt(target);
      onShown();
    });
  }

  showAtPosition(
      event: MouseEvent, bookmarks: chrome.bookmarks.BookmarkTreeNode[],
      priceTracked: boolean, priceTrackingEligible: boolean,
      onShown: Function = () => {}) {
    this.bookmarks_ = bookmarks;
    this.priceTracked_ = priceTracked;
    this.priceTrackingEligible_ = priceTrackingEligible;
    const menuMargin = 20;
    const doc = document.scrollingElement!;
    const minX = doc.scrollLeft + menuMargin;
    const maxX = doc.scrollLeft + doc.clientWidth - menuMargin;
    afterNextRender(this, () => {
      this.$.menu.showAtPosition({
        top: event.clientY,
        left: event.clientX,
        minX: minX,
        maxX: maxX,
      });
      onShown();
    });
  }

  isOpen(): boolean {
    return this.$.menu.open;
  }

  private getMenuItemsForBookmarks_(): MenuItem[] {
    // TODO(crbug.com/40262319): Factor in URLs not available in incognito.
    let bookmarkCount = 0;
    this.bookmarks_.forEach((bookmark) => {
      if (bookmark.url) {
        bookmarkCount += 1;
      } else if (bookmark.children) {
        bookmarkCount +=
            bookmark.children.filter((child) => !!child.url).length;
      }
    });
    const menuItems: MenuItem[] = [
      {
        id: MenuItemId.OPEN_NEW_TAB,
        label: bookmarkCount < 2 ?
            loadTimeData.getString('menuOpenNewTab') :
            loadTimeData.getStringF('menuOpenNewTabWithCount', bookmarkCount),
        disabled: bookmarkCount === 0,
      },
      {
        id: MenuItemId.OPEN_NEW_WINDOW,
        label: bookmarkCount < 2 ?
            loadTimeData.getString('menuOpenNewWindow') :
            loadTimeData.getStringF(
                'menuOpenNewWindowWithCount', bookmarkCount),
        disabled: bookmarkCount === 0,
      },
    ];

    if (!loadTimeData.getBoolean('incognitoMode') &&
        loadTimeData.getBoolean('isIncognitoModeAvailable')) {
      menuItems.push({
        id: MenuItemId.OPEN_INCOGNITO,
        label: bookmarkCount < 2 ?
            loadTimeData.getString('menuOpenIncognito') :
            loadTimeData.getStringF(
                'menuOpenIncognitoWithCount', bookmarkCount),
        disabled: bookmarkCount === 0,
      });
    }

    if (this.bookmarks_.length !== 1 || !this.bookmarks_[0]!.url) {
      menuItems.push({
        id: MenuItemId.OPEN_NEW_TAB_GROUP,
        label: bookmarkCount < 2 ?
            loadTimeData.getString('menuOpenNewTabGroup') :
            loadTimeData.getStringF(
                'menuOpenNewTabGroupWithCount', bookmarkCount),
        disabled: bookmarkCount === 0,
      });
    }

    if (this.bookmarks_.length !== 1) {
      menuItems.push(
          {id: MenuItemId.DIVIDER},
          {
            id: MenuItemId.EDIT,
            label: loadTimeData.getString('tooltipMove'),
          },
          {id: MenuItemId.DIVIDER},
          {
            id: MenuItemId.DELETE,
            label: loadTimeData.getString('tooltipDelete'),
          },
      );
      return menuItems;
    } else if (
        this.bookmarks_[0]!.id === loadTimeData.getString('bookmarksBarId')) {
      return menuItems;
    }

    if (this.bookmarks_[0]!.url ||
        this.bookmarks_[0]!.parentId ===
            loadTimeData.getString('bookmarksBarId') ||
        this.bookmarks_[0]!.parentId ===
            loadTimeData.getString('otherBookmarksId') ||
        this.bookmarks_[0]!.parentId ===
            loadTimeData.getString('mobileBookmarksId')) {
      menuItems.push({id: MenuItemId.DIVIDER});
    }

    if (this.bookmarks_[0]!.url) {
      menuItems.push({
        id: MenuItemId.EDIT,
        label: loadTimeData.getString('menuEdit'),
      });
    }

    if (this.bookmarks_[0]!.parentId ===
        loadTimeData.getString('bookmarksBarId')) {
      menuItems.push({
        id: MenuItemId.REMOVE_FROM_BOOKMARKS_BAR,
        label: loadTimeData.getString('menuMoveToAllBookmarks'),
      });
    } else if (
        this.bookmarks_[0]!.parentId ===
            loadTimeData.getString('otherBookmarksId') ||
        this.bookmarks_[0]!.parentId ===
            loadTimeData.getString('mobileBookmarksId')) {
      menuItems.push({
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

  /**
   * Close the menu on mousedown so clicks can propagate to the underlying UI.
   * This allows the user to right click the list while a context menu is
   * showing and get another context menu.
   */
  private onMousedown_(e: Event): void {
    if ((e.composedPath()[0] as HTMLElement).tagName !== 'DIALOG') {
      return;
    }

    this.$.menu.close();
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
      case MenuItemId.OPEN_NEW_TAB_GROUP:
        this.bookmarksApi_.contextMenuOpenBookmarkInNewTabGroup(
            this.bookmarks_.map(bookmark => bookmark.id),
            ActionSource.kBookmark);
        break;
      case MenuItemId.ADD_TO_BOOKMARKS_BAR:
        assert(this.bookmarks_.length === 1);
        if (editingDisabledByPolicy(this.bookmarks_)) {
          this.dispatchDisabledFeatureEvent_();
        } else {
          this.bookmarksApi_.contextMenuAddToBookmarksBar(
              this.bookmarks_[0]!.id, ActionSource.kBookmark);
        }
        break;
      case MenuItemId.REMOVE_FROM_BOOKMARKS_BAR:
        assert(this.bookmarks_.length === 1);
        if (editingDisabledByPolicy(this.bookmarks_)) {
          this.dispatchDisabledFeatureEvent_();
        } else {
          this.bookmarksApi_.contextMenuRemoveFromBookmarksBar(
              this.bookmarks_[0]!.id, ActionSource.kBookmark);
        }
        break;
      case MenuItemId.TRACK_PRICE:
        assert(this.bookmarks_.length === 1);
        if (editingDisabledByPolicy(this.bookmarks_)) {
          this.dispatchDisabledFeatureEvent_();
        } else {
          if (this.priceTracked_) {
            this.shoppingServiceApi_.untrackPriceForBookmark(
                BigInt(this.bookmarks_[0]!.id));
            chrome.metricsPrivate.recordUserAction(
                'Commerce.PriceTracking.SidePanel.Untrack.ContextMenu');
          } else {
            this.shoppingServiceApi_.trackPriceForBookmark(
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
            bookmarks: this.bookmarks_,
          },
        }));
        break;
      case MenuItemId.RENAME:
        assert(this.bookmarks_.length === 1);
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
              this.bookmarks_.map(bookmark => bookmark.id),
              ActionSource.kBookmark);
          this.dispatchEvent(new CustomEvent('delete-clicked', {
            bubbles: true,
            composed: true,
            detail: {
              bookmarks: this.bookmarks_,
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
