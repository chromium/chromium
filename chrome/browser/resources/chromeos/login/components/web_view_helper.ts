// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert, assertNotReached} from '//resources/js/assert.js';

/**
 * Type of content to load into web view.
 */
export enum ContentType {
  /** UTF-8 encoded text/html content type. */
  HTML ='text/html',
  /** Base64 encoded application/pdf content type. */
  PDF = 'application/pdf',
}

/** Web view helper shared between OOBE screens. */
export class WebViewHelper {
  /**
   * Loads content of the given url into the given web view.
   * The content is loaded via XHR and is sent to web view via data url so that
   * it is properly sandboxed.
   *
   * webView is a WebView element to host the content.
   * url URL to load the content from.
   * contentType type of the content to load.
   */
  static loadUrlContentToWebView(webView: chrome.webviewTag.WebView,
        url: string, contentType: ContentType): void {
    assert(webView.tagName === 'WEBVIEW');

    const onError = function(): void {
      webView.src = 'about:blank';
    };

    /**
     * Sets contents to web view.
     * Prefixes data with appropriate scheme, MIME type and token.
     *
     * data is the string to set.
     */
    const setContents = function(data: string): void {
      switch (contentType) {
        case ContentType.HTML:
          webView.src =
              'data:text/html;charset=utf-8,' + encodeURIComponent(data);
          break;
        case ContentType.PDF:
          webView.src = 'data:application/pdf;base64,' + data;
          break;
        default:
          assertNotReached('Unknown content type to load.');
      }
    };

    const xhr = new XMLHttpRequest();
    xhr.open('GET', url);
    xhr.setRequestHeader('Accept', contentType);
    xhr.onreadystatechange = function() {
      if (xhr.readyState !== XMLHttpRequest.DONE) {
        return;
      }
      if (xhr.status !== 200) {
        onError();
        return;
      }

      const responseContentType = xhr.getResponseHeader('Content-Type');
      if (responseContentType && !responseContentType.includes(contentType)) {
        onError();
        return;
      }
      const contents = xhr.responseText;
      setContents(contents);
    };

    try {
      xhr.send();
    } catch (e) {
      onError();
    }
  }
}
