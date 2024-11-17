// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {sendWithPromise} from 'chrome://resources/ash/common/cr.m.js';

/** @interface */
export class BrowserProxy {
  /**
   * Requests profile information; namely, a dictionary containing the user's
   * e-mail address and profile photo.
   * @return {!Promise<{profilePhotoUrl: string, email: string}>}
   */
  getProfileInfo() {}

  /**
   * Opens settings to the MultiDevice individual feature settings subpage.
   * (a.k.a. Connected Devices).
   */
  openMultiDeviceSettings() {}
}

/** @implements {BrowserProxy} */
export class BrowserProxyImpl {
  getProfileInfo() {
    return sendWithPromise('getProfileInfo');
  }

  openMultiDeviceSettings() {
    chrome.send('openMultiDeviceSettings');
  }

  /** @return {!BrowserProxy} */
  static getInstance() {
    return instance || (instance = new BrowserProxyImpl());
  }

  /** @param {!BrowserProxy} obj */
  static setInstance(obj) {
    instance = obj;
  }
}

/** @type {?BrowserProxy} */
let instance = null;
