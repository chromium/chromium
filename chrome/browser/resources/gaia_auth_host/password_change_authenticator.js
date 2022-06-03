// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// <include src="saml_handler.js">
// Note: webview_event_manager.js is already included by saml_handler.js.

/**
 * @fileoverview Support password change on with SAML provider.
 */

cr.define('cr.samlPasswordChange', function() {
  'use strict';

  /** @const */
  const oktaInjectedScriptName = 'oktaInjected';

  /**
   * "SAML password change extension" which helps detect password change
   *  @type {string}
   */
  const extensionId = 'mkmjngkgbjeljoblnahkagdlcdeiiped';

  /**
   * The script to inject into Okta user settings page.
   * @type {string}
   */
  const oktaInjectedJs = String.raw`
      // <include src="okta_detect_success_injected.js">
  `;

  const BLANK_PAGE_URL = 'about:blank';


  /**
   * @param {string} extensionId The ID of the extension to send the message to.
   * @param {any} message The message to send. This message should be a
   *     JSON-ifiable object.
   * @param {function} callback the response callback function
   * @private
   * @see: https://developer.chrome.com/extensions/runtime#method-sendMessage
   */
  function sendMessage_(extensionId, message, callback) {
    // Sending message to extension and callback will be used to receive
    // response from extension. This way is used to send one time request :
    // https://developer.chrome.com/extensions/messaging#simple
    chrome.runtime.sendMessage(extensionId, message, callback);
  }

  /**
   * The different providers of password-change pages that we support, or are
   * working on supporting.
   * Should match the enum in SAML password change extension
   * @enum {number}
   */
  const PasswordChangePageProvider = {
    UNKNOWN: 0,
    ADFS: 1,
    AZURE: 2,
    OKTA: 3,
    PING: 4,
  };

  /**
   * @param {URL?} url The url of the webpage that is being interacted with.
   * @return {PasswordChangePageProvider} The provider of the password change
   *         page, as detected based on the URL.
   */
  function detectProvider_(url) {
    if (!url) {
      return null;
    }
    if (url.pathname.match(/\/updatepassword\/?$/)) {
      return PasswordChangePageProvider.ADFS;
    }
    if (url.pathname.endsWith('/ChangePassword.aspx')) {
      return PasswordChangePageProvider.AZURE;
    }
    if (url.host.match(/\.okta\.com$/)) {
      return PasswordChangePageProvider.OKTA;
    }
    if (url.pathname.match('/password/chg/') ||
        url.pathname.match('/pwdchange/')) {
      return PasswordChangePageProvider.PING;
    }
    return PasswordChangePageProvider.UNKNOWN;
  }

  /**
   * @param {string?} str A string that should be a valid URL.
   * @return {URL?} A valid URL object, or null.
   */
  function safeParseUrl_(str) {
    try {
      return new URL(str);
    } catch (error) {
      console.error('Invalid url: ' + str);
      return null;
    }
  }

  /**
   * @param {URL?} postUrl Where the password change request was POSTed.
   * @param {URL?} redirectUrl Where the response redirected the browser.
   * @return {boolean} True if we detect that a password change was successful.
   */
  function detectPasswordChangeSuccess(postUrl, redirectUrl) {
    if (!postUrl || !redirectUrl) {
      return false;
    }

    // We count it as a success whenever "status=0" is in the query params.
    // This is what we use for ADFS, but for now, we allow it for every IdP, so
    // that an otherwise unsupported IdP can also send it as a success message.
    // TODO(https://crbug.com/930109): Consider removing this entirely, or,
    // using a more self-documenting parameter like 'passwordChanged=1'.
    if (redirectUrl.searchParams.get('status') == '0') {
      return true;
    }

    const pageProvider = detectProvider_(postUrl);
    // These heuristics work for the following SAML IdPs:
    if (pageProvider == PasswordChangePageProvider.ADFS) {
      return redirectUrl.searchParams.get('status') == '0';
    }
    if (pageProvider == PasswordChangePageProvider.AZURE) {
      return redirectUrl.searchParams.get('ReturnCode') == '0';
    }
    if (pageProvider == PasswordChangePageProvider.PING) {
      // The returnurl is always preserved until password change succeeds - then
      // it is no longer needed.
      return (!!postUrl.searchParams.get('returnurl') &&
              !redirectUrl.searchParams.get('returnurl')) ||
          redirectUrl.pathname.endsWith('Success');
    }

    // We can't currently detect success for Okta just by inspecting the
    // URL or even response headers. To inspect the response body, we need
    // to inject scripts onto their page (see okta_detect_success_injected.js).

    return false;
  }

  /**
   * Initializes the authenticator component.
   */
  class Authenticator extends cr.EventTarget {
    /**
     * @param {webview|string} webview The webview element or its ID to host
     *     IdP web pages.
     */
    constructor(webview) {
      super();

      this.initialFrameUrl_ = null;
      this.webviewEventManager_ = WebviewEventManager.create();

      this.bindToWebview_(webview);

      window.addEventListener('focus', this.onFocus_.bind(this), false);
    }

    /**
     * Reinitializes saml handler.
     */
    resetStates() {
      this.samlHandler_.reset();
    }

    /**
     * Resets the webview to the blank page.
     */
    resetWebview() {
      if (this.webview_.src && this.webview_.src != BLANK_PAGE_URL) {
        this.webview_.src = BLANK_PAGE_URL;
      }
    }

    /**
     * Binds this authenticator to the passed webview.
     * @param {!Object} webview the new webview to be used by this
     *     Authenticator.
     * @private
     */
    bindToWebview_(webview) {
      assert(!this.webview_);
      assert(!this.samlHandler_);

      this.webview_ = typeof webview == 'string' ? $(webview) : webview;

      this.samlHandler_ =
          new cr.login.SamlHandler(this.webview_, true /* startsOnSamlPage */);
      this.webviewEventManager_.addEventListener(
          this.samlHandler_, 'authPageLoaded',
          this.onAuthPageLoaded_.bind(this));

      // Listen for main-frame redirects to check for success - we can mostly
      // detect success by detecting we POSTed something to the password-change
      // URL, and the response redirected us to a particular success URL.
      this.webviewEventManager_.addWebRequestEventListener(
          this.webview_.request.onBeforeRedirect,
          this.onBeforeRedirect_.bind(this),
          {urls: ['*://*/*'], types: ['main_frame']},
      );

      // Inject a custom script for detecting password change success in Okta.
      this.webview_.addContentScripts([{
        name: oktaInjectedScriptName,
        matches: ['*://*.okta.com/*'],
        js: {code: oktaInjectedJs},
        all_frames: true,
        run_at: 'document_start'
      }]);

      // Connect to the script running in Okta web pages once it loads.
      this.webviewEventManager_.addWebRequestEventListener(
          this.webview_.request.onCompleted,
          this.onOktaCompleted_.bind(this),
          {urls: ['*://*.okta.com/*'], types: ['main_frame']},
      );

      // Okta-detect-success-inject script signals success by posting a message
      // that says "passwordChangeSuccess", which we listen for:
      this.webviewEventManager_.addEventListener(
          window, 'message', this.onMessageReceived_.bind(this));
    }

    /**
     * Unbinds this Authenticator from the currently bound webview.
     * @private
     */
    unbindFromWebview_() {
      assert(this.webview_);
      assert(this.samlHandler_);

      this.webviewEventManager_.removeAllListeners();

      this.webview_ = undefined;
      this.samlHandler_.unbindFromWebview();
      this.samlHandler_ = undefined;
    }

    /**
     * Re-binds to another webview.
     * @param {Object} webview the new webview to be used by this Authenticator.
     */
    rebindWebview(webview) {
      this.unbindFromWebview_();
      this.bindToWebview_(webview);
    }

    /**
     * Loads the authenticator component with the given parameters.
     * @param {AuthMode} authMode Authorization mode.
     * @param {Object} data Parameters for the authorization flow.
     */
    load(data) {
      this.resetStates();
      this.initialFrameUrl_ = this.constructInitialFrameUrl_(data);
      this.samlHandler_.blockInsecureContent = true;
      this.webview_.src = this.initialFrameUrl_;
    }

    constructInitialFrameUrl_(data) {
      let url;
      url = data.passwordChangeUrl;
      if (data.userName) {
        url = appendParam(url, 'username', data.userName);
      }
      return url;
    }

    /**
     * Invoked when the sign-in page takes focus.
     * @param {object} e The focus event being triggered.
     * @private
     */
    onFocus_(e) {
      this.webview_.focus();
    }

    /**
     * Sends scraped password and resets the state.
     * @param {bool} isOkta whether the page is Okta page.
     * @private
     */
    onPasswordChangeSuccess_(isOkta) {
      let passwordsOnce;
      let passwordsTwice;
      if (isOkta) {
        passwordsOnce = this.samlHandler_.getPasswordsWithPropertyScrapedTimes(
            1, 'oldPassword');
        const newPasswords =
            this.samlHandler_.getPasswordsWithPropertyScrapedTimes(
                1, 'newPassword');
        const verifyPasswords =
            this.samlHandler_.getPasswordsWithPropertyScrapedTimes(
                1, 'verifyPassword');
        if (newPasswords.length == 1 && verifyPasswords.length == 1 &&
            newPasswords[0] === verifyPasswords[0]) {
          passwordsTwice = Array.from(newPasswords);
        } else {
          passwordsTwice = [];
        }
      } else {
        passwordsOnce =
            this.samlHandler_.getPasswordsWithPropertyScrapedTimes(1);
        passwordsTwice =
            this.samlHandler_.getPasswordsWithPropertyScrapedTimes(2);
      }

      this.dispatchEvent(new CustomEvent('authCompleted', {
        detail: {
          old_passwords: passwordsOnce,
          new_passwords: passwordsTwice,
        }
      }));
      this.resetStates();
    }

    /**
     * Invoked when |samlHandler_| fires 'authPageLoaded' event.
     * @private
     */
    onAuthPageLoaded_(e) {
      this.webview_.focus();
    }

    /**
     * Invoked when a new document loading completes.
     * @param {Object} details The web-request details.
     * @private
     */
    onBeforeRedirect_(details) {
      if (details.method == 'POST') {
        const message = {
          name: 'detectPasswordChangeSuccess',
          url: details.url,
          redirectUrl: details.redirectUrl
        };
        sendMessage_(extensionId, message, (passwordChangeSuccess) => {
          // SAML change password extension will be used to detect the password
          // change success from url passed.
          // 'passwordChangeSuccess' will be equal to undefined in case
          // extension isn't installed or disabled, In this case normal flow
          // will be used.
          // Otherwise 'passwordChangeSuccess' will indcate whether extension
          // detected password change successfully.
          if (passwordChangeSuccess ||
              (typeof passwordChangeSuccess === 'undefined' &&
               detectPasswordChangeSuccess(
                   safeParseUrl_(details.url),
                   safeParseUrl_(details.redirectUrl)))) {
            this.onPasswordChangeSuccess_(false /* isOkta */);
          }
        });
      }
    }

    /**
     * Invoked when loading completes on an Okta page.
     * @param {Object} details The web-request details.
     * @private
     */
    onOktaCompleted_(details) {
      // Okta_detect_success_injected.js needs to be contacted by the parent,
      // so that it can send messages back to the parent.
      // Using setTimeout gives the page time to finish initializing.
      // TODO: timeout value is chosen empirically, we need a better way
      // to pass this to the injected code.
      setTimeout(() => {
        this.webview_.contentWindow.postMessage('connect', details.url);
      }, 2000);
    }

    /**
     * Invoked when the webview posts a message.
     * @param {Object} event The message event.
     * @private
     */
    onMessageReceived_(event) {
      if (event.data == 'passwordChangeSuccess') {
        const message = {name: 'detectProvider', url: event.origin};
        sendMessage_(extensionId, message, (provider) => {
          // SAML change password extension will be used to detect provider
          // from url passed.
          // 'provider' will be equal to undefined in case
          // extension isn't installed or disabled, In this case normal flow
          // will be used.
          if (provider == PasswordChangePageProvider.OKTA ||
              (typeof provider === 'undefined' &&
               detectProvider_(safeParseUrl_(event.origin)) ==
                   PasswordChangePageProvider.OKTA)) {
            this.onPasswordChangeSuccess_(true /* isOkta */);
          }
        });
      }
    }
  }

  return {
    Authenticator: Authenticator,
    detectPasswordChangeSuccess: detectPasswordChangeSuccess,
  };
});
