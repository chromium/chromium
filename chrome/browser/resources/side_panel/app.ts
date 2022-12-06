// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_tabs/cr_tabs.js';
import 'chrome://resources/cr_elements/cr_shared_vars.css.js';
import 'chrome://resources/polymer/v3_0/iron-pages/iron-pages.js';
import './reading_list/app.js'; /* <reading-list-app> */
import '../strings.m.js';
import './bookmarks/bookmarks_list.js';
import './bookmarks/power_bookmarks_list.js';
import './read_anything/app.js'; /* <read-anything-app> */

import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './app.html.js';
import {ReadingListApiProxy, ReadingListApiProxyImpl} from './reading_list/reading_list_api_proxy.js';

// Key for localStorage object that refers to the last active tab's ID.
export const LOCAL_STORAGE_TAB_ID_KEY = 'lastActiveTab';

export class SidePanelAppElement extends PolymerElement {
  static get is() {
    return 'side-panel-app';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      tabs_: {
        type: Object,
        value: () => ({
          'readingList': loadTimeData.getString('title'),
          'bookmarks': loadTimeData.getString('bookmarksTabTitle'),
        }),
      },

      selectedTab_: {
        type: Number,
        value: 0,
      },
    };
  }

  private apiProxy_: ReadingListApiProxy =
      ReadingListApiProxyImpl.getInstance();
  private selectedTab_: number;
  private tabs_: {[key: string]: string};

  override ready() {
    if (loadTimeData.getBoolean('readAnythingEnabled')) {
      this.tabs_['readAnything'] =
          loadTimeData.getString('readAnythingTabTitle');
    }
    super.ready();
  }

  override connectedCallback() {
    super.connectedCallback();
    const lastActiveTab = window.localStorage[LOCAL_STORAGE_TAB_ID_KEY];
    if (loadTimeData.getBoolean('shouldShowBookmark')) {
      window.localStorage[LOCAL_STORAGE_TAB_ID_KEY] = 'bookmarks';
      this.selectedTab_ = Object.keys(this.tabs_).indexOf('bookmarks');
    } else if (loadTimeData.getBoolean('hasUnseenReadingListEntries')) {
      window.localStorage[LOCAL_STORAGE_TAB_ID_KEY] = 'readingList';
    } else if (lastActiveTab) {
      this.selectedTab_ = Object.keys(this.tabs_).indexOf(lastActiveTab) || 0;
    }

    // Show the UI as soon as the app is connected.
    this.apiProxy_.showUi();
  }

  private getTabNames_(): string[] {
    return Object.values(this.tabs_);
  }

  private isSelectedTab_(selectedTab: number, index: number): boolean {
    return selectedTab === index;
  }

  private onCloseClick_() {
    this.apiProxy_.closeUi();
  }

  private onSelectedTabChanged_(event: CustomEvent<{value: number}>) {
    const tabIndex = event.detail.value;
    window.localStorage[LOCAL_STORAGE_TAB_ID_KEY] =
        Object.keys(this.tabs_)[tabIndex];
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'side-panel-app': SidePanelAppElement;
  }
}

customElements.define(SidePanelAppElement.is, SidePanelAppElement);
