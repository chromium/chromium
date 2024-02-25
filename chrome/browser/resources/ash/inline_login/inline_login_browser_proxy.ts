// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {AuthCompletedCredentials} from 'chrome://chrome-signin/gaia_auth_host/authenticator.js';
import {sendWithPromise} from 'chrome://resources/js/cr.js';

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
   * Send 'metricsHandler:recordAction' message.
   * @param metricsAction The action to be recorded.
   */
  recordAction(metricsAction: string): void;

  /** Send 'showIncognito' message to the handler */
  showIncognito(): void;

  /**
   * Send 'getAccounts' message to the handler. The promise will be resolved
   * with the list of emails of accounts in session.
   */
  getAccounts(): Promise<string[]>;

  /**
   * Sends 'getDeviceId' message to the handler. The promise will be resolved
   * with the device identifier for this user.
   */
  getDeviceId(): Promise<string>;

  /** Send 'dialogClose' message to close the login dialog. */
  dialogClose(): void;

  /**
   * Send 'skipWelcomePage' message to the handler.
   * @param skip Whether the welcome page should be skipped.
   */
  skipWelcomePage(skip: boolean): void;

  /** Send 'openGuestWindow' message to the handler */
  openGuestWindow(): void;

  /**
   * @return JSON-encoded dialog arguments.
   */
  getDialogArguments(): string|null;
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

  recordAction(metricsAction: string) {
    chrome.send('metricsHandler:recordAction', [metricsAction]);
  }

  showIncognito() {
    chrome.send('showIncognito');
  }

  getAccounts() {
    return sendWithPromise('getAccounts');
  }

  getDeviceId() {
    return sendWithPromise('getDeviceId');
  }

  dialogClose() {
    chrome.send('dialogClose');
  }

  skipWelcomePage(skip: boolean) {
    chrome.send('skipWelcomePage', [skip]);
  }

  openGuestWindow() {
    chrome.send('openGuestWindow');
  }

  getDialogArguments() {
    return chrome.getVariableValue('dialogArguments');
  }

  static getInstance(): InlineLoginBrowserProxy {
    return instance || (instance = new InlineLoginBrowserProxyImpl());
  }

  static setInstance(obj: InlineLoginBrowserProxy) {
    instance = obj;
  }
}

let instance: InlineLoginBrowserProxy|null = null;
