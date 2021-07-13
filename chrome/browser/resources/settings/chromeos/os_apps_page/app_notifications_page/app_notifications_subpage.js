// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './app_notification_row.js';
import {html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

/**
 * @fileoverview
 * 'app-notifications-page' is responsible for containing controls for
 * notifications of all apps.
 * TODO(ethanimooney): Implement this skeleton element.
 */
export class AppNotificationsSubpage extends PolymerElement {
  static get is() {
    return 'settings-app-notifications-subpage';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      /**
       * @type {!Array<!Object>}
       * @private
       */
      appList_: {
        type: Array,
        // TODO(ethanimooney): Replace placeholders with proper implementation
        // for apps
        value: [
          {title: 'Chrome', id: 'mgndgikekgjfcpckkfioiadnlibdjbkf'},
          {title: 'Files', id: 'hhaomjibdihmijegdhdafkllkbggdgoj'}
        ],
      },
    };
  }
}

customElements.define(AppNotificationsSubpage.is, AppNotificationsSubpage);