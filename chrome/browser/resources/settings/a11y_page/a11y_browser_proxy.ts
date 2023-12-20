// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {sendWithPromise} from 'chrome://resources/js/cr.js';

// <if expr="is_win or is_linux or is_macosx">
/**
 * Numerical values should not be changed because they must stay in sync with
 * screen_ai::ScreenAIInstallState::State defined in screen_ai_install_state.h.
 */
export enum ScreenAiInstallStatus {
  NOT_DOWNLOADED = 0,
  DOWNLOADING = 1,
  FAILED = 2,
  DOWNLOADED = 3,
  READY = 4,
}
// </if>

export interface AccessibilityBrowserProxy {
  openTrackpadGesturesSettings(): void;
  recordOverscrollHistoryNavigationChanged(enabled: boolean): void;
  // <if expr="is_win or is_linux or is_macosx">
  getScreenAiInstallState(): Promise<ScreenAiInstallStatus>;
  // </if>
  getScreenReaderState(): Promise<boolean>;
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

  // <if expr="is_win or is_linux or is_macosx">
  getScreenAiInstallState() {
    return sendWithPromise('getScreenAiInstallState');
  }
  // </if>

  getScreenReaderState() {
    return sendWithPromise('getScreenReaderState');
  }

  static getInstance(): AccessibilityBrowserProxy {
    return instance || (instance = new AccessibilityBrowserProxyImpl());
  }

  static setInstance(obj: AccessibilityBrowserProxy) {
    instance = obj;
  }
}

let instance: AccessibilityBrowserProxy|null = null;
