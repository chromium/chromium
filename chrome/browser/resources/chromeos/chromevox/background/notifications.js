// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Provides notification support for ChromeVox.
 */

goog.provide('Notifications');

goog.require('PanelCommand');

/**
 * ChromeVox update notification.
 * @constructor
 */
function UpdateNotification() {
  this.data = {};
  this.data.type = 'basic';
  this.data.iconUrl = '/images/chromevox-16.png';
  this.data.title = Msgs.getMsg('update_title');
  this.data.message = Msgs.getMsg('update_message_new');
}

UpdateNotification.prototype = {
  /** @return {boolean} */
  shouldShow: function() {
    return !localStorage['notifications_update_notification_shown'] &&
        chrome.runtime.getManifest().version >= '63' &&
        chrome.runtime.getManifest().version < '64';
  },

  /** Shows the notification. */
  show: function() {
    if (!this.shouldShow()) {
      return;
    }
    chrome.notifications.create('update', this.data);
    chrome.notifications.onClicked.addListener(this.onClicked);
    chrome.notifications.onClosed.addListener(this.onClosed);
  },

  /**
   * Handles the chrome.notifications event.
   * @param {string} notificationId
   */
  onClicked: function(notificationId) {
    (new PanelCommand(PanelCommandType.TUTORIAL)).send();
  },

  /**
   * Handles the chrome.notifications event.
   * @param {string} id
   */
  onClosed: function(id) {
    localStorage['notifications_update_notification_shown'] = true;
  },

  /**
   * Removes all listeners added by this object.
   */
  removeAllListeners: function() {
    chrome.notifications.onClicked.removeListener(this.onClicked);
    chrome.notifications.onClosed.removeListener(this.onClosed);
  }
};

/**
 * Set after an update is shown.
 * @type {UpdateNotification}
 */
Notifications.currentUpdate;

/**
 * Runs notifications that should be shown for startup.
 */
Notifications.onStartup = function() {
  // Only run on background page.
  if (document.location.href.indexOf('background.html') == -1) {
    return;
  }

  Notifications.currentUpdate = new UpdateNotification();
  Notifications.currentUpdate.show();
};
