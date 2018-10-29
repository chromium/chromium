// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Web view helper.
 */

/** Web view helper shared between OOBE screens. */
class WebViewHelper {
  /**
   * Loads content of the given url into the given web view.
   * The content is loaded via XHR and is sent to web view via data url so that
   * it is properly sandboxed.
   *
   * @param {!WebView} webView web view element to host the content.
   * @param {string} url URL to load the content from.
   * @param {!ContentType} contentType type of the content to load.
   */
  static loadUrlContentToWebView(webView, url, contentType) {
    assert(webView.tagName === 'WEBVIEW');

    const onError = function() {
      webView.src = 'about:blank';
    };

    /**
     * Sets contents to web view.
     * Prefixes data with appropriate scheme, MIME type and token.
     * @param {string} data data string to set.
     */
    const setContents = function(data) {
      switch (contentType) {
        case WebViewHelper.ContentType.HTML:
          webView.src =
              'data:text/html;charset=utf-8,' + encodeURIComponent(data);
          break;
        case WebViewHelper.ContentType.PDF:
          webView.src = 'data:application/pdf;base64,' + data;
          break;
        default:
          assertNotReached('Unknown content type to load.');
      }
    };

    var xhr = new XMLHttpRequest();
    xhr.open('GET', url);
    xhr.setRequestHeader('Accept', contentType);
    xhr.onreadystatechange = function() {
      if (xhr.readyState != XMLHttpRequest.DONE)
        return;
      if (xhr.status != 200) {
        onError();
        return;
      }

      var responseContentType = xhr.getResponseHeader('Content-Type');
      if (responseContentType && !responseContentType.includes(contentType)) {
        onError();
        return;
      }
      setContents(xhr.response);
    };

    try {
      xhr.send();
    } catch (e) {
      onError();
    }
  }
}

/**
 * Type of content to load into web view.
 * @enum {string}
 */
WebViewHelper.ContentType = {
  /** UTF-8 encoded text/html content type. */
  HTML: 'text/html',
  /** Base64 encoded application/pdf content type. */
  PDF: 'application/pdf',
};