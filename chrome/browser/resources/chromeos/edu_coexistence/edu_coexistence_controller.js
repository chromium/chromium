// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {PostMessageApiServer} from 'chrome://resources/ash/common/post_message_api/post_message_api_server.js';

import {AuthCompletedCredentials, Authenticator, AuthParams} from '../../gaia_auth_host/authenticator.js';

import {EduCoexistenceBrowserProxyImpl} from './edu_coexistence_browser_proxy.js';

const MILLISECONDS_PER_SECOND = 1000;

/**
 * @typedef {{
 *   hl: (string),
 *   url: (string),
 *   clientId: (string),
 *   sourceUi: (string),
 *   clientVersion: (string),
 *   eduCoexistenceAccessToken: (string),
 *   eduCoexistenceId: (string),
 *   platformVersion: (string),
 *   releaseChannel: (string),
 *   deviceId: (string),
 *   email: (string|undefined),
 *   readOnlyEmail: (string|undefined),
 *   signinTime: (number),
 * }}
 */
export let EduCoexistenceParams;


/**
 * Constructs the EDU Coexistence URL.
 * @param {!EduCoexistenceParams} params Parameters for the flow.
 * @return {URL}
 */
function constructEduCoexistenceUrl(params) {
  const url = new URL(params.url);
  url.searchParams.set('hl', params.hl);
  url.searchParams.set('source_ui', params.sourceUi);
  url.searchParams.set('client_id', params.clientId);
  url.searchParams.set('client_version', params.clientVersion);
  url.searchParams.set('edu_coexistence_id', params.eduCoexistenceId);
  url.searchParams.set('platform_version', params.platformVersion);
  url.searchParams.set('release_channel', params.releaseChannel);
  url.searchParams.set('device_id', params.deviceId);
  if (params.email) {
    url.searchParams.set('email', params.email);
    if (params.readOnlyEmail) {
      url.searchParams.set('read_only_email', params.readOnlyEmail);
    }
  }
  return url;
}

/**
 * Class that orchestrates the EDU Coexistence signin flow.
 */
export class EduCoexistenceController extends PostMessageApiServer {
  /**
   * @param {!Element} ui Polymer object edu-coexistence-ui
   * @param {!Element} webview  The <webview> element to listen to as a
   *     client.
   * @param {!EduCoexistenceParams} params  The params for the flow.
   */
  constructor(ui, webview, params) {
    const flowURL = constructEduCoexistenceUrl(params);
    const protocol = flowURL.hostname === 'localhost' ? 'http://' : 'https://';
    const originURLPrefix = protocol + flowURL.host;
    super(webview, originURLPrefix, originURLPrefix);

    this.ui = ui;
    this.isOobe_ = params.sourceUi === 'oobe';
    this.flowURL_ = flowURL;
    this.originURLPrefix_ = originURLPrefix;
    this.webview_ = webview;
    this.userInfo_ = null;
    this.authCompletedReceived_ = false;
    this.browserProxy_ = EduCoexistenceBrowserProxyImpl.getInstance();
    this.eduCoexistenceAccessToken_ = params.eduCoexistenceAccessToken;
    this.signinTime_ = new Date(params.signinTime);

    this.webview_.request.onBeforeSendHeaders.addListener(
        (details) => {
          if (this.originMatchesFilter(details.url)) {
            details.requestHeaders.push({
              name: 'Authorization',
              value: 'Bearer ' + this.eduCoexistenceAccessToken_,
            });
          }

          return {requestHeaders: details.requestHeaders};
        },

        {urls: ['<all_urls>']}, ['blocking', 'requestHeaders']);

    /**
     * The state of the guest content, saved as requested by
     * the guest content to ensure that its state outlives content
     * reload events, which destroy the state of the guest content.
     * The value itself is opaque encoded binary data.
     * @private {?Uint8Array}
     */
    this.guestFlowState_ = null;

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

  /** @override */
  onInitializationError(origin) {
    this.reportError_(
        ['Error initializing communication channel with origin:' + origin]);
  }

  /** @return {boolean} */
  getIsOobe() {
    return this.isOobe_;
  }


  /**
   * Returns the hostname of the origin of the flow's URL (the one it was
   * initialized with, not its current URL).
   * @return {string}
   */
  getFlowOriginHostname() {
    return this.flowURL_.hostname;
  }

  /** @private */
  initializeAfterDomLoaded_() {
    this.isDomLoaded_ = true;
    // Register methods with PostMessageAPI.
    this.registerMethod('consentValid', this.consentValid_.bind(this));
    this.registerMethod('consentLogged', this.consentLogged_.bind(this));
    this.registerMethod('requestClose', this.requestClose_.bind(this));
    this.registerMethod('reportError', this.reportError_.bind(this));
    this.registerMethod(
        'saveGuestFlowState', this.saveGuestFlowState_.bind(this));
    this.registerMethod(
        'fetchGuestFlowState', this.fetchGuestFlowState_.bind(this));
    this.registerMethod(
        'getEduAccountEmail', this.getEduAccountEmail_.bind(this));
    this.registerMethod(
        'getTimeDeltaSinceSigninSeconds',
        this.getTimeDeltaSinceSigninSeconds_.bind(this));

    // Add listeners for Authenticator.
    this.addAuthExtHostListeners_();
  }

  /**
   * Loads the flow into the controller.
   * @param {!AuthParams} data parameters for auth extension.
   */
  loadAuthExtension(data) {
    // We use the Authenticator to set the web flow URL instead
    // of setting it ourselves, so that the content isn't loaded twice.
    // This is why this class doesn't directly set webview.src_ (except in
    // onAuthCompleted below to handle the corner case of loading
    // accounts.google.com for running against webserver running on localhost).
    // The EDU Coexistence web flow will be responsible for constructing
    // and forwarding to the accounts.google.com URL that Authenticator
    // interacts with.
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
        'getAccounts', () => this.onGetAccounts_());
    this.authExtHost_.addEventListener(
        'authCompleted',
        e => this.onAuthCompleted_(
            /** @type {!CustomEvent<!AuthCompletedCredentials>} */ (e)));
  }

