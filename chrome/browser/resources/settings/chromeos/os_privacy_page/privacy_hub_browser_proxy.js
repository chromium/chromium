// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {sendWithPromise} from 'chrome://resources/ash/common/cr.m.js';

/** @interface */
export class PrivacyHubBrowserProxy {
  /** @return {!Promise<boolean>} */
  getInitialMicrophoneHardwareToggleState() {}
}

/**
 * @implements {PrivacyHubBrowserProxy}
 */
export class PrivacyHubBrowserProxyImpl {
  /** @override */
  getInitialMicrophoneHardwareToggleState() {
    return sendWithPromise('getInitialMicrophoneHardwareToggleState');
  }

  /** @return {!PrivacyHubBrowserProxy} */
  static getInstance() {
    return instance || (instance = new PrivacyHubBrowserProxyImpl());
  }

  /** @param {!PrivacyHubBrowserProxy} obj */
  static setInstanceForTesting(obj) {
    instance = obj;
  }
}

/** @type {?PrivacyHubBrowserProxy} */
let instance = null;
