// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview A helper object used from the "Default Browser" section
 * to interact with the browser.
 */

// clang-format off
import {sendWithPromise} from 'chrome://resources/js/cr.js';
// clang-format on

export interface DefaultBrowserInfo {
  canBeDefault: boolean;
  isDefault: boolean;
  isDisabledByPolicy: boolean;
  isUnknownError: boolean;
}

export interface DefaultBrowserBrowserProxy {
  /**
   * Get the initial DefaultBrowserInfo and begin sending updates to
   * 'settings.updateDefaultBrowserState'.
   */
  requestDefaultBrowserState(): Promise<DefaultBrowserInfo>;

  /*
   * Try to set the current browser as the default browser. The new status of
   * the settings will be sent to 'settings.updateDefaultBrowserState'.
   */
  setAsDefaultBrowser(): void;
}

export class DefaultBrowserBrowserProxyImpl implements
    DefaultBrowserBrowserProxy {
  requestDefaultBrowserState() {
    return sendWithPromise('requestDefaultBrowserState');
  }

  setAsDefaultBrowser() {
    chrome.send('setAsDefaultBrowser');
  }

  static getInstance(): DefaultBrowserBrowserProxy {
    return instance || (instance = new DefaultBrowserBrowserProxyImpl());
  }

  static setInstance(obj: DefaultBrowserBrowserProxy) {
    instance = obj;
  }
}

let instance: DefaultBrowserBrowserProxy|null = null;
