// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(function() {
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

const EULA_FONTS_CSS = {
  code: `body * {
        font-family: Roboto, sans-serif !important;
        font-size: 13px !important;
        line-height: 20px !important;}
       body h2 {
         font-size: 15px !important;
         line-height: 22px !important;}`
};

/**
 * Timeout to load online Eula.
 * @type {number}
 */
const ONLINE_EULA_LOAD_TIMEOUT_IN_MS = 7000;

/**
 * Timeout between consequent loads of online Eula.
 * @type {number}
 */
const ONLINE_EULA_RETRY_BACKOFF_TIMEOUT_IN_MS = 1000;

/**
 * URL to use when online page is not available.
 * @type {string}
 */
const TERMS_URL = 'chrome://terms';

// EulaLoader assists on the process of loading an URL into a webview.
// It listens for events from the webRequest API for the given URL and
// loads an offline version of the EULA in case of failure. Calling
// setURL() multiple times with the same URL while requests are being made
// won't affect current requests. Instead, it will mark the flag
// 'reloadRequested' for the given URL. The reload will be performed only
// if the current requests fail. This prevents webview-loadAbort events
// from being fired and unnecessary reloads.
class EulaLoader {
  /**
   * @suppress {missingProperties} as WebView type has no addContentScripts
   */
  constructor(webview, timeout, load_offline_callback, clear_anchors) {
    assert(webview.tagName === 'WEBVIEW');

    // Do not create multiple loaders.
    if (EulaLoader.instances[webview.id]) {
      return EulaLoader.instances[webview.id];
    }

    this.webview_ = webview;
    this.timeout_ = timeout;
    this.isPerformingRequests_ = false;
    this.reloadRequested_ = false;
    this.loadOfflineCallback_ = load_offline_callback;
    this.loadTimer_ = 0;
    this.backOffTimer_ = 0;
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
    webview.addEventListener('contentload', () => {
      webview.insertCSS(EULA_FONTS_CSS);
    });

    // Monitor webRequests API events
    this.webview_.request.onCompleted.addListener(
        this.onCompleted_.bind(this),
        {urls: ['<all_urls>'], types: ['main_frame']});
    this.webview_.request.onErrorOccurred.addListener(
        this.onErrorOccurred_.bind(this),
        {urls: ['<all_urls>'], types: ['main_frame']});

    // The only instance of the EulaLoader.
    EulaLoader.instances[webview.id] = this;
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
    // Return if we are no longer monitoring requests. Sanity check.
    if (!this.isPerformingRequests_)
      return;

    if (this.reloadRequested_)
      this.loadWithFallbackTimer();
    else
      this.tryLoadOffline();
  }

  // Loads the offline version of the EULA.
  tryLoadOffline() {
    this.clearInternalState();
    if (this.loadOfflineCallback_)
      this.loadOfflineCallback_();
  }

  /**
   * Only process events for the current URL and when performing requests.
   * @param {!Object} details
   */
  shouldProcessEvent(details) {
    return this.isPerformingRequests_ && (details.url === this.url_);
  }

