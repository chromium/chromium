// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview The implementation of Notification API
 */

import {ClickEventListener, NotificationApi, NotificationOptions, VoidCallback} from './types.js';

/**
 * Provides the implementation for Notification API.
 */
export class NotificationApiImpl implements NotificationApi {
  create(
      notificationId: string, options: NotificationOptions,
      callback: VoidCallback) {
    if (typeof chrome.notifications.create !== 'function') {
      throw new Error('Create notification is not supported ');
    }
    const notificationOptions = {
      title: options.title,
      message: options.message,
      iconUrl: options.iconUrl,
      type: 'basic' as chrome.notifications.TemplateType,
      buttons: options.buttons,
    };
    chrome.notifications.create(notificationId, notificationOptions, callback);
  }
  addClickEventListener(callback: ClickEventListener) {
    if (typeof chrome.notifications.onButtonClicked.addListener !==
        'function') {
      throw new Error('Add notification button listener is not supported ');
    }
    chrome.notifications.onButtonClicked.addListener(callback);
  }
  clear(notificationId: string) {
    if (typeof chrome.notifications.onButtonClicked.addListener !==
        'function') {
      throw new Error('Clear notification is not supported ');
    }
    chrome.notifications.clear(notificationId);
  }
}
