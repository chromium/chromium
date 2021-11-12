// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// #import {assert, assertNotReached} from '//resources/js/assert.m.js';

/**
 * @fileoverview web view loader.
 */

const CLEAR_ANCHORS_CONTENT_SCRIPT = {
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

// WebViewLoader assists on the process of loading an URL into a webview.
// It listens for events from the webRequest API for the given URL and
// calls load_failure_callback case of failure.
/* #export */ class WebViewLoader {
  /**
   * @suppress {missingProperties} as WebView type has no addContentScripts
   */
  constructor(webview, load_failure_callback, clear_anchors, inject_css) {
    assert(webview.tagName === 'WEBVIEW');

    // Do not create multiple loaders.
    if (WebViewLoader.instances[webview.id])
      return WebViewLoader.instances[webview.id];

    this.webview_ = webview;
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

    this.webview_.request.onErrorOccurred.addListener(() => {
      if (this.loadFailureCallback_)
        this.loadFailureCallback_();
    }, {urls: ['<all_urls>'], types: ['main_frame']});

    // The only instance of the WebViewLoader.
    WebViewLoader.instances[webview.id] = this;
  }

  // Sets an URL to be loaded in the webview. If the URL is different from the
  // previous one, it will be immediately loaded. If the URL is the same as
  // the previous one, it will be reloaded. If requests are under way, the
  // reload will be performed once the current requests are finished.
  setUrl(url) {
    assert(/^https?:\/\//.test(url));

    this.url_ = url;
    this.loadOnline();
  }

  loadOnline() {
    if (this.webview_.src === this.url_)
      this.webview_.reload();
    else
      this.webview_.src = this.url_;
  }
}

WebViewLoader.instances = {};
