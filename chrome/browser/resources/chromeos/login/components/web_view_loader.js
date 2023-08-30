// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert, assertNotReached} from '//resources/ash/common/assert.js';

import {loadTimeData} from '../i18n_setup.js';

/**
 * @fileoverview web view loader.
 */

export const CLEAR_ANCHORS_CONTENT_SCRIPT = {
  code: 'A=Array.from(document.getElementsByTagName("a"));' +
      'for(var i = 0; i < A.length; ++i) {' +
      '  const el = A[i];' +
      '  let e = document.createElement("span");' +
      '  if (el.textContent.trim().length > 0) {' +
      '    e.textContent=el.textContent + "(" + el.href + ")";' +
      '  }' +
      '  el.parentNode.replaceChild(e,el);' +
      '}',
};

const GENERATE_FONTS_CSS = () => {
  const isOobeJellyEnabled = loadTimeData.getBoolean('isOobeJellyEnabled');
  if (!isOobeJellyEnabled) {
    return {
      code: `body * {
            font-family: Roboto, sans-serif !important;
            font-size: 13px !important;
            line-height: 20px !important;}
            body h2 {
             font-size: 15px !important;
             line-height: 22px !important;}`,
    };
  }

  return {
    // 'body *' values correspond to the body2 typography token.
    // 'body h2' values correspond to the button2 typography token.
    code: `body * {
      font-family: 'Google Sans Text Regular', 'Google Sans', 'Roboto', sans-serif !important;
      font-size: 13px !important;
      font-weight: 400 !important;
      line-height: 20px !important;}
      body h2 {
       font-family: 'Google Sans Text Medium', 'Google Sans', 'Roboto', sans-serif !important;
       font-size: 13px !important;
       font-weight: 500 !important;
       line-height: 20px !important;}`,
  };
};

const WEB_VIEW_FONTS_CSS = GENERATE_FONTS_CSS();

/**
 * Timeout between consequent loads of online webview.
 * @type {number}
 */
const ONLINE_RETRY_BACKOFF_TIMEOUT_IN_MS = 1000;

/**
 * Histogram name for the first load result UMA metric.
 * @type {string}
 */
const FIRST_LOAD_RESULT_HISTOGRAM = 'OOBE.WebViewLoader.FirstLoadResult';

/**
 * This enum is tied directly to a UMA enum defined in
 * //tools/metrics/histograms/enums.xml, and should always reflect it (do not
 * change one without changing the other).
 * These values are persisted to logs. Entries should not be renumbered and
 * numeric values should never be reused.
 * @enum {number}
 */
const OobeWebViewLoadResult = {
  SUCCESS: 0,
  LOAD_TIMEOUT: 1,
  LOAD_ERROR: 2,
  HTTP_ERROR: 3,
  MAX: 4,
};


