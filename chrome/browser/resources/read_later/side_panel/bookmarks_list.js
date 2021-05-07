// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './bookmark_folder.js';

import {html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {BookmarksApiProxy, BookmarksApiProxyImpl} from './bookmarks_api_proxy.js';

export class BookmarksListElement extends PolymerElement {
  static get is() {
    return 'bookmarks-list';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      /** @private {!Array<!chrome.bookmarks.BookmarkTreeNode>} */
      folders_: {
        type: Array,
        value: [],
      },
    };
  }

  constructor() {
    super();

    /** @private @const {!BookmarksApiProxy} */
    this.bookmarksApi_ = BookmarksApiProxyImpl.getInstance();
  }

  connectedCallback() {
    super.connectedCallback();
    this.bookmarksApi_.getFolders().then(folders => {
      this.folders_ = folders;
    });
  }
}

customElements.define(BookmarksListElement.is, BookmarksListElement);