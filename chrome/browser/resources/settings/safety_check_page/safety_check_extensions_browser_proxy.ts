// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {sendWithPromise} from 'chrome://resources/js/cr.js';

// Allows the extensions safety check to get to proper display string
// from the `SafetyCheckExtensionsHandler`.
export interface SafetyCheckExtensionsBrowserProxy {
  getNumberOfExtensionsThatNeedReview(): Promise<number>;
}

export class SafetyCheckExtensionsBrowserProxyImpl implements
    SafetyCheckExtensionsBrowserProxy {
  getNumberOfExtensionsThatNeedReview() {
    return sendWithPromise('getNumberOfExtensionsThatNeedReview');
  }

  static getInstance(): SafetyCheckExtensionsBrowserProxy {
    return instance || (instance = new SafetyCheckExtensionsBrowserProxyImpl());
  }

  static setInstance(obj: SafetyCheckExtensionsBrowserProxy) {
    instance = obj;
  }
}

let instance: SafetyCheckExtensionsBrowserProxy|null = null;
