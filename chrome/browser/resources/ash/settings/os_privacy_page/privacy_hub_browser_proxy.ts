// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {sendWithPromise} from 'chrome://resources/js/cr.js';

export interface PrivacyHubBrowserProxy {
  getInitialMicrophoneHardwareToggleState(): Promise<boolean>;
  getInitialMicrophoneMutedBySecurityCurtainState(): Promise<boolean>;
  getInitialCameraSwitchForceDisabledState(): Promise<boolean>;
  getCameraLedFallbackState(): Promise<boolean>;
  getCurrentTimeZoneName(): Promise<string>;
  getCurrentSunriseTime(): Promise<string>;
  getCurrentSunsetTime(): Promise<string>;
  sendLeftOsPrivacyPage(): void;
  sendOpenedOsPrivacyPage(): void;
}

let instance: PrivacyHubBrowserProxy|null = null;

export class PrivacyHubBrowserProxyImpl implements PrivacyHubBrowserProxy {
  getInitialMicrophoneHardwareToggleState(): Promise<boolean> {
    return sendWithPromise('getInitialMicrophoneHardwareToggleState');
  }

  getInitialMicrophoneMutedBySecurityCurtainState(): Promise<boolean> {
    return sendWithPromise('getInitialMicrophoneMutedBySecurityCurtainState');
  }

  getInitialCameraSwitchForceDisabledState(): Promise<boolean> {
    return sendWithPromise('getInitialCameraSwitchForceDisabledState');
  }

  getCameraLedFallbackState(): Promise<boolean> {
    return sendWithPromise('getCameraLedFallbackState');
  }

  getCurrentTimeZoneName(): Promise<string> {
    return sendWithPromise('getCurrentTimeZoneName');
  }

  getCurrentSunriseTime(): Promise<string> {
    return sendWithPromise('getCurrentSunriseTime');
  }

  getCurrentSunsetTime(): Promise<string> {
    return sendWithPromise('getCurrentSunsetTime');
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
