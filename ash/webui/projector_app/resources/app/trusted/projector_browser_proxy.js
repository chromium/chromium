// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {sendWithPromise} from 'chrome://resources/ash/common/cr.m.js';

/**
 * To use the browser proxy, please import this module and call
 * ProjectorBrowserProxyImpl.getInstance().*
 *
 * @interface
 */
export class ProjectorBrowserProxy {
  /**
   * Gets the list of primary and secondary accounts currently available on the
   * device.
   * @return {Promise<Array<!projectorApp.Account>>}
   */
  getAccounts() {}

  /**
   * Gets the oauth token with the required scopes for the specified account.
   * @param {string} email, user's email.
   * @return {!Promise<!projectorApp.OAuthToken>}
   */
  getOAuthTokenForAccount(email) {}

  /**
   * Sends 'error' message to handler.
   * The Handler will log the message. If the error is not a recoverable error,
   * the handler closes the corresponding WebUI.
   * @param {!Array<string>} msg Error messages.
   */
  onError(msg) {}

  /**
   * Gets information about the specified video from DriveFS.
   * @param {string} videoFileId The Drive item id of the video file.
   * @param {string|undefined} resourceKey The Drive item resource key.
   * @return {!Promise<!projectorApp.Video>}
   */
  getVideo(videoFileId, resourceKey) {}
}

/**
 * @type {ProjectorBrowserProxyImpl}
 */
let browserProxy;

/**
 * @implements {ProjectorBrowserProxy}
 */
export class ProjectorBrowserProxyImpl {
  /**
   * @returns {ProjectorBrowserProxyImpl}
   */
  static getInstance() {
    if (!browserProxy) {
      browserProxy = new ProjectorBrowserProxyImpl();
    }
    return browserProxy;
  }

  /** @override */
  getAccounts() {
    return sendWithPromise('getAccounts');
  }

  /** @override */
  getOAuthTokenForAccount(email) {
    return sendWithPromise('getOAuthTokenForAccount', [email]);
  }

  /** @override */
  onError(msg) {
    return chrome.send('onError', msg);
  }

  /** @override */
  getVideo(videoFileId, resourceKey) {
    return sendWithPromise('getVideo', [videoFileId, resourceKey]);
  }
}
