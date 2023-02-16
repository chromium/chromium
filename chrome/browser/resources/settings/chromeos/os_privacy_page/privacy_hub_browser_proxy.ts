// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {sendWithPromise} from 'chrome://resources/js/cr.js';

export interface PrivacyHubBrowserProxy {
  getInitialMicrophoneHardwareToggleState(): Promise<boolean>;
  sendLeftOsPrivacyPage(): void;
  sendOpenedOsPrivacyPage(): void;
}

let instance: PrivacyHubBrowserProxy|null = null;

export class PrivacyHubBrowserProxyImpl implements PrivacyHubBrowserProxy {
  getInitialMicrophoneHardwareToggleState(): Promise<boolean> {
    return sendWithPromise('getInitialMicrophoneHardwareToggleState');
  }

  sendLeftOsPrivacyPage(): void {
    chrome.send('leftOsPrivacyPage');
  }

  sendOpenedOsPrivacyPage(): void {
    chrome.send('osPrivacyPageWasOpened');
  }

  static getInstance(): PrivacyHubBrowserProxy {
    return instance || (instance = new PrivacyHubBrowserProxyImpl());
  }

  static setInstanceForTesting(obj: PrivacyHubBrowserProxy): void {
    instance = obj;
  }
}
