// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '../read_later_item.js';
import 'chrome://resources/cr_elements/mwb_shared_style.js';

import {html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {ReadLaterApiProxyImpl} from '../read_later_api_proxy.js';

const readLaterApi = ReadLaterApiProxyImpl.getInstance();

class SidePanel extends PolymerElement {
  static get is() {
    return 'side-panel';
  }

  static get template() {
    return html`
      <style include="mwb-shared-style"></style>

      <h2>Bookmarks</h2>
      <template is="dom-repeat" items="[[bookmarkFolders_]]">
        <bookmark-folder folder="[[item]]"></bookmark-folder>
      </template>

      <h2>Unread items</h2>
      <template is="dom-repeat" items="[[unreadItems_]]">
        <read-later-item class="mwb-list-item" data="[[item]]">
        </read-later-item>
      </template>

      <h2>Read items</h2>
      <template is="dom-repeat" items="[[readItems_]]">
        <read-later-item class="mwb-list-item" data="[[item]]">
        </read-later-item>
      </template>
    `;
  }

  static get properties() {
    return {
      unreadItems_: {
        type: Array,
        value: () => [],
      },
      readItems_: {
        type: Array,
        value: () => [],
      },
      bookmarkFolders_: {
        type: Array,
        value: () => [],
      },
    };
  }

  connectedCallback() {
    super.connectedCallback();
    readLaterApi.getReadLaterEntries().then(({entries}) => {
      this.unreadItems_ = entries.unreadEntries;
      this.readItems_ = entries.readEntries;
    });
    chrome.bookmarks.getTree(([{children}]) => {
      this.bookmarkFolders_ = children;
    });
  }
}
customElements.define(SidePanel.is, SidePanel);

class BookmarkFolder extends PolymerElement {
  static get is() {
    return 'bookmark-folder';
  }

  static get template() {
    return html`
      <div><b>[[folder.title]]</b></div>
      <template is="dom-repeat" items="[[folder.children]]">
        <bookmark-item item="[[item]]"></bookmark-item>
      </template>
    `;
  }

  static get properties() {
    return {
      folder: {
        type: Object,
        value: () => {},
      },
    };
  }
}
customElements.define(BookmarkFolder.is, BookmarkFolder);

class BookmarkItem extends PolymerElement {
  static get is() {
    return 'bookmark-item';
  }

  static get template() {
    return html`
      <img src="chrome://favicon/[[item.url]]">
      <div>[[item.title]]</div>
    `;
  }

  static get properties() {
    return {
      item: {
        type: Object,
        value: () => {},
      },
    };
  }
}
customElements.define(BookmarkItem.is, BookmarkItem);