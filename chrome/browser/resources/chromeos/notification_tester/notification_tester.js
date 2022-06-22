// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

export class NotificationTester extends PolymerElement {
  static get is() {
    return 'notification-tester';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {};
  }

  onClickGenerate_() {
    // Extract arguments from user selections.
    const notifTitleElem = this.$.title;
    const notifBodyElem = this.$.body;
    const notifTitleValue =
        notifTitleElem.options[notifTitleElem.selectedIndex].value;
    const notifBodyValue =
        notifBodyElem.options[notifBodyElem.selectedIndex].value;
    chrome.send('generateNotificationForm', [notifTitleValue, notifBodyValue]);
  }
}

customElements.define(NotificationTester.is, NotificationTester);