// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {AuthCompletedCredentials} from 'chrome://chrome-signin/gaia_auth_host/authenticator.js';

export interface InlineLoginBrowserProxy {
  /** Send 'initialize' message to prepare for starting auth. */
  initialize(): void;

  /**
   * Send 'authenticatorReady' message to handle tasks after authenticator
   * loads.
   */
  authenticatorReady(): void;

  /**
   * Send 'switchToFullTab' message to switch the UI from a constrained dialog
   * to a full tab.
   */
  switchToFullTab(url: string): void;

  /**
   * Send 'completeLogin' message to complete login.
   */
  completeLogin(credentials: AuthCompletedCredentials): void;

  /**
   * Send 'lstFetchResults' message.
   * @param arg The string representation of the json data returned by
   *     the sign in dialog after it has finished the sign in process.
   */
  lstFetchResults(arg: string): void;

  /**
   * Send 'metricsHandler:recordAction' message.
   * @param metricsAction The action to be recorded.
   */
  recordAction(metricsAction: string): void;

  /** Send 'showIncognito' message to the handler */
  showIncognito(): void;

  /** Send 'dialogClose' message to close the login dialog. */
  dialogClose(): void;
}

export class InlineLoginBrowserProxyImpl implements InlineLoginBrowserProxy {
  initialize() {
    chrome.send('initialize');
  }

  authenticatorReady() {
    chrome.send('authenticatorReady');
  }

  switchToFullTab(url: string) {
    chrome.send('switchToFullTab', [url]);
  }

  completeLogin(credentials: AuthCompletedCredentials) {
    chrome.send('completeLogin', [credentials]);
  }

  lstFetchResults(arg: string) {
    chrome.send('lstFetchResults', [arg]);
  }

  recordAction(metricsAction: string) {
    chrome.send('metricsHandler:recordAction', [metricsAction]);
  }

  showIncognito() {
    chrome.send('showIncognito');
  }

  dialogClose() {
    chrome.send('dialogClose');
  }

  static getInstance(): InlineLoginBrowserProxy {
    return instance || (instance = new InlineLoginBrowserProxyImpl());
  }

  static setInstance(obj: InlineLoginBrowserProxy) {
    instance = obj;
  }
}

let instance: InlineLoginBrowserProxy|null = null;