// WebViewLoader assists on the process of loading an URL into a webview.
// It listens for events from the webRequest API for the given URL and
// calls load_failure_callback case of failure.
// When using WebViewLoader to load a new webview, add the webview id with the
// first character capitalized to the variants of
// `OOBE.WebViewLoader.FirstLoadResult` histogram.
export class WebViewLoader {
  /**
   * @suppress {missingProperties} as WebView type has no addContentScripts
   */
  constructor(
      webview, timeout, load_failure_callback, clear_anchors, inject_css) {
    assert(webview.tagName === 'WEBVIEW');

    // Do not create multiple loaders.
    if (WebViewLoader.instances[webview.id]) {
      return WebViewLoader.instances[webview.id];
    }

    this.webview_ = webview;
    this.timeout_ = timeout;
    this.isPerformingRequests_ = false;
    this.reloadRequested_ = false;
    this.loadTimer_ = 0;
    this.backOffTimer_ = 0;
    this.loadFailureCallback_ = load_failure_callback;
    this.url_ = '';
    this.loadResultRecorded_ = false;

    if (clear_anchors) {
      // Add the CLEAR_ANCHORS_CONTENT_SCRIPT that will clear <a><\a>
      // (anchors) in order to prevent any navigation in the webview itself.
      webview.addEventListener('contentload', () => {
        webview.executeScript(CLEAR_ANCHORS_CONTENT_SCRIPT, () => {
          if (chrome.runtime.lastError) {
            console.warn(
                'Clear anchors script failed: ' +
                chrome.runtime.lastError.message);
          }
        });
      });
    }
    if (inject_css) {
      webview.addEventListener('contentload', () => {
        webview.insertCSS(WEB_VIEW_FONTS_CSS, () => {
          if (chrome.runtime.lastError) {
            console.warn(
                'Failed to insertCSS: ' + chrome.runtime.lastError.message);
          }
        });
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
      if (this.isPerformingRequests_) {
        this.reloadRequested_ = true;
      } else {
        this.loadWithFallbackTimer();
      }
    }
  }

  // This method only gets invoked if the webview webRequest API does not
  // fire either 'onErrorOccurred' or 'onCompleted' before the timer runs out.
  // See: https://developer.chrome.com/extensions/webRequest
  onTimeoutError_() {
    console.warn('Loading ' + this.url_ + ' timed out');

    // Return if we are no longer monitoring requests. Confidence check.
    if (!this.isPerformingRequests_) {
      return;
    }

    if (!this.loadResultRecorded_) {
      this.loadResultRecorded_ = true;
      this.RecordUMAHistogramForFirstLoadResult_(
          OobeWebViewLoadResult.LOAD_TIMEOUT);
    }

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
    console.warn(
        'Failed to load ' + details.url + ' with error ' + details.error);
    if (!this.isPerformingRequests_) {
      return;
    }

    if (details && details.error == 'net::ERR_ABORTED') {
      // Retry triggers net::ERR_ABORTED, so ignore it.
      // TODO(crbug.com/1327977): Load an embedded offline copy as a fallback.
      return;
    }

    if (!this.loadResultRecorded_) {
      this.loadResultRecorded_ = true;
      this.RecordUMAHistogramForFirstLoadResult_(
          OobeWebViewLoadResult.LOAD_ERROR);
    }

    if (this.reloadRequested_) {
      this.loadWithFallbackTimer();
    } else {
      this.loadAfterBackoff();
    }
  }

  /**
   * webRequest API Event Handler for 'onCompleted'
   * @suppress {missingProperties} no statusCode for details
   * @param {!Object} details
   */
  onCompleted_(details) {
    if (!this.isPerformingRequests_) {
      return;
    }

    // Http errors such as 4xx, 5xx hit here instead of 'onErrorOccurred'.
    if (details.statusCode != 200) {
      // Not a successful request. Perform a reload if requested.
      console.info('Loading ' + this.url_ + ' has completed with HTTP error.');
      if (!this.loadResultRecorded_) {
        this.loadResultRecorded_ = true;
        this.RecordUMAHistogramForFirstLoadResult_(
            OobeWebViewLoadResult.HTTP_ERROR);
      }

      if (this.reloadRequested_) {
        this.loadWithFallbackTimer();
      } else {
        this.loadAfterBackoff();
      }
    } else {
      // Success!
      console.info('Loading ' + this.url_ + ' has completed successfully.');
      if (!this.loadResultRecorded_) {
        this.loadResultRecorded_ = true;
        this.RecordUMAHistogramForFirstLoadResult_(
            OobeWebViewLoadResult.SUCCESS);
      }

      this.clearInternalState();
    }
  }

  // Loads the URL into the webview and starts a timer.
  loadWithFallbackTimer() {
    console.info('Trying to load ' + this.url_);
    // Clear previous timer and perform a load.
    window.clearTimeout(this.loadTimer_);
    this.loadTimer_ =
        window.setTimeout(this.onTimeoutError_.bind(this), this.timeout_);
    this.tryLoadOnline();
  }

  loadAfterBackoff() {
    console.info('Trying to reload ' + this.url_);
    window.clearTimeout(this.backOffTimer_);
    this.backOffTimer_ = window.setTimeout(
        this.tryLoadOnline.bind(this), ONLINE_RETRY_BACKOFF_TIMEOUT_IN_MS);
  }

  tryLoadOnline() {
    this.reloadRequested_ = false;

    // A request is being made
    this.isPerformingRequests_ = true;
    if (this.webview_.src === this.url_) {
      this.webview_.reload();
    } else {
      this.webview_.src = this.url_;
    }
  }

  RecordUMAHistogramForFirstLoadResult_(result) {
    const id = this.webview_.id[0].toUpperCase() + this.webview_.id.slice(1);
    const histogramName = FIRST_LOAD_RESULT_HISTOGRAM + '.' + id;
    chrome.send(
        'metricsHandler:recordInHistogram',
        [histogramName, result, OobeWebViewLoadResult.MAX]);
  }
}

WebViewLoader.instances = {};
