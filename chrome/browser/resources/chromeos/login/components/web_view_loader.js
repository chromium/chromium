// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// #import {assert, assertNotReached} from '//resources/js/assert.m.js';

/**
 * @fileoverview web view loader.
 */

/* #export */ const CLEAR_ANCHORS_CONTENT_SCRIPT = {
  code: 'A=Array.from(document.getElementsByTagName("a"));' +
      'for(var i = 0; i < A.length; ++i) {' +
      '  const el = A[i];' +
      '  let e = document.createElement("span");' +
      '  if (el.textContent.trim().length > 0) {' +
      '    e.textContent=el.textContent + "(" + el.href + ")";' +
      '  }' +
      '  el.parentNode.replaceChild(e,el);' +
      '}'
};

const WEB_VIEW_FONTS_CSS = {
  code: `body * {
        font-family: Roboto, sans-serif !important;
        font-size: 13px !important;
        line-height: 20px !important;}
       body h2 {
         font-size: 15px !important;
         line-height: 22px !important;}`
};

/**
 * Timeout between consequent loads of online webview.
 * @type {number}
 */
const ONLINE_RETRY_BACKOFF_TIMEOUT_IN_MS = 1000;

// WebViewLoader assists on the process of loading an URL into a webview.
// It listens for events from the webRequest API for the given URL and
// calls load_failure_callback case of failure.
/* #export */ class WebViewLoader {
  /**
   * @suppress {missingProperties} as WebView type has no addContentScripts
   */
  constructor(
      webview, timeout, load_failure_callback, clear_anchors, inject_css) {
    assert(webview.tagName === 'WEBVIEW');

    // Do not create multiple loaders.
    if (WebViewLoader.instances[webview.id])
      return WebViewLoader.instances[webview.id];

    this.webview_ = webview;
    this.timeout_ = timeout;
    this.isPerformingRequests_ = false;
    this.reloadRequested_ = false;
    this.loadTimer_ = 0;
    this.backOffTimer_ = 0;
    this.loadFailureCallback_ = load_failure_callback;
    this.url_ = '';

    if (clear_anchors) {
      // Add the CLEAR_ANCHORS_CONTENT_SCRIPT that will clear <a><\a>
      // (anchors) in order to prevent any navigation in the webview itself.
      webview.addContentScripts([{
        name: 'clearAnchors',
        matches: ['<all_urls>'],
        js: CLEAR_ANCHORS_CONTENT_SCRIPT,
      }]);
      webview.addEventListener('contentload', () => {
        webview.executeScript(CLEAR_ANCHORS_CONTENT_SCRIPT);
      });
    }
    if (inject_css) {
      webview.addEventListener('contentload', () => {
        webview.insertCSS(WEB_VIEW_FONTS_CSS);
      });
    }

    // Monitor webRequests API events
    this.webview_.request.onCompleted.addListener(
        this.onCompleted_.bind(this),
        {urls: ['<all_urls>'], types: ['main_frame']});
    this.webview_.request.onErrorOccurred.addListener(
        this.onErrorOccurred_.bind(this),
        {urls: ['<all_urls>'], types: ['main_frame']});

    // The only instance of the WebViewLoader.
    WebViewLoader.instances[webview.id] = this;
  }

  // Clears the internal state of the EULA loader. Stops the timeout timer
  // and prevents events from being handled.
  clearInternalState() {
    window.clearTimeout(this.loadTimer_);
    window.clearTimeout(this.backOffTimer_);
    this.isPerformingRequests_ = false;
    this.reloadRequested_ = false;
  }

  // Sets an URL to be loaded in the webview. If the URL is different from the
  // previous one, it will be immediately loaded. If the URL is the same as
  // the previous one, it will be reloaded. If requests are under way, the
  // reload will be performed once the current requests are finished.
  setUrl(url) {
    assert(/^https?:\/\//.test(url));

    if (url != this.url_) {
      // Clear the internal state and start with a new URL.
      this.clearInternalState();
      this.url_ = url;
      this.loadWithFallbackTimer();
    } else {
      // Same URL was requested again. Reload later if a request is under way.
      if (this.isPerformingRequests_)
        this.reloadRequested_ = true;
      else
        this.loadWithFallbackTimer();
    }
  }

  // This method only gets invoked if the webview webRequest API does not
  // fire either 'onErrorOccurred' or 'onCompleted' before the timer runs out.
  // See: https://developer.chrome.com/extensions/webRequest
  onTimeoutError_() {
    // Return if we are no longer monitoring requests. Confidence check.
    if (!this.isPerformingRequests_)
      return;

    if (this.reloadRequested_) {
      this.loadWithFallbackTimer();
    } else {
      this.clearInternalState();
      this.loadFailureCallback_();
    }
  }

  /**
   * webRequest API Event Handler for 'onErrorOccurred'.
   * @param {!Object} details
   */
  onErrorOccurred_(details) {
    if (!this.isPerformingRequests_)
      return;

    if (this.reloadRequested_)
      this.loadWithFallbackTimer();
    else
      this.loadAfterBackoff();
  }

  /**
   * webRequest API Event Handler for 'onCompleted'
   * @suppress {missingProperties} no statusCode for details
   * @param {!Object} details
   */
  onCompleted_(details) {
    if (!this.isPerformingRequests_)
      return;

    // Http errors such as 4xx, 5xx hit here instead of 'onErrorOccurred'.
    if (details.statusCode != 200) {
      // Not a successful request. Perform a reload if requested.
      if (this.reloadRequested_)
        this.loadWithFallbackTimer();
      else
        this.loadAfterBackoff();
    } else {
      // Success!
      this.clearInternalState();
    }
  }

  // Loads the URL into the webview and starts a timer.
  loadWithFallbackTimer() {
    // Clear previous timer and perform a load.
    window.clearTimeout(this.loadTimer_);
    this.loadTimer_ =
        window.setTimeout(this.onTimeoutError_.bind(this), this.timeout_);
    this.tryLoadOnline();
  }

  loadAfterBackoff() {
    window.clearTimeout(this.backOffTimer_);
    this.backOffTimer_ = window.setTimeout(
        this.tryLoadOnline.bind(this), ONLINE_RETRY_BACKOFF_TIMEOUT_IN_MS);
  }

  tryLoadOnline() {
    this.reloadRequested_ = false;

    // A request is being made
    this.isPerformingRequests_ = true;
    if (this.webview_.src === this.url_)
      this.webview_.reload();
    else
      this.webview_.src = this.url_;
  }
}

WebViewLoader.instances = {};
