// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

export interface AccessibilityBrowserProxy {
  openTrackpadGesturesSettings(): void;
  recordOverscrollHistoryNavigationChanged(enabled: boolean): void;
}

export class AccessibilityBrowserProxyImpl implements
    AccessibilityBrowserProxy {
  openTrackpadGesturesSettings() {
    chrome.send('openTrackpadGesturesSettings');
  }

  recordOverscrollHistoryNavigationChanged(enabled: boolean) {
    chrome.metricsPrivate.recordBoolean(
        'Settings.OverscrollHistoryNavigation.Enabled', enabled);
  }

  static getInstance(): AccessibilityBrowserProxy {
    return instance || (instance = new AccessibilityBrowserProxyImpl());
  }

  static setInstance(obj: AccessibilityBrowserProxy) {
    instance = obj;
  }
}

let instance: AccessibilityBrowserProxy|null = null;
