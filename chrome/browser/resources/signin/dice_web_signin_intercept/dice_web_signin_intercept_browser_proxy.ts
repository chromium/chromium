// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview A helper object used by the dice web signin intercept bubble to
 * interact with the browser.
 */
import {sendWithPromise} from 'chrome://resources/js/cr.js';

/** Account information sent from C++. */
export interface AccountInfo {
  isManaged: boolean;
  pictureUrl: string;
}

export interface InterceptionParameters {
  headerText: string;
  bodyTitle: string;
  bodyText: string;
  confirmButtonLabel: string;
  cancelButtonLabel: string;
  managedDisclaimerText: string;
  headerTextColor: string;
  interceptedProfileColor: string;
  primaryProfileColor: string;
  interceptedAccount: AccountInfo;
  primaryAccount: AccountInfo;
  showGuestOption: boolean;
  useV2Design: boolean;
  showManagedDisclaimer: boolean;
}

export interface DiceWebSigninInterceptBrowserProxy {
  // Called when the user accepts the interception bubble.
  accept(): void;

  // Called when the user cancels the interception.
  cancel(): void;

  // Called when user selects Guest mode.
  guest(): void;

  // Called when the page is loaded.
  pageLoaded(): Promise<InterceptionParameters>;
}

export class DiceWebSigninInterceptBrowserProxyImpl implements
    DiceWebSigninInterceptBrowserProxy {
  accept() {
    chrome.send('accept');
  }

  cancel() {
    chrome.send('cancel');
  }

  guest() {
    chrome.send('guest');
  }

  pageLoaded() {
    return sendWithPromise('pageLoaded');
  }

  static getInstance(): DiceWebSigninInterceptBrowserProxy {
    return instance ||
        (instance = new DiceWebSigninInterceptBrowserProxyImpl());
  }

  static setInstance(obj: DiceWebSigninInterceptBrowserProxy) {
    instance = obj;
  }
}

let instance: DiceWebSigninInterceptBrowserProxy|null = null;