  /** @private */
  onAuthReady_() {
    this.browserProxy_.authExtensionReady();
  }

  /** @private */
  onGetAccounts_() {
    this.browserProxy_.getAccounts().then(result => {
      this.authExtHost_.getAccountsResponse(result);
    });
  }

  /** @private */
  onAuthCompleted_(e) {
    this.authCompletedReceived_ = true;
    this.userInfo_ = e.detail;
    this.browserProxy_.completeLogin(e.detail);

    // The EDU Signin page doesn't forward to the next page on success, so we have
    // to manually update the src to continue to the last page of the flow.
    const finishURL = this.flowURL_;
    finishURL.pathname = '/supervision/coexistence/finish';
    this.webview_.src = finishURL.toString();
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

  /*
   * @private
   * @param {!Array<Uint8Array>} An array that contains guest flow state in its
   *    first element.
   */
  saveGuestFlowState_(guestFlowState) {
    this.guestFlowState_ = guestFlowState[0];
  }

  /**
   * @param {!Array} unused Placeholder unused empty parameter.
   * @return {?Object}  The guest flow state previously saved
   *     using saveGuestFlowState().
   */
  fetchGuestFlowState_(unused) {
    return {'state': this.guestFlowState_};
  }

  /**
   * @param {!Array} unused Placeholder unused empty parameter.
   * @return {!String}  The edu-account email that is being added to the device.
   */
  getEduAccountEmail_(unused) {
    console.assert(this.userInfo_);
    return this.userInfo_.email;
  }

  /**
   * @private
   * Notifies the API that there was an unrecoverable error during the flow.
   * @param {!Array<string>} error An array that contains the error message at
   *     index 0.
   */
  reportError_(error) {
    // Notify the app to switch to error screen.
    this.ui.fire('go-error');

    // Send the error strings to C++ handler so they are logged.
    this.browserProxy_.onError(error);
  }

  /**
   * @private
   * @return {number} Returns the number of seconds that have elapsed since
   * the user's initial signin.
   */
  getTimeDeltaSinceSigninSeconds_() {
    return (Date.now() - this.signinTime_) / MILLISECONDS_PER_SECOND;
  }
}
