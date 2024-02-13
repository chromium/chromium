// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {sendWithPromise} from 'chrome://resources/js/cr.js';

/**
 * @see chrome/browser/ui/webui/tab_search/tab_search_sign_in_handler.cc
 */
export interface TabSearchSignInBrowserProxy {
  /**
   * Returns whether the user is signed in
   */
  getSignInState(): Promise<boolean>;
}

export class TabSearchSignInBrowserProxyImpl implements
    TabSearchSignInBrowserProxy {
  getSignInState() {
    return sendWithPromise('GetSignInState');
  }

  static getInstance(): TabSearchSignInBrowserProxy {
    return instance || (instance = new TabSearchSignInBrowserProxyImpl());
  }

  static setInstance(obj: TabSearchSignInBrowserProxy) {
    instance = obj;
  }
}

let instance: TabSearchSignInBrowserProxy|null = null;
