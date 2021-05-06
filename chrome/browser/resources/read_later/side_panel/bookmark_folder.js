// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

class BookmarkFolder extends PolymerElement {
  static get is() {
    return 'bookmark-folder';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      /** @private {!chrome.bookmarks.BookmarkTreeNode} */
      folder: Object,
    };
  }
}

customElements.define(BookmarkFolder.is, BookmarkFolder);