// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {PostMessageAPIServer} from '../../chromeos/add_supervision/post_message_api.m.js';
import {AuthCompletedCredentials, Authenticator, AuthParams} from '../../gaia_auth_host/authenticator.m.js';

/**
 * The methods to expose to the hosted content via the PostMessageAPI.
 */
const METHOD_LIST = ['consentValid', 'consentLogged', 'requestClose', 'error'];


/**
 * @typedef {{
 *   hl: (string|undefined),
 *   url: (string|undefined),
 *   clientId: (string|undefined),
 *   sourceUi: (string|undefined),
 *   clientVersion: (string|undefined),
 *   eduCoexistenceAccessToken: (string|undefined),
 *   eduCoexistenceId: (string|undefined),
 *   platformVersion: (string|undefined),
 *   releaseChannel: (string|undefined),
 * }}
 */
let EduCoexistenceParams;


/* Constructs the EDU Coexistence URL.
 * @param {!EduCoexistenceParams} params Parameters for the flow.
 * @return {URL}
 */
function constructEduCoexistenceUrl(params) {
  const url = new URL(params.url);
  url.searchParams.set('hl', params.hl);
  url.searchParams.set('source_ui', params.sourceUi);
  url.searchParams.set('client_version', params.clientVersion);
  url.searchParams.set('edu_coexistence_id', params.eduCoexistenceId);
  url.searchParams.set('platform_version', params.platformVersion);
  url.searchParams.set('release_channel', params.releaseChannel);
  return url;
}

/**
 * Class that orchestrates the EDU Coexistence signin flow.
 */
export class EduCoexistenceController extends PostMessageAPIServer {
  /**
   * @param {!Element} webview  The <webview> element to listen to as a
   *     client.
   * @param {!EduCoexistenceParams} params  The params for the flow.
   */
  constructor(webview, params) {
    const flowURL = constructEduCoexistenceUrl(params);
    const originURLPrefix = 'https://' + flowURL.host;
    super(webview, METHOD_LIST, flowURL, originURLPrefix);

    this.flowURL_ = flowURL;
    this.originURLPrefix_ = originURLPrefix;
    this.webview_ = webview;
    this.userInfo_ = null;
    this.authCompletedReceived_ = false;

    // TODO(danan):  Set auth tokens in appropriate headers.

    /**
     * The auth extension host instance.
     * @private {Authenticator}
     */
    this.authExtHost_ = new Authenticator(
        /** @type {!WebView} */ (this.webview_));

    /**
     * @type {boolean}
     * @private
     */
    this.isDomLoaded_ = document.readyState !== 'loading';
    if (this.isDomLoaded_) {
      this.initializeAfterDomLoaded_();
    } else {
      document.addEventListener(
          'DOMContentLoaded', this.initializeAfterDomLoaded_.bind(this));
    }
  }

  /** @private */
  initializeAfterDomLoaded_() {
    this.isDomLoaded_ = true;
    // Register methods with PostMessageAPI.
    this.registerMethod('consentValid', this.consentValid_.bind(this));
    this.registerMethod('consentLogged', this.consentLogged_.bind(this));
    this.registerMethod('requestClose', this.requestClose_.bind(this));
    this.registerMethod('reportError', this.reportError_.bind(this));

    // Add listeners for Authenticator.
    this.addAuthExtHostListeners_();
  }

  /**
   * Loads the flow into the controller.
   */
  load() {
    this.webview_.src = this.flowURL_.toString();
  }

  /**
   * Resets the internal state of the controller.
   */
  reset() {
    this.userInfo_ = null;
    this.authCompletedReceived_ = false;
  }

  /** @private */
  addAuthExtHostListeners_() {
    this.authExtHost_.addEventListener('ready', () => this.onAuthReady_());
    this.authExtHost_.addEventListener(
        'authCompleted',
        e => this.onAuthCompleted_(
            /** @type {!CustomEvent<!AuthCompletedCredentials>} */ (e)));
  }

  /** @private */
  onAuthReady_() {
    console.error('Got onAuthReady_');
    // TODO(danan): do whatever is required after authenticator initialization.
  }

  /** @private */
  onAuthCompleted_(e) {
    this.authCompletedReceived_ = true;
    console.error('Got onAuthCompleted_');
    this.userInfo_ = e.details;
  }

  /**
   * @private
   * Informs API that the parent consent is now valid.
   * @param {!Array} unused Placeholder unused empty parameter.
   */
  consentValid_(unused) {
    // TODO(danan): Set up object to wait for GAIA EDU Login page load, by
    // observing for a page reload using this.webview_.request , and then
    // this.authExtHost_.load(); Return promise acknowledging receipt.
    console.error('Got consentValid_');
    return Promise.resolve();
  }

  /*
   * @private
   * @param {!Array} unused Placeholder unused empty parameter.
   * @return {Promise <{accountCreated: boolean}>} Returns a promise
   * with a boolean indicating that the local account was created.
   */
  consentLogged_(unused) {
    // TODO(danan): Send message to owner indicating that the flow successfully
    // completed
    console.error('Got consentLogged_');
    return Promise.resolve();
  }

  /**
   * @private
   * Attempts to close the widget hosting the flow.
   * @return {Promise <{closed: boolean}>} If the widget is not closed
   * this promise will resolve with boolean result indicating whether the
   * dialog was closed.
   */
  requestClose_() {
    // TODO(danan): Attempt to close the widget hosting the flow.
    console.error('Got requestClose_');
    return Promise.resolve();
  }

  /**
   * @private
   * Notifies the API that there was an unrecoverable error during the flow.
   * @param {!Array} unused Placeholder unused empty parameter.
   */
  reportError_(unused) {
    // TODO(danan): Pass the error back up the stack.
    console.error('Got reportError_');
    return Promise.resolve();
  }
}
