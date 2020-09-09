// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview A helper object used by the dice web signin intercept bubble to
 * interact with the browser.
 */
import {addSingletonGetter, sendWithPromise} from 'chrome://resources/js/cr.m.js';

/**
 * Account information sent from C++.
 * @typedef {{
 *   isManaged: boolean,
 *   pictureUrl: string,
 * }}
 */
export let AccountInfo;

/**
 * @typedef {{
 *   headerText: string,
 *   bodyTitle: string,
 *   bodyText: string,
 *   headerTextColor: string,
 *   headerBackgroundColor: string,
 *   interceptedAccount: AccountInfo,
 * }}
 */
export let InterceptionParameters;

/** @interface */
export class DiceWebSigninInterceptBrowserProxy {
  /** Called when the user accepts the interception bubble. */
  accept() {}

  /** Called when the user cancels the interception. */
  cancel() {}

  /**
   * Called when the page is loaded.
   * @return {!Promise<!InterceptionParameters>}
   * */
  pageLoaded() {}
}

/** @implements {DiceWebSigninInterceptBrowserProxy} */
export class DiceWebSigninInterceptBrowserProxyImpl {
  /** @override */
  accept() {
    chrome.send('accept');
  }

  /** @override */
  cancel() {
    chrome.send('cancel');
  }

  /** @override */
  pageLoaded() {
    return sendWithPromise('pageLoaded');
  }
}

addSingletonGetter(DiceWebSigninInterceptBrowserProxyImpl);
