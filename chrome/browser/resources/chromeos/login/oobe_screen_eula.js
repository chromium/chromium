// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Oobe eula screen implementation.
 */

login.createScreen('EulaScreen', 'eula', function() {
  var CLEAR_ANCHORS_CONTENT_SCRIPT = {
    code: 'A=Array.from(document.getElementsByTagName("a"));' +
        'for(var i = 0; i < A.length; ++i) {' +
        '  const el = A[i];' +
        '  let e = document.createElement("span");' +
        '  e.textContent=el.textContent;' +
        '  el.parentNode.replaceChild(e,el);' +
        '}'
  };

  // EulaLoader assists on the process of loading an URL into a webview.
  // It listens for events from the webRequest API for the given URL and loads
  // an offline version of the EULA in case of failure.
  // Calling setURL() multiple times with the same URL while requests are being
  // made won't affect current requests. Instead, it will mark the flag
  // 'reloadRequested' for the given URL. The reload will be performed only if
  // the current requests fail. This prevents webview-loadAbort events from
  // being fired and unnecessary reloads.
  class EulaLoader {
    constructor(webview, timeout, load_offline_callback) {
      assert(webview.tagName === 'WEBVIEW');

      // Do not create multiple loaders.
      if (EulaLoader.instance_) {
        return EulaLoader.instance_;
      }

      this.webview_ = webview;
      this.timeout_ = timeout;
      this.isPerformingRequests_ = false;
      this.reloadRequested_ = false;
      this.loadOfflineCallback_ = load_offline_callback;
      this.loadTimer_ = 0;

      // Add the CLEAR_ANCHORS_CONTENT_SCRIPT that will clear <a><\a> (anchors)
      // in order to prevent any navigation in the webview itself.
      webview.addContentScripts([{
        name: 'clearAnchors',
        matches: ['<all_urls>'],
        js: CLEAR_ANCHORS_CONTENT_SCRIPT,
      }]);
      webview.addEventListener('contentload', () => {
        webview.executeScript(CLEAR_ANCHORS_CONTENT_SCRIPT);
      });

      // Monitor webRequests API events
      this.webview_.request.onCompleted.addListener(
          this.onCompleted_.bind(this),
          {urls: ['<all_urls>'], types: ['main_frame']});
      this.webview_.request.onErrorOccurred.addListener(
          this.onErrorOccurred_.bind(this),
          {urls: ['<all_urls>'], types: ['main_frame']});

      // The only instance of the EulaLoader.
      EulaLoader.instance_ = this;
    }

    // Clears the internal state of the EULA loader. Stops the timeout timer
    // and prevents events from being handled.
    clearInternalState() {
      window.clearTimeout(this.loadTimer_);
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
      // Return if we are no longer monitoring requests. Sanity check.
      if (!this.isPerformingRequests_)
        return;

      this.reloadIfRequestedOrLoadOffline();
    }

    // Loads the offline version of the EULA.
    tryLoadOffline() {
      this.clearInternalState();
      if (this.loadOfflineCallback_)
        this.loadOfflineCallback_();
    }

    // Only process events for the current URL and when performing requests.
    shouldProcessEvent(details) {
      return this.isPerformingRequests_ && (details.url === this.url_);
    }

    // webRequest API Event Handler for 'onErrorOccurred'
    onErrorOccurred_(details) {
      if (!this.shouldProcessEvent(details))
        return;

      this.reloadIfRequestedOrLoadOffline();
    }

    // webRequest API Event Handler for 'onCompleted'
    onCompleted_(details) {
      if (!this.shouldProcessEvent(details))
        return;

      // Http errors such as 4xx, 5xx hit here instead of 'onErrorOccurred'.
      if (details.statusCode != 200) {
        // Not a successful request. Perform a reload if requested.
        this.reloadIfRequestedOrLoadOffline();
      } else {
        // Success!
        this.clearInternalState();
      }
    }

    // Loads the URL into the webview and starts a timer.
    loadWithFallbackTimer() {
      // A request is being made
      this.isPerformingRequests_ = true;

      // Clear previous timer and perform a load.
      window.clearTimeout(this.loadTimer_);
      this.loadTimer_ =
          window.setTimeout(this.onTimeoutError_.bind(this), this.timeout_);

      if (this.webview_.src === this.url_)
        this.webview_.reload();
      else
        this.webview_.src = this.url_;
    }

    // Tries to perform a reload if it was requested, otherwise load offline.
    reloadIfRequestedOrLoadOffline() {
      if (this.reloadRequested_) {
        this.reloadRequested_ = false;
        this.loadWithFallbackTimer();
      } else {
        this.tryLoadOffline();
      }
    }
  }

  return {
    /** @override */
    decorate: function() {
      $('oobe-eula-md').screen = this;
    },

    /**
     * Called from $('oobe-eula-md') onUsageChanged handler.
     * @param {boolean} value $('usage-stats').checked value.
     */
    onUsageStatsClicked_: function(value) {
      chrome.send('EulaScreen.usageStatsEnabled', [value]);
    },

    /**
     * Event handler that is invoked when 'chrome://terms' is loaded.
     */
    onFrameLoad: function() {
      $('eula').classList.remove('eula-loading');
    },

    /**
     * Event handler that is invoked just before the screen is shown.
     * @param {object} data Screen init payload.
     */
    onBeforeShow: function() {
      $('eula').classList.add('eula-loading');
      this.updateLocalizedContent();
    },

    /**
     * Returns a control which should receive an initial focus.
     */
    get defaultControl() {
      return $('oobe-eula-md');
    },

    enableKeyboardFlow: function() {},

    /**
     * Updates localized content of the screen that is not updated via template.
     */
    updateLocalizedContent: function() {
      // Reload the terms contents.
      $('oobe-eula-md').updateLocalizedContent();
    },

    /**
     * Sets TPM password.
     * @param {text} password TPM password to be shown.
     */
    setTpmPassword: function(password) {
      $('oobe-eula-md').password = password;
    },

    /**
     * Load Eula into the given webview. Online version is attempted first with
     * a timeout. If it fails to load, fallback to chrome://terms. The loaded
     * terms contents is then set to the webview via data url. Webview is
     * used as a sandbox for both online and local contents. Data url is
     * used for chrome://terms so that webview never needs to have the
     * privileged webui bindings.
     *
     * @param {!WebView} webview Webview element to host the terms.
     */
    loadEulaToWebview_: function(webview) {
      assert(webview.tagName === 'WEBVIEW');

      /**
       * Timeout to load online Eula.
       * @type {number}
       */
      var ONLINE_EULA_LOAD_TIMEOUT_IN_MS = 7000;

      /**
       * URL to use when online page is not available.
       * @type {string}
       */
      var TERMS_URL = 'chrome://terms';

      var loadBundledEula = function() {
        WebViewHelper.loadUrlContentToWebView(
            webview, TERMS_URL, WebViewHelper.ContentType.HTML);
      };

      var onlineEulaUrl = loadTimeData.getString('eulaOnlineUrl');
      if (!onlineEulaUrl) {
        loadBundledEula();
        return;
      }

      // Load online Eula with a timeout to fallback to the offline version.
      // This won't construct multiple EulaLoaders. Single instance.
      var eulaLoader = new EulaLoader(
          webview, ONLINE_EULA_LOAD_TIMEOUT_IN_MS, loadBundledEula);
      eulaLoader.setUrl(onlineEulaUrl);
    },

    /**
     * Called when focus is returned.
     */
    onFocusReturned: function() {
      $('oobe-eula-md').focus();
    },
  };
});
