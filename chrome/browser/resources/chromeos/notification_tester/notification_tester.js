// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './select_custom.js';

import {html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {FormSelectOptions} from './form_constants.js';
import {Notification} from './types.js';

// Web component housing the form for chrome://notification-tester.
export class NotificationTester extends PolymerElement {
  static get is() {
    return 'notification-tester';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      /*
       @type {!Notification}
       */
      notifMetadata: {
        type: Object,
        value: function() {
          return {};
        },
      },
      /*
       * @private
       */
      titleSelectList: {
        type: Array,
        value: FormSelectOptions.TITLE_OPTIONS,
      },
      /*
       * @private
       */
      messageSelectList: {
        type: Array,
        value: FormSelectOptions.MESSAGE_OPTIONS,
      },
      /*
       * @private
       */
      badgeSelectList: {
        type: Array,
        value: FormSelectOptions.BADGE_OPTIONS,
      },
      /*
       * @private
       */
      imageSelectList: {
        type: Array,
        value: FormSelectOptions.IMAGE_OPTIONS,
      },
      /*
       * @private
       */
      iconSelectList: {
        type: Array,
        value: FormSelectOptions.ICON_OPTIONS,
      },
    };
  }

  onClickGenerate() {
    // Send notification data to C++
    chrome.send('generateNotificationForm', [this.notifMetadata]);
  }
}

customElements.define(NotificationTester.is, NotificationTester);