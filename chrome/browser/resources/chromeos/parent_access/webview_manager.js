// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Class that handles managing and configuring a <webview> for use
 * for embedded hosted web apps in system components.  Handles
 * settings options to provide for the security and control of the
 * content in the webview.  Using this class doesn't precluding configuring
 * the webview outside of this class.  Rather, this class is intended to
 * standardize some common configurations of webview.
 */
export class WebviewManager {
  /** @param {!WebView} webview The webview to manage. */
  constructor(webview) {
    /**
     * @private {!WebView} the webview element that this class
     *     will manage.
     */
    this.webview_ = webview;

    /**
     * Tracks the current function used to filter destinations
     * to which we send access tokens.
     * @private {?function(string):boolean}
     */
    this.shouldSendTokenToUrlFn_ = null;

    /**
     * Tracks the current function used to filter destinations
     * to which we send allow requests.
     * @private {?function(string):boolean}
     */
    this.allowedRequestFn_ = null;
  }

  /**
   * Configures the webview to use the specified token to authenticate the user.
   * Sets the token as part of the Auhtorization: Bearer HTTP header.
   * @param {string} accessToken the access token
   * @param {!function(string):boolean} shouldSendTokenToUrlFn function that
   *     returns true if the access token should be sent to the specified host.
   */
  setAccessToken(accessToken, shouldSendTokenToUrlFn) {
    if (this.shouldSendTokenToUrlFn_) {
      this.webview_.request.onBeforeSendHeaders.removeListener(
          this.shouldSendTokenToUrlFn_);
      this.shouldSendTokenToUrlFn_ = null;
    }
    this.shouldSendTokenToUrlFn_ = shouldSendTokenToUrlFn;

    this.webview_.request.onBeforeSendHeaders.addListener(
        (details) => {
          if (this.shouldSendTokenToUrlFn_(details.url)) {
            details.requestHeaders.push({
              name: 'Authorization',
              value: 'Bearer ' + accessToken,
            });
          }

          return {requestHeaders: details.requestHeaders};
        },

        {urls: ['<all_urls>']}, ['blocking', 'requestHeaders']);
  }

  /**
   * Configures the webview to permit navigation only to URLs allowed
   * by the specified function.
   * @param {!function(string):boolean} allowedRequestFn function that returns
   *     true if the request to the specified URL is allowed.
   */
  setAllowRequestFn(allowedRequestFn) {
    if (this.allowedRequestFn_) {
      this.webview_.request.onBeforeSendHeaders.removeListener(
          this.allowedRequestFn_);
      this.allowedRequestFn_ = null;
    }
    this.allowedRequestFn_ = allowedRequestFn;

    this.webview_.request.onBeforeRequest.addListener((details) => {
      return {cancel: !this.allowedRequestFn_(details.url)};
    }, {urls: ['<all_urls>']}, ['blocking']);
  }
}
