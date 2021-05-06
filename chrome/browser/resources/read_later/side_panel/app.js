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

class SidePanel extends PolymerElement {
  static get is() {
    return 'side-panel-app';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      /** @private {!Array<string>} */
      tabs_: {
        type: Array,
        value: () => ([
          'title',
          'bookmarksTabTitle',
        ].map(id => loadTimeData.getString(id))),
      },

      /** @private {number} */
      selectedTab_: {
        type: Number,
        value: 0,
      },
    };
  }
}
customElements.define(SidePanel.is, SidePanel);
