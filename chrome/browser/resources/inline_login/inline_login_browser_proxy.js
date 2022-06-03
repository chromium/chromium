// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {addSingletonGetter, sendWithPromise} from 'chrome://resources/js/cr.m.js';

import {AuthCompletedCredentials} from '../gaia_auth_host/authenticator.m.js';

/** @interface */
export class InlineLoginBrowserProxy {
  /** Send 'initialize' message to prepare for starting auth. */
  initialize() {}

  /**
   * Send 'authExtensionReady' message to handle tasks after auth extension
   * loads.
   */
  authExtensionReady() {}

  /**
   * Send 'switchToFullTab' message to switch the UI from a constrained dialog
   * to a full tab.
   * @param {!string} url
   */
  switchToFullTab(url) {}

  /**
   * Send 'completeLogin' message to complete login.
   * @param {!AuthCompletedCredentials} credentials
   */
  completeLogin(credentials) {}

  /**
   * Send 'lstFetchResults' message.
   * @param {string} arg The string representation of the json data returned by
   * the sign in dialog after it has finished the sign in process.
   */
  lstFetchResults(arg) {}

  /**
   * Send 'metricsHandler:recordAction' message.
   * @param {string} metricsAction The action to be recorded.
   */
  recordAction(metricsAction) {}

  /** Send 'showIncognito' message to the handler */
  showIncognito() {}

  /**
   * Send 'getAccounts' message to the handler. The promise will be resolved
   * with the list of emails of accounts in session.
   * @return {Promise<Array<string>>}
   */
  getAccounts() {}

  /** Send 'dialogClose' message to close the login dialog. */
  dialogClose() {}

  // <if expr="chromeos">
  /**
   * Send 'skipWelcomePage' message to the handler.
   * @param {boolean} skip Whether the welcome page should be skipped.
   */
  skipWelcomePage(skip) {}
  // </if>
}

/** @implements {InlineLoginBrowserProxy} */
export class InlineLoginBrowserProxyImpl {
  /** @override */
  initialize() {
    chrome.send('initialize');
  }

  /** @override */
  authExtensionReady() {
    chrome.send('authExtensionReady');
  }

  /** @override */
  switchToFullTab(url) {
    chrome.send('switchToFullTab', [url]);
  }

  /** @override */
  completeLogin(credentials) {
    chrome.send('completeLogin', [credentials]);
  }

  /** @override */
  lstFetchResults(arg) {
    chrome.send('lstFetchResults', [arg]);
  }

  /** @override */
  recordAction(metricsAction) {
    chrome.send('metricsHandler:recordAction', [metricsAction]);
  }

  /** @override */
  showIncognito() {
    chrome.send('showIncognito');
  }

  /** @override */
  getAccounts() {
    return sendWithPromise('getAccounts');
  }

  /** @override */
  dialogClose() {
    chrome.send('dialogClose');
  }

  // <if expr="chromeos">
  /** @override */
  skipWelcomePage(skip) {
    chrome.send('skipWelcomePage', [skip]);
  }
  // </if>
}

addSingletonGetter(InlineLoginBrowserProxyImpl);
