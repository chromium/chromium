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
import {getTemplate} from './power_bookmarks_context_menu.html.js';

export interface PowerBookmarksContextMenuElement {
  $: {
    menu: CrActionMenuElement,
  };
}

export enum MenuItemId {
  OPEN_NEW_TAB = 0,
  OPEN_NEW_WINDOW = 1,
  OPEN_INCOGNITO = 2,
  DELETE = 3,
  DIVIDER = 4,
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
  private bookmarks_: chrome.bookmarks.BookmarkTreeNode[];

  showAt(event: MouseEvent, bookmarks: chrome.bookmarks.BookmarkTreeNode[]) {
    this.bookmarks_ = bookmarks;
    this.$.menu.showAt(event.target as HTMLElement);
  }

  showAtPosition(
      event: MouseEvent, bookmarks: chrome.bookmarks.BookmarkTreeNode[]) {
    this.bookmarks_ = bookmarks;
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

    if (this.bookmarks_.length === 1 &&
        this.bookmarks_[0]!.id !== loadTimeData.getString('bookmarksBarId')) {
      menuItems.push(
          {id: MenuItemId.DIVIDER},
          {
            id: MenuItemId.DELETE,
            label: loadTimeData.getString('tooltipDelete'),
          },
      );
    }

    return menuItems;
  }

  private showDivider_(menuItem: MenuItem): boolean {
    return menuItem.id === MenuItemId.DIVIDER;
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
      case MenuItemId.DELETE:
        // Not expected to ever be called when this.bookmarks_ has more than
        // one entry.
        this.bookmarksApi_.contextMenuDelete(
            this.bookmarks_[0]!.id, ActionSource.kBookmark);
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
