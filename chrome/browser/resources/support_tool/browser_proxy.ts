// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {sendWithPromise} from 'chrome://resources/js/cr.m.js';

export interface BrowserProxy {
  /**
   * Gets the list of email addresses that are logged in from C++ side.
   */
  getEmailAddresses(): Promise<string[]>;
}

export class BrowserProxyImpl implements BrowserProxy {
  getEmailAddresses() {
    return sendWithPromise('getEmailAddresses');
  }

  static getInstance(): BrowserProxy {
    return instance || (instance = new BrowserProxyImpl());
  }

  static setInstance(obj: BrowserProxy) {
    instance = obj;
  }
}

// The singleton instance_ is replaced with a test version of this wrapper
// during testing.
let instance: BrowserProxy|null = null;