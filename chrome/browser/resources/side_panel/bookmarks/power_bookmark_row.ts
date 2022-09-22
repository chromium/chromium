// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_shared_style.css.js';

import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './power_bookmark_row.html.js';

export class PowerBookmarkRowElement extends PolymerElement {
  static get is() {
    return 'power-bookmark-row';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      bookmark: Object,
    };
  }

  bookmark: chrome.bookmarks.BookmarkTreeNode;
}

declare global {
  interface HTMLElementTagNameMap {
    'power-bookmark-row': PowerBookmarkRowElement;
  }
}

customElements.define(PowerBookmarkRowElement.is, PowerBookmarkRowElement);
