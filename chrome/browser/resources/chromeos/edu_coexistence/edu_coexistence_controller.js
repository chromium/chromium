// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {PostMessageAPIServer} from '../../chromeos/add_supervision/post_message_api.m.js';
import {AuthCompletedCredentials, Authenticator, AuthParams} from '../../gaia_auth_host/authenticator.m.js';
import {EduCoexistenceBrowserProxyImpl} from './edu_coexistence_browser_proxy.js';

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
    this.browserProxy_ = EduCoexistenceBrowserProxyImpl.getInstance();


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
   * @param {!AuthParams} data parameters for auth extension.
   */
  loadAuthExtension(data) {
    data.frameUrl = this.flowURL_;
    this.authExtHost_.load(data.authMode, data);
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
    this.browserProxy_.authExtensionReady();
  }

  /**
   * @param {!CustomEvent<!AuthCompletedCredentials>} e
   * @private
   */
  onAuthCompleted_(e) {
    this.authCompletedReceived_ = true;
    this.userInfo_ = e.detail;
    this.browserProxy_.completeLogin(e.detail);
  }

  /**
   * @private
   * Informs API that the parent consent is now valid.
   * @param {!Array} unused Placeholder unused empty parameter.
   */
  consentValid_(unused) {
    this.browserProxy_.consentValid();
  }

  /*
   * @private
   * @param {!Array<string>} An array that contains eduCoexistenceToSVersion.
   * with a boolean indicating that the local account was created.
   */
  consentLogged_(eduCoexistenceToSVersion) {
    return this.browserProxy_.consentLogged(
        this.userInfo_.email, eduCoexistenceToSVersion[0]);
  }

  /**
   * @private
   * Attempts to close the widget hosting the flow.
   */
  requestClose_() {
    this.browserProxy_.dialogClose();
  }

  /**
   * @private
   * Notifies the API that there was an unrecoverable error during the flow.
   * @param {!Array} unused Placeholder unused empty parameter.
   */
  reportError_(unused) {
    this.browserProxy_.error();
    // TODO(yilkal): Show the error ui.
  }
}
