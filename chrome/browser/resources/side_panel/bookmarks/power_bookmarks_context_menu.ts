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
      bookmark_: Object,

      depth_: Number,
    };
  }

  private bookmarksApi_: BookmarksApiProxy =
      BookmarksApiProxyImpl.getInstance();
  private bookmark_: chrome.bookmarks.BookmarkTreeNode;
  private depth_: number;

  showAt(
      event: MouseEvent, bookmark: chrome.bookmarks.BookmarkTreeNode,
      depth: number) {
    this.bookmark_ = bookmark;
    this.depth_ = depth;
    this.$.menu.showAt(event.target as HTMLElement);
  }

  showAtPosition(
      event: MouseEvent, bookmark: chrome.bookmarks.BookmarkTreeNode,
      depth: number) {
    this.bookmark_ = bookmark;
    this.depth_ = depth;
    this.$.menu.showAtPosition({top: event.clientY, left: event.clientX});
  }

  private getMenuItemsForBookmark_(): MenuItem[] {
    const menuItems: MenuItem[] = [
      {
        id: MenuItemId.OPEN_NEW_TAB,
        label: loadTimeData.getString('menuOpenNewTab'),
      },
      {
        id: MenuItemId.OPEN_NEW_WINDOW,
        label: loadTimeData.getString('menuOpenNewWindow'),
      },
    ];

    if (!loadTimeData.getBoolean('incognitoMode')) {
      menuItems.push({
        id: MenuItemId.OPEN_INCOGNITO,
        label: loadTimeData.getString('menuOpenIncognito'),
      });
    }

    menuItems.push(
        {id: MenuItemId.DIVIDER},
        {id: MenuItemId.DELETE, label: loadTimeData.getString('tooltipDelete')},
    );

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
            this.bookmark_!.id, ActionSource.kBookmark);
        break;
      case MenuItemId.OPEN_NEW_WINDOW:
        this.bookmarksApi_.contextMenuOpenBookmarkInNewWindow(
            this.bookmark_!.id, ActionSource.kBookmark);
        break;
      case MenuItemId.OPEN_INCOGNITO:
        this.bookmarksApi_.contextMenuOpenBookmarkInIncognitoWindow(
            this.bookmark_!.id, ActionSource.kBookmark);
        break;
      case MenuItemId.DELETE:
        this.bookmarksApi_.contextMenuDelete(
            this.bookmark_!.id, ActionSource.kBookmark);
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
