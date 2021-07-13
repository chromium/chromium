// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

/**
 * @fileoverview
 * 'app-notification-row' is a custom row element for the OS Settings
 * Notifications Subpage. Each row contains an app icon, app name, and toggle.
 */
export class AppNotificationRowElement extends PolymerElement {
  static get is() {
    return 'app-notification-row';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      /** @type {!Object} */
      app: {
        type: Object,
      },
    };
  }
}

customElements.define(AppNotificationRowElement.is, AppNotificationRowElement);