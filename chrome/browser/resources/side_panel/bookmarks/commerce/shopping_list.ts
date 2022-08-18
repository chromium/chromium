// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';
import 'chrome://resources/cr_elements/shared_vars_css.m.js';
import 'chrome://resources/cr_elements/mwb_element_shared_style.css.js';
import 'chrome://resources/cr_elements/cr_auto_img/cr_auto_img.js';
import './icons.html.js';

import {getFaviconForPageURL} from 'chrome://resources/js/icon.js';
import {DomRepeatEvent, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {ActionSource} from '../bookmarks.mojom-webui.js';
import {BookmarksApiProxy, BookmarksApiProxyImpl} from '../bookmarks_api_proxy.js';

import {getTemplate} from './shopping_list.html.js';
import {BookmarkProductInfo} from './shopping_list.mojom-webui.js';

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

      productInfos: Array,
    };
  }

  productInfos: BookmarkProductInfo[];
  private open_: boolean;
  private bookmarksApi_: BookmarksApiProxy =
      BookmarksApiProxyImpl.getInstance();

  private getFaviconUrl_(url: string): string {
    return getFaviconForPageURL(url, false);
  }

  private onFolderClick_(event: Event) {
    event.preventDefault();
    event.stopPropagation();

    this.open_ = !this.open_;
  }

  private onProductAuxClick_(
      event: DomRepeatEvent<BookmarkProductInfo, MouseEvent>) {
    if (event.button !== 1) {
      // Not a middle click.
      return;
    }

    event.preventDefault();
    event.stopPropagation();
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
}

declare global {
  interface HTMLElementTagNameMap {
    'shopping-list': ShoppingListElement;
  }
}

customElements.define(ShoppingListElement.is, ShoppingListElement);
