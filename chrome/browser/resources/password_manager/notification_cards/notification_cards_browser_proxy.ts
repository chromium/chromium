// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {sendWithPromise} from 'chrome://resources/js/cr.js';

export interface NotificationCard {
  id: string;
  title: string;
  description: string;
  actionButtonText?: string;
  isDismissible: boolean;
}

export interface NotificationCardsProxy {
  /**
   * Returns a notification card to show, or null if there are no available
   * notification cards.
   */
  getAvailableNotificationCard(): Promise<NotificationCard|null>;

  /**
   * Records dismissal of a notification card. This is important to determine
   * whether the notification should be shown in the future.
   */
  recordNotificationDismissed(id: string): void;
}

export class NotificationCardsProxyImpl implements NotificationCardsProxy {
  getAvailableNotificationCard() {
    return sendWithPromise<NotificationCard|null>(
        'getAvailableNotificationCard');
  }

  recordNotificationDismissed(id: string) {
    chrome.send('recordNotificationDismissed', [id]);
  }

  static getInstance(): NotificationCardsProxy {
    return instance || (instance = new NotificationCardsProxyImpl());
  }

  static setInstance(obj: NotificationCardsProxy) {
    instance = obj;
  }
}

let instance: NotificationCardsProxy|null = null;
