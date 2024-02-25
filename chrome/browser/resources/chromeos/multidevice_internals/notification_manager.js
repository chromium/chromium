// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/ash/common/cr_elements/cr_shared_style.css.js';
import 'chrome://resources/ash/common/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/ash/common/cr_elements/cr_input/cr_input.js';
import './shared_style.css.js';
import './notification_form.js';

import {Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './notification_manager.html.js';
import {ImageType, Importance, Notification} from './types.js';

/**
 * @param {number} notificationId
 * @param {number} nextNotificationInlineReplyId
 * @return {!Notification}
 */
function newNotification(notificationId, nextNotificationInlineReplyId) {
  return {
    sent: false,
    id: notificationId,
    appMetadata: {
      visibleAppName: 'Fake visible app name',
      packageName: 'Fake package name',
      icon: ImageType.RED,
    },
    timestamp: Date.now(),
    importance: Importance.DEFAULT,
    inlineReplyId: nextNotificationInlineReplyId,
    title: null,
    textContent: null,
    sharedImage: ImageType.NONE,
    contactImage: ImageType.NONE,
  };
}

Polymer({
  is: 'notification-manager',

  _template: getTemplate(),

  properties: {
    /** @private */
    idLatest_: {
      type: Number,
      value: 0,
    },

    /** @private */
    inlineReplyIdLatest_: {
      type: Number,
      value: 0,
    },

    /**
     * @type {!Array<!Notification>}
     */
    notificationList_: {
      type: Array,
      value: [],
    },

    /**
     * @type {!Array<number>}
     */
    sentNotificationIds_: {
      type: Array,
      computed: 'computeSentNotificationIds_(notificationList_.*)',
    },

    /**
     * @type {!Array<number>}
     */
    sentInlineReplyIds_: {
      type: Array,
      computed: 'computeSentInlineReplyIds_(notificationList_.*)',
    },
  },

  /** @private */
  onAddNotificationClick_() {
    this.idLatest_++;
    this.inlineReplyIdLatest_++;
    this.notificationList_.unshift(
        newNotification(this.idLatest_, this.inlineReplyIdLatest_));
    this.$.notificationList.render();
  },

  /**
   * @param {!Event} e
   * @private
   */
  onRemoveNotification_(e) {
    const notificationEl = e.composedPath()[0];
    const notificationIndex =
        this.$.notificationList.indexForElement(notificationEl);
    this.notificationList_.splice(notificationIndex, 1);
    this.$.notificationList.render();
  },

  /** @return {!Array<number>} */
  computeSentNotificationIds_() {
    return this.notificationList_.filter(item => item.sent)
        .map(item => item.id);
  },

  /** @return  {!Array<number>}*/
  computeSentInlineReplyIds_() {
    return this.notificationList_.filter(item => item.sent)
        .map(item => item.inlineReplyId);
  },
});
