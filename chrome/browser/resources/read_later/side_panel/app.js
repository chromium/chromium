// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// TODO(crbug.com/1199527): The current structure of c/b/r/read_later/* is
// only temporary. Eventually, this side_panel directory should become the main
// directory, with read_later being moved into a subdirectory within side_panel.

import 'chrome://resources/cr_elements/cr_tabs/cr_tabs.js';
import 'chrome://resources/polymer/v3_0/iron-pages/iron-pages.js';
import './bookmarks_list.js';
import '../app.js'; /* <read-later-app> */
import '../strings.m.js';

import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

/**
 * Key for localStorage object that refers to the last active tab's ID.
 * @const {string}
 */
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
      /** @private {!Object<string, string>} */
      tabs_: {
        type: Object,
        value: () => ({
          'readingList': loadTimeData.getString('title'),
          'bookmarks': loadTimeData.getString('bookmarksTabTitle'),
        }),
      },

      /** @private {number} */
      selectedTab_: {
        type: Number,
        value: 0,
      },
    };
  }

  connectedCallback() {
    super.connectedCallback();
    const lastActiveTab = window.localStorage[LOCAL_STORAGE_TAB_ID_KEY];
    if (lastActiveTab) {
      this.selectedTab_ = Object.keys(this.tabs_).indexOf(lastActiveTab) || 0;
    }
  }

  /**
   * @return {!Array<string>}
   * @private
   */
  getTabNames_() {
    return Object.values(this.tabs_);
  }

  /**
   * @param {!Event} event
   * @private
   */
  onSelectedTabChanged_(event) {
    const tabIndex = event.detail.value;
    window.localStorage[LOCAL_STORAGE_TAB_ID_KEY] =
        Object.keys(this.tabs_)[tabIndex];
  }
}
customElements.define(SidePanelAppElement.is, SidePanelAppElement);