  /**
   * webRequest API Event Handler for 'onErrorOccurred'
   * @param {!Object} details
   */
  onErrorOccurred_(details) {
    if (!this.shouldProcessEvent(details))
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
    if (!this.shouldProcessEvent(details))
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
        this.tryLoadOnline.bind(this), ONLINE_EULA_RETRY_BACKOFF_TIMEOUT_IN_MS);
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

EulaLoader.instances = {};

/**
 * @fileoverview Polymer element for displaying material design Terms Of Service
 * screen.
 */

Polymer({
  is: 'oobe-eula-md',

  behaviors: [OobeI18nBehavior, OobeDialogHostBehavior, LoginScreenBehavior],

  EXTERNAL_API: [
    'setUsageStats',
    'showAdditionalTosDialog',
    'showSecuritySettingsDialog',
  ],

  properties: {
    /**
     * Shows "Loading..." section.
     */
    eulaLoadingScreenShown: {
      type: Boolean,
      value: true,
    },

    /**
     * "Accepot and continue" button is disabled until content is loaded.
     */
    acceptButtonDisabled: {
      type: Boolean,
      value: true,
    },

    /**
     * If "Report anonymous usage stats" checkbox is checked.
     */
    usageStatsChecked: {
      type: Boolean,
      value: false,
    },
  },

  /**
   * Flag that ensures that eula screen set up once.
   * @private {boolean}
   */
  initialized_: false,

  focus() {
    if (this.eulaLoadingScreenShown) {
      this.$.eulaLoadingDialog.focus();
    } else {
      this.$.crosEulaFrame.focus();
    }
  },

  /** Called just before the dialog is shown */
  onBeforeShow() {
    window.setTimeout(this.initializeScreen_.bind(this), 0);
    this.updateLocalizedContent();
  },

  ready() {
    this.initializeLoginScreen('EulaScreen', {
      resetAllowed: true,
    });
  },

  /**
   * Set up dialog before shown it for the first time.
   * @private
   */
  initializeScreen_() {
    if (this.initialized_)
      return;
    this.$.eulaDialog.scrollToBottom();
    this.applyOobeConfiguration_();
    this.initialized_ = true;
  },

  /**
   * Called when dialog is shown for the first time.
   * @private
   */
  applyOobeConfiguration_() {
    var configuration = Oobe.getInstance().getOobeConfiguration();
    if (!configuration)
      return;
    if (configuration.eulaSendStatistics) {
      this.usageStatsChecked = true;
    }
    if (configuration.eulaAutoAccept) {
      this.eulaAccepted_();
    }
  },

  /**
   * Event handler that is invoked when EULA is loaded. Either online version or
   * 'chrome://terms' fallback.
   */
  onFrameLoad_() {
    this.acceptButtonDisabled = false;
    this.eulaLoadingScreenShown = false;
    this.focus();
    this.$.eulaDialog.scrollToBottom();
  },

  /**
   * Load Eula into the given webview. Online version is attempted first with
   * a timeout. If it fails to load, fallback to chrome://terms. The loaded
   * terms contents is then set to the webview via data url. Webview is
   * used as a sandbox for both online and local contents. Data url is
   * used for chrome://terms so that webview never needs to have the
   * privileged webui bindings.
   *
   * @param {!Object} webview Webview element to host the terms.
   * @param {!string} onlineEulaUrl
   * @param {boolean} clear_anchors if true the script will clear anchors
   *                                from the loaded page.
   */
  loadEulaToWebview_(webview, onlineEulaUrl, clear_anchors) {
    assert(webview.tagName === 'WEBVIEW');

    var loadBundledEula = function() {
      WebViewHelper.loadUrlContentToWebView(
          webview, TERMS_URL, WebViewHelper.ContentType.HTML);
    };

    // Load online Eula with a timeout to fallback to the offline version.
    // This won't construct multiple EulaLoaders. Single instance.
    var eulaLoader = new EulaLoader(
        webview, ONLINE_EULA_LOAD_TIMEOUT_IN_MS, loadBundledEula,
        clear_anchors);
    eulaLoader.setUrl(onlineEulaUrl);
  },

  /**
   * This is called when strings are updated.
   */
  updateLocalizedContent() {
    // This forces frame to reload.
    const onlineEulaUrl = loadTimeData.getString('eulaOnlineUrl');

    this.eulaLoadingScreenShown = true;
    this.loadEulaToWebview_(
        this.$.crosEulaFrame, onlineEulaUrl, false /* clear_anchors */);

    const additionalToSUrl =
        loadTimeData.getString('eulaAdditionalToSOnlineUrl');
    this.loadEulaToWebview_(
        this.$.additionalChromeToSFrame, additionalToSUrl,
        true /* clear_anchors */);
    this.i18nUpdateLocale();
  },

  /**
   * This is 'on-tap' event handler for 'Accept' button.
   *
   * @private
   */
  eulaAccepted_() {
    this.userActed('accept-button');
  },

  /**
   * On-change event handler for usageStats.
   *
   * @private
   */
  onUsageChanged_() {
    if (this.usageStatsChecked) {
      this.userActed('select-stats-usage');
    } else {
      this.userActed('unselect-stats-usage');
    }
  },

  /**
   * @private
   */
  onAdditionalTermsClicked_() {
    this.userActed('show-additional-tos');
  },

  /**
   * Shows additional terms of service dialog.
   */
  showAdditionalTosDialog() {
    this.$.additionalToS.showModal();
    this.$.additionalToS.focus();
  },

  /**
   * On-click event handler for close button of the additional ToS dialog.
   *
   * @private
   */
  hideToSDialog_() {
    this.$.additionalToS.close();
  },

  /**
   * @private
   */
  focusAdditionalTermsLink_() {
    this.$.additionalTerms.focus();
  },

  /**
   * On-tap event handler for securitySettings.
   *
   * @private
   */
  onSecuritySettingsClicked_() {
    this.userActed('show-security-settings');
  },

  /**
   * Shows system security settings dialog.
   */
  showSecuritySettingsDialog() {
    this.$.eulaDialog.hidden = true;
    this.$.securitySettingsDialog.hidden = false;
    this.$.securitySettingsDialog.show();
  },

  /**
   * On-tap event handler for the close button on security settings page.
   *
   * @private
   */
  onSecuritySettingsCloseClicked_() {
    this.$.securitySettingsDialog.hidden = true;
    this.$.eulaDialog.hidden = false;
    this.$.eulaDialog.show();
    this.$.securitySettings.focus();
  },

  /**
   * On-tap event handler for stats-help-link.
   *
   * @private
   */
  onUsageStatsHelpLinkClicked_(e) {
    this.userActed('show-stats-usage-learn-more');
    this.$.learnMore.focus();
    e.stopPropagation();
  },

  /**
   * On-tap event handler for back button.
   *
   * @private
   */
  onEulaBackButtonPressed_() {
    this.userActed('back-button');
  },

  /**
   * Sets usage statistics checkbox.
   * @param {boolean} checked Is the checkbox checked?
   */
  setUsageStats(checked) {
    this.usageStatsChecked = checked;
  },

  /**
   * Called when focus is returned.
   */
  onFocusReturned() {
    this.focus();
  },
});
})();
