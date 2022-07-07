// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './select_custom.js';
import 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import 'chrome://resources/cr_elements/cr_radio_group/cr_radio_group.m.js';
import 'chrome://resources/cr_elements/cr_radio_button/cr_radio_button.m.js';
import 'chrome://resources/cr_elements/cr_checkbox/cr_checkbox.m.js';

import {html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {FormSelectOptions, NotificationType, NotifierType} from './form_constants.js';
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
          // Set default values of form elements.
          return {
            notifierType: 'System',
            richDataNeverTimeout: false,
            richDataPinned: false,
            richDataRenotify: false,
            richDataProgress: '-1',
            richDataShowSettings: false,
            richDataShowSnooze: false,
            richDataNumButtons: '0',
            richDataNumNotifItems: '0',
            originURL: '',
          };
        },
      },
      /*
      @type {!boolean}
      */
      showTypeSpecificDesc: {type: Boolean},
      /*
      @type {!boolean}
      */
      showProgressOptions: {type: Boolean},
      /*
      @type {!boolean}
      */
      showMultiOptions: {type: Boolean},
      /*
      @type {!boolean}
      */
      showDisplaySource: {type: Boolean},
      /*
      @type {!boolean}
      */
      showOriginURL: {type: Boolean},
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
      /*
       * @private
       */
      smallImageSelectList: {
        type: Array,
        value: FormSelectOptions.SMALL_IMAGE_OPTIONS,
      },
      /*
       * @private
       */
      sourceSelectList: {
        type: Array,
        value: FormSelectOptions.SOURCE_OPTIONS,
      },
      /*
       * @private
       */
      originURLSelectList: {
        type: Array,
        value: FormSelectOptions.URL_OPTIONS,
      },
      /*
       * @private
       */
      notificationTypeSelectList: {
        type: Array,
        value: FormSelectOptions.NOTIFICATION_TYPE_OPTIONS,
      },
      /*
       * @private
       */
      prioritySelectList: {
        type: Array,
        value: FormSelectOptions.PRIORITY_OPTIONS,
      },
      /*
       * @private
       */
      progressStatusSelectList: {
        type: Array,
        value: FormSelectOptions.PROGRESS_STATUS_OPTIONS,
      },
      /*
       * @private
       */
      notificationIDSelectList: {
        type: Array,
        value: FormSelectOptions.NOTIFICATION_ID_OPTIONS,
      },
    };
  }

  static get observers() {
    return [
      'notificationTypeChanged_(notifMetadata.notificationType)',
      'notifierTypeChanged_(notifMetadata.notifierType)',
    ];
  }

  onClickGenerate() {
    // Convert properties where strings represent base-10 numbers to base-10
    // numbers.
    const BASE_TEN = 10;
    this.notifMetadata.richDataNumButtons =
        parseInt(this.notifMetadata.richDataNumButtons, BASE_TEN);
    this.notifMetadata.richDataNumNotifItems =
        parseInt(this.notifMetadata.richDataNumNotifItems, BASE_TEN);
    this.notifMetadata.richDataProgress =
        parseInt(this.notifMetadata.richDataProgress, BASE_TEN);

    // Send notification data to C++
    chrome.send('generateNotificationForm', [this.notifMetadata]);
  }

  // Show / hide dom elements when this.notifMetadata.notificationType changes
  notificationTypeChanged_(notificationType) {
    this.showMultiOptions =
        (notificationType == NotificationType.NOTIFICATION_TYPE_MULTIPLE);
    this.showProgressOptions =
        (notificationType == NotificationType.NOTIFICATION_TYPE_PROGRESS);
    this.showTypeSpecificDesc =
        !(this.showMultiOptions || this.showProgressOptions);
  }

  // Set this.notifMetadata.notifierType to the appropriate number given the
  // notifier as a string.
  notifierTypeChanged_(notifierType) {
    // notifierType is guaranteed to be 'System' or 'Web'.
    this.showDisplaySource = notifierType == 'System';
    this.showOriginURL = notifierType == 'Web';
    if (notifierType == 'System') {
      this.notifMetadata.notifierType = NotifierType.SYSTEM_COMPONENT;
      return;
    }

    this.notifMetadata.notifierType = NotifierType.WEB_PAGE;
  }
}

customElements.define(NotificationTester.is, NotificationTester);