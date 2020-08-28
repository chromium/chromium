// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {addSingletonGetter} from 'chrome://resources/js/cr.m.js';

/**
 * JavaScript hooks into the native WebUI handler for Phonehub tab.
 */
export class MultidevicePhoneHubBrowserProxy {
  /**
   * Enables or disables the FakePhoneHubManager.
   * @param {boolean} enabled Whether to enable the fake phone hub manager.
   */
  setFakePhoneHubManagerEnabled(enabled) {
    return chrome.send('setFakePhoneHubManagerEnabled', [enabled]);
  }
}

addSingletonGetter(MultidevicePhoneHubBrowserProxy);
