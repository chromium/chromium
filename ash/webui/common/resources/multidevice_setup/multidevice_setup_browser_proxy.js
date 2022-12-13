// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {sendWithPromise} from 'chrome://resources/ash/common/cr.m.js';
import {addSingletonGetter} from 'chrome://resources/ash/common/cr_deprecated.js';

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
  /** @override */
  getProfileInfo() {
    return sendWithPromise('getProfileInfo');
  }

  /** @override */
  openMultiDeviceSettings() {
    chrome.send('openMultiDeviceSettings');
  }
}

addSingletonGetter(BrowserProxyImpl);
