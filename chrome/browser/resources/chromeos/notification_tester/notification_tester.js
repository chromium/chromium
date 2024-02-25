// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './select_custom.js';
import 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import 'chrome://resources/ash/common/cr_elements/cr_radio_group/cr_radio_group.js';
import 'chrome://resources/ash/common/cr_elements/cr_radio_button/cr_radio_button.js';
import 'chrome://resources/ash/common/cr_elements/cr_checkbox/cr_checkbox.js';
import 'chrome://resources/ash/common/cr_elements/cr_input/cr_input.js';

import {html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {FormSelectOptions, NOTIFICATION_VIEW_TYPES, NotificationPriority, NotificationType, NotifierType, SystemNotificationWarningLevel} from './form_constants.js';
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
      @type {boolean}
      */
      showTypeSpecificDesc: {type: Boolean},
      /*
      @type {boolean}
      */
      showProgressOptions: {type: Boolean},
      /*
      @type {boolean}
      */
      showMultiOptions: {type: Boolean},
      /*
      @type {boolean}
      */
      isSystemNotification: {type: Boolean},
      /*
      @type {boolean}
      */
      isWebNotification: {type: Boolean},
      /*
      @type {boolean}
      */
      generatingDelayedNotification: {type: Boolean, value: false},
      /*
      @type {number}
      */
      countdownDisplayTime: {type: String},
      /*
      @type {boolean}
      */
      delayTimeInvalid: {type: Boolean},
      /*
      @type {string}
      */
      notificationDelayTime: {type: String, value: '0'},
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
      displaySourceSelectList: {
        type: Array,
        value: FormSelectOptions.DISPLAY_SOURCE_OPTIONS,
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
      /*
       * @private
       */
      warningLevelSelectList: {
        type: Array,
        value: FormSelectOptions.WARNING_LEVEL_OPTIONS,
      },
      /*
       * @private
       */
      timestampSelectList: {
        type: Array,
        value: FormSelectOptions.TIME_STAMP_OPTIONS,
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
    const NUM_MS_IN_S = 1000;
    if (!this.delayTimeInvalid) {
      // If the user enters 0 or leaves the delay time field blank, generate
      // notifications synchronously.
      const timedInputValueNumber =
          parseInt(this.notificationDelayTime, BASE_TEN);
      if (timedInputValueNumber === 0 ||
          this.notificationDelayTime.length === 0) {
        chrome.send('generateNotificationForm', [this.notifMetadata]);
      } else {
        this.startDelayedNotificationCountdown(timedInputValueNumber);

        // Enable synchronous generation while delayed notification generation
        // is underway.
        this.notificationDelayTime = '';
        // Create a deep copy of the current state of this.notifMetadata to
        // ensure it won't be modified before chrome.send() is called.
        const notifMetadataCopy = structuredClone(this.notifMetadata);
        setTimeout(
            chrome.send, timedInputValueNumber * NUM_MS_IN_S,
            'generateNotificationForm', [notifMetadataCopy]);
      }
    }
  }

  onClickReset() {
    this.set('notifMetadata.id', 'random');
    this.set('notifMetadata.title', 'Notification Title');
    this.set('notifMetadata.message', 'Notification content');
    this.set('notifMetadata.icon', 'none');
    this.set('notifMetadata.displaySource', 'Sample Display Source');
    this.set('notifMetadata.originURL', 'https://testurl.xyz');
    this.set(
        'notifMetadata.notificationType',
        NotificationType.NOTIFICATION_TYPE_SIMPLE);
    this.set('notifMetadata.notifierType', 'System');
    this.set(
        'notifMetadata.warningLevel', SystemNotificationWarningLevel.NORMAL);
    this.set('notifMetadata.richDataImage', 'none');
    this.set('notifMetadata.richDataSmallImage', 'kProductIcon');
    this.set('notifMetadata.richDataNeverTimeout', false);
    this.set(
        'notifMetadata.richDataPriority',
        NotificationPriority.DEFAULT_PRIORITY);
    this.set('notifMetadata.richDataTimestamp', 0);
    this.set('notifMetadata.richDataPinned', false);
    this.set('notifMetadata.richDataShowSnooze', false);
    this.set('notifMetadata.richDataShowSettings', false);
    this.set('notifMetadata.richDataProgress', '-1');
    this.set('notifMetadata.richDataProgressStatus', 'Progress Status');
    this.set('notifMetadata.richDataNumNotifItems', '0');
    this.set('notifMetadata.richDataNumButtons', '0');
  }

  // Show / hide dom elements when this.notifMetadata.notificationType changes
  notificationTypeChanged_(notificationType) {
    this.showMultiOptions =
        (notificationType === NotificationType.NOTIFICATION_TYPE_MULTIPLE);
    this.showProgressOptions =
        (notificationType === NotificationType.NOTIFICATION_TYPE_PROGRESS);
    this.showTypeSpecificDesc =
        !(this.showMultiOptions || this.showProgressOptions);
  }

  // Set this.notifMetadata.notifierType to the appropriate number given the
  // notifier as a string.
  notifierTypeChanged_(notifierType) {
    // notifierType is guaranteed to be 'System' or 'Web'.
    this.isSystemNotification = notifierType === 'System';
    this.isWebNotification = notifierType === 'Web';
    if (notifierType === 'System') {
      this.notifMetadata.notifierType = NotifierType.SYSTEM_COMPONENT;
      return;
    }

    this.notifMetadata.notifierType = NotifierType.WEB_PAGE;
  }

  // Start a countdown from the given number in seconds. Used for delayed
  // notification generation.
  startDelayedNotificationCountdown(startTime) {
    // Note that setInterval initially delays before executing the function.
    const ONE_SECOND = 1000;  // milliseconds.
    this.countdownDisplayTime = startTime;
    this.generatingDelayedNotification = true;
    const countdownTimer = setInterval(() => {
      this.countdownDisplayTime--;
      if (this.countdownDisplayTime <= 0) {
        clearInterval(countdownTimer);
        this.generatingDelayedNotification = false;
      }
    }, ONE_SECOND);
  }

  // Generate notifications of all types from
  // go/cros-notification-spec.
  onClickGenerateAllTypes() {
    // chrome.send doesn't seem to properly register more than two consecutive
    // synchronously calls, so they are sent with a small delay.
    const DELAY_MS = 250;
    for (let i = 0; i < NOTIFICATION_VIEW_TYPES.length; i++) {
      setTimeout(
          chrome.send, DELAY_MS * i, 'generateNotificationForm',
          [NOTIFICATION_VIEW_TYPES[i]]);
    }
  }
}

customElements.define(NotificationTester.is, NotificationTester);
