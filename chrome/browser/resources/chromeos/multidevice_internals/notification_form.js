// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/ash/common/cr_elements/cr_shared_style.css.js';
import 'chrome://resources/ash/common/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/ash/common/cr_elements/cr_input/cr_input.js';
import './shared_style.css.js';

import {WebUIListenerBehavior} from 'chrome://resources/ash/common/web_ui_listener_behavior.js';
import {Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {MultidevicePhoneHubBrowserProxy} from './multidevice_phonehub_browser_proxy.js';
import {getTemplate} from './notification_form.html.js';
import {ImageType, imageTypeToStringMap, Importance, importanceToString, Notification} from './types.js';

Polymer({
  is: 'notification-form',

  _template: getTemplate(),

  behaviors: [
    WebUIListenerBehavior,
  ],

  properties: {
    /** @type{!Notification} */
    notification: Object,

    /** @private */
    isNotificationDataValid_: {
      type: Boolean,
      computed: 'computeIsNotificationDataValid_(notification.*)',
    },

    /** @private */
    isSent_: {
      type: Boolean,
      computed: 'computeIsSent_(notification.*)',
      reflectToAttribute: true,
    },

    /** @private */
    isValidId_: {
      type: Boolean,
      computed: 'computeIsValidId_(forbiddenIds, notification.id)',
    },

    /** @private */
    isValidInlineReplyId_: {
      type: Boolean,
      computed: 'computeIsValidInlineReplyId_(forbiddenInlineReplyIds, ' +
          'notification.inlineReplyId)',
    },

    /** @type{!Array<number>} */
    forbiddenIds: {
      type: Array,
      value: [],
    },

    /** @type{!Array<number>} */
    forbiddenInlineReplyIds: {
      type: Array,
      value: [],
    },

    /** @private */
    imageList_: {
      type: Array,
      value: () => {
        return [
          ImageType.NONE,
          ImageType.PINK,
          ImageType.RED,
          ImageType.GREEN,
          ImageType.BLUE,
          ImageType.YELLOW,
        ];
      },
      readonly: true,
    },

    /** @private */
    importanceList_: {
      type: Array,
      value: () => {
        return [
          Importance.UNSPECIFIED,
          Importance.NONE,
          Importance.MIN,
          Importance.LOW,
          Importance.DEFAULT,
          Importance.HIGH,
        ];
      },
      readonly: true,
    },

    /** @private */
    updateNotificationText_: {
      type: String,
      value: 'Update this notification',
    },

    /** @private */
    userDismissed_: {
      type: Boolean,
      value: false,
      reflectToAttribute: true,
    },
  },

  /** @private{?MultidevicePhoneHubBrowserProxy}*/
  browserProxy_: null,

  /** @override */
  created() {
    this.browserProxy_ = MultidevicePhoneHubBrowserProxy.getInstance();
  },

  /** @override */
  attached() {
    this.addWebUIListener(
        'removed-notification-ids', this.onNotificationIdsRemoved_.bind(this));
  },

  /**
   * @param{Array<number>} ids Removed notifications' ids.
   * @private
   */
  onNotificationIdsRemoved_(ids) {
    if (this.notification.sent && ids.includes(this.notification.id)) {
      this.userDismissed_ = true;
    }
  },

  /**
   * @return {boolean}
   * @private
   */
  computeIsValidId_() {
    return this.notification.sent ||
        !this.forbiddenIds.includes(Number(this.notification.id));
  },

  /**
   * @return {boolean}
   * @private
   */
  computeIsValidInlineReplyId_() {
    return this.notification.sent ||
        !this.forbiddenInlineReplyIds.includes(
            Number(this.notification.inlineReplyId));
  },

  /**
   * @return {boolean}
   * @private
   */
  computeIsNotificationDataValid_() {
    // If the user dismissed the notification, it is no longer valid.
    if (this.userDismissed_) {
      return false;
    }

    // If either the notification ID or inline reply id is invalid,
    // the notification is invalid.
    if (!this.isValidId_ || !this.isValidInlineReplyId_) {
      return false;
    }

    // Other required fields that need to be formatted correctly.
    if (!this.notification.appMetadata.visibleAppName ||
        this.notification.icon === ImageType.NONE ||
        Number(this.notification.timestamp) < 0) {
      return false;
    }

    // At least the title, text content, or shared image must be populated.
    return !!this.notification.title || !!this.notification.textContent ||
        this.notification.sharedImage !== ImageType.NONE;
  },

  /** @private */
  computeIsSent_() {
    return this.notification.sent;
  },

  /** @private */
  onSetNotification_() {
    this.browserProxy_.setNotification(this.notification);
    this.notification.sent = true;
    this.notifyPath('notification.sent');
  },

  /** @private */
  onUpdateNotification_() {
    this.onSetNotification_();
    this.updateNotificationText_ = 'Update Sent!';
    setTimeout(() => {
      this.updateNotificationText_ = 'Update this notification';
    }, 1000);
  },

  /** @private */
  onRemoveButtonClick_() {
    if (!this.userDismissed_) {
      this.browserProxy_.removeNotification(this.notification.id);
    }
    this.fire('remove-notification');
  },

  /**
   * @param {ImageType} imageType
   * @return {String}
   * @private
   */
  getImageTypeName_(imageType) {
    return imageTypeToStringMap.get(imageType);
  },

  /**
   * @param {Importance} importance
   * @return {String}
   * @private
   */
  getImportanceName_(importance) {
    return importanceToString.get(importance);
  },

  /** @private */
  onInlineReplyIdChanged_() {
    //<cr-input> does not save value numerically.
    this.notification.inlineReplyId = Number(this.notification.inlineReplyId);
    this.notifyPath('notification.inlineReplyId');
  },

  /** @private */
  onNotificationIdChanged_() {
    //<cr-input> does not save value numerically.
    this.notification.id = Number(this.notification.id);
    this.notifyPath('notification.id');
  },

  /** @private */
  onTimeStampChanged_() {
    //<cr-input> does not save value numerically.
    this.notification.timestamp = Number(this.notification.timestamp);
    this.notifyPath('notification.timestamp');
  },

  /** @private */
  onIconImageTypeSelected_() {
    const select = /** @type {!HTMLSelectElement} */
        (this.$$('#iconImageTypeSelector'));
    this.notification.appMetadata.icon = this.imageList_[select.selectedIndex];
  },

  /** @private */
  onSharedImageTypeSelected_() {
    const select = /** @type {!HTMLSelectElement} */
        (this.$$('#sharedImageTypeSelector'));
    this.notification.sharedImage = this.imageList_[select.selectedIndex];
    this.notifyPath('notification.sharedImage');
  },

  /** @private */
  onContactImageTypeSelected_() {
    const select = /** @type {!HTMLSelectElement} */
        (this.$$('#contactImageSelector'));
    this.notification.contactImage = this.imageList_[select.selectedIndex];
    this.notifyPath('notification.contactImage');
  },

  /** @private */
  onImportanceSelected_() {
    const select = /** @type {!HTMLSelectElement} */
        (this.$$('#importanceSelector'));
    this.notification.importance = this.importanceList_[select.selectedIndex];
    this.notifyPath('notification.importance');
  },

  /**
   * @param {*} lhs
   * @param {*} rhs
   * @return {boolean}
   * @private
   */
  isEqual_(lhs, rhs) {
    return lhs === rhs;
  },
});
