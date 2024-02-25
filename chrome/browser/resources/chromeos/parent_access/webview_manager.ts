// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

type ShouldSendTokenToUrlFunction = (token: string) => boolean;
type AllowedRequestFunction = (request: string) => boolean;
type OnBeforeSendHeadersListener = (obj: any) =>
    chrome.webRequest.BlockingResponse|null;

/**
 * Class that handles managing and configuring a <webview> for use
 * for embedded hosted web apps in system components.  Handles
 * settings options to provide for the security and control of the
 * content in the webview.  Using this class doesn't precluding configuring
 * the webview outside of this class.  Rather, this class is intended to
 * standardize some common configurations of webview.
 */
export class WebviewManager {
  private webview_: chrome.webviewTag.WebView;
  /**
   * Tracks the current listener used to filter destinations
   * to which we send access tokens.
   */
  private shouldSendTokenToUrlListener_: OnBeforeSendHeadersListener|null;
  /**
   * Tracks the current listener used to filter destinations
   * to which we send allow requests.
   */
  private allowedRequestListener_: OnBeforeSendHeadersListener|null;

  constructor(webview: chrome.webviewTag.WebView) {
    this.webview_ = webview;
  }

  /**
   * Configures the webview to use the specified token to authenticate the user.
   * Sets the token as part of the Auhtorization: Bearer HTTP header.
   * @param {string} accessToken the access token
   * @param {!function(string):boolean} shouldSendTokenToUrlFn function that
   *     returns true if the access token should be sent to the specified host.
   */
  setAccessToken(
      accessToken: string,
      shouldSendTokenToUrlFn: ShouldSendTokenToUrlFunction) {
    if (this.shouldSendTokenToUrlListener_) {
      this.webview_.request.onBeforeSendHeaders.removeListener(
          this.shouldSendTokenToUrlListener_);
      this.shouldSendTokenToUrlListener_ = null;
    }
    this.shouldSendTokenToUrlListener_ = (details) => {
      if (shouldSendTokenToUrlFn(details.url)) {
        details.requestHeaders.push({
          name: 'Authorization',
          value: 'Bearer ' + accessToken,
        });
      }

      return {requestHeaders: details.requestHeaders};
    };

    this.webview_.request.onBeforeSendHeaders.addListener(
        this.shouldSendTokenToUrlListener_, {urls: ['<all_urls>']},
        ['blocking', 'requestHeaders']);
  }

  /**
   * Configures the webview to permit navigation only to URLs allowed
   * by the specified function.
   * @param {!function(string):boolean} allowedRequestFn function that returns
   *     true if the request to the specified URL is allowed.
   */
  setAllowRequestFn(allowedRequestFn: AllowedRequestFunction) {
    if (this.allowedRequestListener_) {
      this.webview_.request.onBeforeSendHeaders.removeListener(
          this.allowedRequestListener_);
      this.allowedRequestListener_ = null;
    }
    this.allowedRequestListener_ = (details) => {
      return {cancel: !allowedRequestFn(details.url)};
    };

    this.webview_.request.onBeforeRequest.addListener(
        this.allowedRequestListener_, {urls: ['<all_urls>']}, ['blocking']);
  }
}
