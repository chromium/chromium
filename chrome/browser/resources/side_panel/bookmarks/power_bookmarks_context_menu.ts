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

      menuItems_: {
        type: Array,
        value: () => [loadTimeData.getString('menuOpenNewTab')],
      },
    };
  }

  private bookmarksApi_: BookmarksApiProxy =
      BookmarksApiProxyImpl.getInstance();
  private bookmark_: chrome.bookmarks.BookmarkTreeNode;
  private depth_: number;
  private menuItems_: string[];

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

  private onMenuItemClicked_(event: DomRepeatEvent<string>) {
    event.preventDefault();
    event.stopPropagation();
    switch (event.model.index) {
      case 0:
        // Open in new tab
        this.bookmarksApi_.openBookmark(
            this.bookmark_!.id, this.depth_, {
              middleButton: true,
              altKey: false,
              ctrlKey: false,
              metaKey: false,
              shiftKey: false,
            },
            ActionSource.kBookmark);
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
