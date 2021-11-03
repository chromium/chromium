// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {addSingletonGetter, sendWithPromise} from 'chrome://resources/js/cr.m.js';

/**
 * To use the browser proxy, please import this module and call
 * ProjectorBrowserProxyImpl.getInstance().*
 *
 * @interface
 */
export class ProjectorBrowserProxy {
  /**
   * Notifies the embedder content that tool has been set for annotator.
   * @param {!projectorApp.AnnotatorToolParams} tool
   */
  onToolSet(tool) {}

  /**
   * Notifies the embedder content that undo/redo availability changed for
   * annotator.
   * @param {boolean} undoAvailable
   * @param {boolean} redoAvailable
   */
  onUndoRedoAvailabilityChanged(undoAvailable, redoAvailable) {}

  /**
   * Gets the list of primary and secondary accounts currently available on the
   * device.
   * @return {Promise<Array<!projectorApp.Account>>}
   */
  getAccounts() {}

  /**
   * Checks whether the SWA can trigger a new Projector session.
   * @return {Promise<boolean>}
   */
  canStartProjectorSession() {}

  /**
   * Launches the Projector recording session. Returns true if a projector
   * recording session was successfully launched.
   * @param {string} storageDir, the directory name in which the screen cast
   *     will be saved in.
   * @return {Promise<boolean>}
   */
  startProjectorSession(storageDir) {}

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
   * Send XHR request.
   * @param {string} url the request URL.
   * @param {string} method the request method.
   * @param {string=} requestBody the request body data.
   * @param {boolean=} useCredentials authorize the request with end user
   *     credentials. Used for getting streaming URL.
   * @return {!Promise<!projectorApp.XhrResponse>}
   */
  sendXhr(url, method, requestBody, useCredentials) {}

  /**
   * Return true if the "new screencast" button should be shown to the user.
   * @return {!Promise<boolean>}
   */
  shouldShowNewScreencastButton() {}

  /**
   * Returns true if the "install speech recognition" button should be shown to
   * the user.
   * @return {!Promise<boolean>}
   */
  shouldDownloadSoda() {}

  /**
   * Triggers the installation of on device speech recognition binary and
   * language packs for the user's locale. Returns true if download and
   * installation started.
   * @return {!Promise<boolean>}
   */
  installSoda() {}

  /**
   * Gets the list of pending screencasts that are uploading to drive.
   * @return {Promise<Array<projectorApp.PendingScreencast>>}
   */
  // TODO(b/204372280): return
  // "Promise<!Array<!projectorApp.PendingScreencast>>"
  getPendingScreencasts() {}
}

/**
 * @implements {ProjectorBrowserProxy}
 */
export class ProjectorBrowserProxyImpl {
  /** @override */
  onToolSet(tool) {
    return chrome.send('onToolSet', [tool]);
  }

  /** @override */
  onUndoRedoAvailabilityChanged(undoAvailable, redoAvailable) {
    return chrome.send(
        'onUndoRedoAvailabilityChanged', [undoAvailable, redoAvailable]);
  }

  /** @override */
  getAccounts() {
    return sendWithPromise('getAccounts');
  }

  /** @override */
  canStartProjectorSession() {
    return sendWithPromise('canStartProjectorSession');
  }

  /** @override */
  startProjectorSession(storageDir ) {
    return sendWithPromise('startProjectorSession', [storageDir]);
  }

  /** @override */
  getOAuthTokenForAccount(email) {
    return sendWithPromise('getOAuthTokenForAccount', [email]);
  }

  /** @override */
  onError(msg) {
    return chrome.send("onError", msg);
  }

  /** @override */
  sendXhr(url, method, requestBody, useCredentials) {
    return sendWithPromise(
        'sendXhr', [url, method, requestBody, useCredentials]);
  }

  /** @override */
  shouldShowNewScreencastButton() {
    return sendWithPromise('shouldShowNewScreencastButton');
  }

  /** @override */
  shouldDownloadSoda() {
    return sendWithPromise('shouldDownloadSoda');
  }

  /** @override */
  installSoda() {
    return sendWithPromise('installSoda');
  }
  /** @override */
  getPendingScreencasts() {
    return sendWithPromise('getPendingScreencasts');
  }
}

addSingletonGetter(ProjectorBrowserProxyImpl);
