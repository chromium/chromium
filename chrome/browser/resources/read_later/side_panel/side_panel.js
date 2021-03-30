// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '../read_later_item.js';
import 'chrome://resources/cr_elements/hidden_style_css.m.js';
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
      <style include="mwb-shared-style cr-hidden-style">
        #header {
          align-items: center;
          display: flex;
          padding: 16px 0 4px 8px;
        }

        #searchContainer {
          background: #f1f3f4;
          border-radius: 8px;
          display: flex;
          flex: 1;
          height: 36px;
        }

        cr-icon-button {
          margin: 0;
        }

        input {
          appearance: none;
          background: transparent;
          border: 0;
          display: block;
          flex: 1;
          height: 100%;
          line-height: 36px;
          outline: none;
        }

        #tabs {
          display: flex;
          height: 48px;
          position: relative;
          width: 100%;
        }

        #tabs a {
          cursor: pointer;
          display: block;
          flex: 1;
          line-height: 48px;
          text-align: center;
        }

        #tabs span {
          position: relative;
        }

        #tabs a[active] span::after {
          content: '';
          display: block;
          background: #1A73E8;
          bottom: -8px;
          border-radius: 3px 3px 0 0;
          height: 3px;
          left: 2px;
          position: absolute;
          right: 2px;
        }

        #tabsBorder {
          background: #dadce0;
          bottom: 8px;
          height: 1px;
          position: absolute;
          width: 100%;
        }

        .tab {
          display: none;
        }

        .tab[active] {
          display: block;
        }

        h2 {
          font-size: 11px;
          font-weight: normal;
          padding: 16px 16px 8px 16px;
          text-transform: uppercase;
        }
      </style>

      <div id="header">
        <div id="searchContainer">
          <cr-icon-button iron-icon="cr:search"></cr-icon-button>
          <input value="{{searchQuery_::input}}">
        </div>
        <cr-icon-button iron-icon="cr:close" on-click="close_"></cr-icon-button>
      </div>

      <div id="tabs">
        <template is="dom-repeat" items="[[tabs_]]">
          <a on-click="activateTab_"
              active$="[[isActiveTab_(item.id, activeTab_)]]">
            <span>[[item.label]]</span>
          </a>
        </template>
        <div id="tabsBorder"></div>
      </div>

      <div id="readingList" class="tab"
          active$="[[isActiveTab_('readingList', activeTab_)]]">
        <h2>Unread</h2>
        <template is="dom-repeat" items="[[unreadItems_]]">
          <read-later-item class="mwb-list-item" data="[[item]]"
              hidden$="[[shouldHideItem_(item.title, searchQuery_)]]">
          </read-later-item>
        </template>
        <h2>Read</h2>
        <template is="dom-repeat" items="[[readItems_]]">
          <read-later-item class="mwb-list-item" data="[[item]]"
            hidden$="[[shouldHideItem_(item.title, searchQuery_)]]">
          </read-later-item>
        </template>
      </div>

      <div id="bookmarks" class="tab"
          active$="[[isActiveTab_('bookmarks', activeTab_)]]">
        <template is="dom-repeat" items="[[bookmarkFolders_]]">
          <bookmark-folder folder="[[item]]"
              search-query="[[searchQuery_]]"></bookmark-folder>
        </template>
      </div>
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
      tabs_: {
        type: Array,
        value: () => {
          return [
            {
              id: 'readingList',
              label: 'Reading list',
            },
            {
              id: 'bookmarks',
              label: 'Bookmarks',
            },
          ];
        },
      },
      activeTab_: {
        type: Object,
        value: () => {},
      },
      searchQuery_: {
        type: String,
        value: '',
      }
    };
  }

  connectedCallback() {
    super.connectedCallback();
    this.activeTab_ = this.tabs_[0];
    readLaterApi.getReadLaterEntries().then(({entries}) => {
      this.unreadItems_ = entries.unreadEntries;
      this.readItems_ = entries.readEntries;
    });
    readLaterApi.getCallbackRouter().itemsChanged.addListener(
        items => this.onReadLaterItemsUpdated_(items));
    chrome.bookmarks.getTree(([{children}]) => {
      this.bookmarkFolders_ = children;
    });
  }

  activateTab_(event) {
    this.activeTab_ = event.model.item;
  }

  close_() {
    readLaterApi.closeUI();
  }

  isActiveTab_(tabId) {
    return this.activeTab_.id === tabId;
  }

  onReadLaterItemsUpdated_(items) {
    this.unreadItems_ = items.unreadEntries;
    this.readItems_ = items.readEntries;
  }

  shouldHideItem_(title) {
    if (this.searchQuery_.length === 0) {
      return false;
    }

    return title.toLowerCase().indexOf(this.searchQuery_.toLowerCase()) === -1;
  }
}
customElements.define(SidePanel.is, SidePanel);

class BookmarkFolder extends PolymerElement {
  static get is() {
    return 'bookmark-folder';
  }

  static get template() {
    return html`
      <style include="cr-hidden-style">
      </style>
      <template is="dom-repeat" items="[[folder.children]]">
        <template is="dom-if" if="[[item.children]]">
          <bookmark-folder folder="[[item]]" search-query="[[searchQuery]]">
          </bookmark-folder>
        </template>
        <template is="dom-if" if="[[!item.children]]">
          <bookmark-item item="[[item]]"
              hidden$="[[shouldHideItem_(item.title, searchQuery)]]">
          </bookmark-item>
        </template>
      </template>
    `;
  }

  static get properties() {
    return {
      folder: {
        type: Object,
        value: () => {},
      },
      searchQuery: {
        type: String,
        value: () => '',
      },
    };
  }

  shouldHideItem_(title) {
    if (this.searchQuery.length === 0) {
      return false;
    }

    return title.toLowerCase().indexOf(this.searchQuery.toLowerCase()) === -1;
  }
}
customElements.define(BookmarkFolder.is, BookmarkFolder);

class BookmarkItem extends PolymerElement {
  static get is() {
    return 'bookmark-item';
  }

  static get template() {
    return html`
      <style>
        :host {
          align-items: center;
          cursor: pointer;
          display: flex;
          height: 40px;
          line-height: 40px;
          overflow: hidden;
          padding: 0 16px;
          white-space: nowrap;
        }

        :host(:hover) {
          background: var(--mwb-list-item-hover-background-color);
        }

        #favicon {
          width: 16px;
          height: 16px;
          margin-right: 16px;
        }

        #title {
          overflow: hidden;
          text-overflow: ellipsis;
        }
      </style>

      <img id="favicon" src="chrome://favicon/[[item.url]]">
      <div id="title">[[item.title]]</div>
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

  constructor() {
    super();
    this.addEventListener('click', () => this.onClick_());
  }

  onClick_() {
    chrome.tabs.create({url: this.item.url});
  }
}
customElements.define(BookmarkItem.is, BookmarkItem);
