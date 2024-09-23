// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {sendWithPromise} from 'chrome://resources/js/cr.js';

export enum PerformanceFeedbackCategory {
  NOTIFICATIONS = 'performance_notifications',
  TABS = 'performance_tabs',
  BATTERY = 'performance_battery',
  SPEED = 'performance_speed',
}

export interface PerformanceBrowserProxy {
  getCurrentOpenSites(): Promise<string[]>;
  getDeviceHasBattery(): Promise<boolean>;
  openFeedbackDialog(categoryTag: PerformanceFeedbackCategory): void;
  validateTabDiscardExceptionRule(rule: string): Promise<boolean>;
}

export class PerformanceBrowserProxyImpl implements PerformanceBrowserProxy {
  getCurrentOpenSites() {
    return sendWithPromise('getCurrentOpenSites');
  }

  getDeviceHasBattery() {
    return sendWithPromise('getDeviceHasBattery');
  }

  openFeedbackDialog(categoryTag: PerformanceFeedbackCategory) {
    chrome.send('openPerformanceFeedbackDialog', [categoryTag]);
  }

  validateTabDiscardExceptionRule(rule: string) {
    return sendWithPromise('validateTabDiscardExceptionRule', rule);
  }

  static getInstance(): PerformanceBrowserProxy {
    return instance || (instance = new PerformanceBrowserProxyImpl());
  }

  static setInstance(obj: PerformanceBrowserProxy) {
    instance = obj;
  }
}

let instance: PerformanceBrowserProxy|null = null;
