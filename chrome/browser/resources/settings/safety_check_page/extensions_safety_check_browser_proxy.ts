// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {sendWithPromise} from 'chrome://resources/js/cr.js';

// Allows the extensions safety check to get to proper display string
// from the `ExtensionsSafetyCheckHandler`.
export interface ExtensionsSafetyCheckBrowserProxy {
  getTriggeringExtensions(): Promise<string>;
}

export class ExtensionsSafetyCheckBrowserProxyImpl implements
    ExtensionsSafetyCheckBrowserProxy {
  getTriggeringExtensions() {
    return sendWithPromise('getExtensionsThatNeedReview');
  }

  static getInstance(): ExtensionsSafetyCheckBrowserProxy {
    return instance || (instance = new ExtensionsSafetyCheckBrowserProxyImpl());
  }

  static setInstance(obj: ExtensionsSafetyCheckBrowserProxy) {
    instance = obj;
  }
}

let instance: ExtensionsSafetyCheckBrowserProxy|null = null;
