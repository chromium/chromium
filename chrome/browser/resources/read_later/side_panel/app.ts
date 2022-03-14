// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// TODO(crbug.com/1199527): The current structure of c/b/r/read_later/* is
// only temporary. Eventually, this side_panel directory should become the main
// directory, with read_later being moved into a subdirectory within side_panel.

import 'chrome://resources/cr_elements/cr_tabs/cr_tabs.js';
import 'chrome://resources/cr_elements/shared_vars_css.m.js';
import 'chrome://resources/polymer/v3_0/iron-pages/iron-pages.js';
import '../app.js'; /* <read-later-app> */
import '../strings.m.js';
import './bookmarks_list.js';
import './reader_mode/app.js'; /* <reader-mode-app> */

import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {ReadLaterApiProxy, ReadLaterApiProxyImpl} from '../read_later_api_proxy.js';

// Key for localStorage object that refers to the last active tab's ID.
export const LOCAL_STORAGE_TAB_ID_KEY = 'lastActiveTab';

export class SidePanelAppElement extends PolymerElement {
  static get is() {
    return 'side-panel-app';
  }

  static get template() {
    return html`{__html_template__}`;
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

  private apiProxy_: ReadLaterApiProxy = ReadLaterApiProxyImpl.getInstance();
  private selectedTab_: number;
  private tabs_: {[key: string]: string};

  override ready() {
    if (loadTimeData.getBoolean('readerModeSidePanelEnabled')) {
      this.tabs_['readerMode'] = loadTimeData.getString('readerModeTabTitle');
    }
    super.ready();
  }

  override connectedCallback() {
    super.connectedCallback();
    const lastActiveTab = window.localStorage[LOCAL_STORAGE_TAB_ID_KEY];
    if (loadTimeData.getBoolean('hasUnseenReadingListEntries')) {
      window.localStorage[LOCAL_STORAGE_TAB_ID_KEY] = 'readingList';
    } else if (lastActiveTab) {
      this.selectedTab_ = Object.keys(this.tabs_).indexOf(lastActiveTab) || 0;
    }

    // Show the UI as soon as the app is connected.
    this.apiProxy_.showUI();
  }

  private getTabNames_(): string[] {
    return Object.values(this.tabs_);
  }

  private isSelectedTab_(selectedTab: number, index: number): boolean {
    return selectedTab === index;
  }

  private onCloseClick_() {
    this.apiProxy_.closeUI();
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
