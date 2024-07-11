// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {sendWithPromise} from 'chrome://resources/js/cr.js';

export interface TabSearchSyncBrowserProxy {
  /**
   * Returns whether the user is signed in
   */
  getSignInState(): Promise<boolean>;
}

export class TabSearchSyncBrowserProxyImpl implements
    TabSearchSyncBrowserProxy {
  getSignInState() {
    return sendWithPromise('GetSignInState');
  }

  static getInstance(): TabSearchSyncBrowserProxy {
    return instance || (instance = new TabSearchSyncBrowserProxyImpl());
  }

  static setInstance(obj: TabSearchSyncBrowserProxy) {
    instance = obj;
  }
}

let instance: TabSearchSyncBrowserProxy|null = null;
