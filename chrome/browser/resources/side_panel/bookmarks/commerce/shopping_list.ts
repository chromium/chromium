// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';
import 'chrome://resources/cr_elements/shared_vars_css.m.js';
import 'chrome://resources/cr_elements/mwb_element_shared_style.css.js';
import 'chrome://resources/cr_elements/cr_auto_img/cr_auto_img.js';
import './icons.html.js';

import {getFaviconForPageURL} from 'chrome://resources/js/icon.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

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

  private getFaviconUrl_(url: string): string {
    return getFaviconForPageURL(url, false);
  }

  private onFolderClick_(event: Event) {
    event.preventDefault();
    event.stopPropagation();

    this.open_ = !this.open_;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'shopping-list': ShoppingListElement;
  }
}

customElements.define(ShoppingListElement.is, ShoppingListElement);
