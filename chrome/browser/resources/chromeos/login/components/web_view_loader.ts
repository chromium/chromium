// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from '//resources/js/assert.js';
import {loadTimeData} from '//resources/js/load_time_data.js';

/**
 * @fileoverview web view loader.
 */

export const CLEAR_ANCHORS_CONTENT_SCRIPT = {
  code: 'A=Array.from(document.getElementsByTagName("a"));' +
      'for(let i = 0; i < A.length; ++i) {' +
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
 */
const ONLINE_RETRY_BACKOFF_TIMEOUT_IN_MS = 1000;

/**
 * Histogram name for the first load result UMA metric.
 */
const FIRST_LOAD_RESULT_HISTOGRAM = 'OOBE.WebViewLoader.FirstLoadResult';

/**
 * This enum is tied directly to a UMA enum defined in
 * //tools/metrics/histograms/enums.xml, and should always reflect it (do not
 * change one without changing the other).
 * These values are persisted to logs. Entries should not be renumbered and
 * numeric values should never be reused.
 */
enum OobeWebViewLoadResult {
  SUCCESS = 0,
  LOAD_TIMEOUT = 1,
  LOAD_ERROR = 2,
  HTTP_ERROR = 3,
  MAX = 4,
}

interface WebviewCallback {
  (): void;
}

interface ErrorDetailsListener {
  (details: ErrorDetails): void;
}

interface OnCompletedDetailsListener {
  (details: OnCompletedDetails): void;
}

interface ErrorDetails {
  url?: string;
  error?: string;
}

interface OnCompletedDetails {
  statusCode?: number;
}

interface WebRequestObserver {
  onCompleted:
      chrome.webRequest.WebRequestBaseEvent<OnCompletedDetailsListener>;
  onErrorOccurred: chrome.webRequest.WebRequestBaseEvent<ErrorDetailsListener>;
}



// WebViewLoader assists on the process of loading an URL into a webview.
// It listens for events from the webRequest API for the given URL and
// calls loadFailureCallback case of failure.
// When using WebViewLoader to load a new webview, add the webview id with the
// first character capitalized to the variants of
// `OOBE.WebViewLoader.FirstLoadResult` histogram.
export class WebViewLoader {
  static instances: {[key: string]: WebViewLoader} = {};

  private webview: chrome.webviewTag.WebView;
  private timeout: number;
  private loadFailureCallback: WebviewCallback;
  private isPerformingRequests: boolean;
  private reloadRequested: boolean;
  private loadTimer: number;
  private backOffTimer: number;
  private url: string;
  private loadResultRecorded: boolean;

  constructor(
      webview: chrome.webviewTag.WebView, timeout: number,
      loadFailureCallback: WebviewCallback, clearAnchors: boolean,
      injectCss: boolean) {
    assert(webview.tagName === 'WEBVIEW');

    // Do not create multiple loaders.
    if (WebViewLoader.instances[webview.id]) {
      return WebViewLoader.instances[webview.id];
    }

    this.webview = webview;
    this.timeout = timeout;
    this.isPerformingRequests = false;
    this.reloadRequested = false;
    this.loadTimer = 0;
    this.backOffTimer = 0;
    this.loadFailureCallback = loadFailureCallback;
    this.url = '';
    this.loadResultRecorded = false;

    if (clearAnchors) {
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
    if (injectCss) {
      webview.addEventListener('contentload', () => {
        webview.insertCSS(WEB_VIEW_FONTS_CSS, () => {
          if (chrome.runtime.lastError) {
            console.warn(
                'Failed to insertCSS: ' + chrome.runtime.lastError.message);
          }
        });
      });
    }

    const request = this.webview.request;
    // Monitor webRequests API events
    assert(this.isWebRequestObserver(request));

    (request as WebRequestObserver)
        .onCompleted.addListener(this.onCompleted.bind(this), {
          urls: ['<all_urls>'],
          types: ['main_frame' as chrome.webRequest.ResourceType],
        });

    (request as WebRequestObserver)
        .onErrorOccurred.addListener(this.onErrorOccurred.bind(this), {
          urls: ['<all_urls>'],
          types: ['main_frame' as chrome.webRequest.ResourceType],
        });

    // The only instance of the WebViewLoader.
    WebViewLoader.instances[webview.id] = this;
  }

  private isWebRequestObserver(obj: any): obj is WebRequestObserver {
    return 'onCompleted' in obj && 'onErrorOccurred' in obj;
  }

  // Clears the internal state of the EULA loader. Stops the timeout timer
  // and prevents events from being handled.
  clearInternalState() {
    window.clearTimeout(this.loadTimer);
    window.clearTimeout(this.backOffTimer);
    this.isPerformingRequests = false;
    this.reloadRequested = false;
  }

  // Sets an URL to be loaded in the webview. If the URL is different from the
  // previous one, it will be immediately loaded. If the URL is the same as
  // the previous one, it will be reloaded. If requests are under way, the
  // reload will be performed once the current requests are finished.
  setUrl(url: string) {
    assert(/^https?:\/\//.test(url));

    if (url !== this.url) {
      // Clear the internal state and start with a new URL.
      this.clearInternalState();
      this.url = url;
      this.loadWithFallbackTimer();
    } else {
      // Same URL was requested again. Reload later if a request is under way.
      if (this.isPerformingRequests) {
        this.reloadRequested = true;
      } else {
        this.loadWithFallbackTimer();
      }
    }
  }

  // This method only gets invoked if the webview webRequest API does not
  // fire either 'onErrorOccurred' or 'onCompleted' before the timer runs out.
  // See: https://developer.chrome.com/extensions/webRequest
  onTimeoutError() {
    console.warn('Loading ' + this.url + ' timed out');

    // Return if we are no longer monitoring requests. Confidence check.
    if (!this.isPerformingRequests) {
      return;
    }

    if (!this.loadResultRecorded) {
      this.loadResultRecorded = true;
      this.recordUmaHistogramForFirstLoadResult(
          OobeWebViewLoadResult.LOAD_TIMEOUT);
    }

    if (this.reloadRequested) {
      this.loadWithFallbackTimer();
    } else {
      this.clearInternalState();
      this.loadFailureCallback();
    }
  }

  /**
   * webRequest API Event Handler for 'onErrorOccurred'.
   */
  onErrorOccurred(details: ErrorDetails) {
    console.warn(
        'Failed to load ' + details.url + ' with error ' + details.error);
    if (!this.isPerformingRequests) {
      return;
    }

    if (details && details.error === 'net::ERR_ABORTED') {
      // Retry triggers net::ERR_ABORTED, so ignore it.
      // TODO(crbug.com/1327977): Load an embedded offline copy as a fallback.
      return;
    }

    if (!this.loadResultRecorded) {
      this.loadResultRecorded = true;
      this.recordUmaHistogramForFirstLoadResult(
          OobeWebViewLoadResult.LOAD_ERROR);
    }

    if (this.reloadRequested) {
      this.loadWithFallbackTimer();
    } else {
      this.loadAfterBackoff();
    }
  }

  /**
   * webRequest API Event Handler for 'onCompleted'
   * @suppress {missingProperties} no statusCode for details
   */
  onCompleted(details: OnCompletedDetails) {
    if (!this.isPerformingRequests) {
      return;
    }

    // Http errors such as 4xx, 5xx hit here instead of 'onErrorOccurred'.
    if (details.statusCode !== 200) {
      // Not a successful request. Perform a reload if requested.
      console.info('Loading ' + this.url + ' has completed with HTTP error.');
      if (!this.loadResultRecorded) {
        this.loadResultRecorded = true;
        this.recordUmaHistogramForFirstLoadResult(
            OobeWebViewLoadResult.HTTP_ERROR);
      }

      if (this.reloadRequested) {
        this.loadWithFallbackTimer();
      } else {
        this.loadAfterBackoff();
      }
    } else {
      // Success!
      console.info('Loading ' + this.url + ' has completed successfully.');
      if (!this.loadResultRecorded) {
        this.loadResultRecorded = true;
        this.recordUmaHistogramForFirstLoadResult(
            OobeWebViewLoadResult.SUCCESS);
      }

      this.clearInternalState();
    }
  }

  // Loads the URL into the webview and starts a timer.
  loadWithFallbackTimer() {
    console.info('Trying to load ' + this.url);
    // Clear previous timer and perform a load.
    window.clearTimeout(this.loadTimer);
    this.loadTimer =
        window.setTimeout(this.onTimeoutError.bind(this), this.timeout);
    this.tryLoadOnline();
  }

  loadAfterBackoff() {
    console.info('Trying to reload ' + this.url);
    window.clearTimeout(this.backOffTimer);
    this.backOffTimer = window.setTimeout(
        this.tryLoadOnline.bind(this), ONLINE_RETRY_BACKOFF_TIMEOUT_IN_MS);
  }

  tryLoadOnline() {
    this.reloadRequested = false;

    // A request is being made
    this.isPerformingRequests = true;
    if (this.webview.src === this.url) {
      this.webview.reload();
    } else {
      this.webview.src = this.url;
    }
  }

  recordUmaHistogramForFirstLoadResult(result: number) {
    const id = this.webview.id[0].toUpperCase() + this.webview.id.slice(1);
    const histogramName = FIRST_LOAD_RESULT_HISTOGRAM + '.' + id;
    chrome.send(
        'metricsHandler:recordInHistogram',
        [histogramName, result, OobeWebViewLoadResult.MAX]);
  }
}
