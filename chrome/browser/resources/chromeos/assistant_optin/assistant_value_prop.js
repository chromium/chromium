// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Polymer element for displaying material design assistant
 * value prop screen.
 *
 * Event 'loading' will be fired when the page is loading/reloading.
 * Event 'error' will be fired when the webview failed to load.
 * Event 'loaded' will be fired when the page has been successfully loaded.
 */

Polymer({
  is: 'assistant-value-prop',

  behaviors: [OobeDialogHostBehavior],

  properties: {
    /**
     * Buttons are disabled when the webview content is loading.
     */
    buttonsDisabled: {
      type: Boolean,
      value: true,
    },

    /**
     * System locale.
     */
    locale: {
      type: String,
    },

    /**
     * Default url for locale en_us.
     */
    defaultUrl: {
      type: String,
      value:
          'https://www.gstatic.com/opa-android/oobe/a02187e41eed9e42/v2_omni_en_us.html',
    },
  },

  /**
   * Whether try to reload with the default url when a 404 error occurred.
   * @type {boolean}
   * @private
   */
  reloadWithDefaultUrl_: false,

  /**
   * Whether an error occurs while the webview is loading.
   * @type {boolean}
   * @private
   */
  loadingError_: false,

  /**
   * The value prop webview object.
   * @type {Object}
   * @private
   */
  valuePropView_: null,

  /**
   * Whether the screen has been initialized.
   * @type {boolean}
   * @private
   */
  initialized_: false,

  /**
   * Whether the response header has been received for the value prop view.
   * @type {boolean}
   * @private
   */
  headerReceived_: false,

  /**
   * Whether the webview has been successfully loaded.
   * @type {boolean}
   * @private
   */
  webViewLoaded_: false,

  /**
   * Whether all the setting zippy has been successfully loaded.
   * @type {boolean}
   * @private
   */
  settingZippyLoaded_: false,

  /**
   * Whether all the consent text strings has been successfully loaded.
   * @type {boolean}
   * @private
   */
  consentStringLoaded_: false,

  /**
   * Whether the screen has been shown to the user.
   * @type {boolean}
   * @private
   */
  screenShown_: false,

  /**
   * Sanitizer used to sanitize html snippets.
   * @type {HtmlSanitizer}
   * @private
   */
  sanitizer_: new HtmlSanitizer(),

  /**
   * On-tap event handler for skip button.
   *
   * @private
   */
  onSkipTap_: function() {
    if (this.buttonsDisabled) {
      return;
    }
    this.buttonsDisabled = true;
    chrome.send(
        'login.AssistantOptInFlowScreen.ValuePropScreen.userActed',
        ['skip-pressed']);
  },

  /**
   * On-tap event handler for next button.
   *
   * @private
   */
  onNextTap_: function() {
    if (this.buttonsDisabled) {
      return;
    }
    this.buttonsDisabled = true;
    chrome.send(
        'login.AssistantOptInFlowScreen.ValuePropScreen.userActed',
        ['next-pressed']);
  },

  /**
   * Sets learn more content text and shows it as overlay dialog.
   * @param {string} content HTML formatted text to show.
   */
  showLearnMoreOverlay: function(title, additionalInfo) {
    this.$['overlay-title-text'].innerHTML =
        this.sanitizer_.sanitizeHtml(title);
    this.$['overlay-additional-info-text'].innerHTML =
        this.sanitizer_.sanitizeHtml(additionalInfo);

    var overlay = this.$['learn-more-overlay'];
    overlay.hidden = false;
  },

  /**
   * Hides overlay dialog.
   */
  hideOverlay: function() {
    this.$['learn-more-overlay'].hidden = true;
  },

  /**
   * Reloads value prop webview.
   */
  reloadPage: function() {
    this.fire('loading');

    if (this.initialized_) {
      chrome.send(
          'login.AssistantOptInFlowScreen.ValuePropScreen.userActed',
          ['reload-requested']);
      this.settingZippyLoaded_ = false;
      this.consentStringLoaded_ = false;
    }

    this.loadingError_ = false;
    this.headerReceived_ = false;
    this.valuePropView_.src =
        'https://www.gstatic.com/opa-android/oobe/a02187e41eed9e42/v2_omni_' +
        this.locale + '.html';

    this.buttonsDisabled = true;
  },

  /**
   * Handles event when value prop webview cannot be loaded.
   */
  onWebViewErrorOccurred: function(details) {
    this.fire('error');
    this.loadingError_ = true;
  },

  /**
   * Handles event when value prop webview is loaded.
   */
  onWebViewContentLoad: function(details) {
    if (details == null) {
      return;
    }
    if (this.loadingError_ || !this.headerReceived_) {
      return;
    }
    if (this.reloadWithDefaultUrl_) {
      this.valuePropView_.src = this.defaultUrl;
      this.headerReceived_ = false;
      this.reloadWithDefaultUrl_ = false;
      return;
    }

    this.webViewLoaded_ = true;
    if (this.settingZippyLoaded_ && this.consentStringLoaded_) {
      this.onPageLoaded();
    }
  },

  /**
   * Handles event when webview request headers received.
   */
  onWebViewHeadersReceived: function(details) {
    if (details == null) {
      return;
    }
    this.headerReceived_ = true;
    if (details.statusCode == '404') {
      if (details.url != this.defaultUrl) {
        this.reloadWithDefaultUrl_ = true;
        return;
      } else {
        this.onWebViewErrorOccurred();
      }
    } else if (details.statusCode != '200') {
      this.onWebViewErrorOccurred();
    }
  },

  /**
   * Reload the page with the given consent string text data.
   */
  reloadContent: function(data) {
    this.$['user-image'].src = data['valuePropUserImage'];
    this.$['title-text'].textContent = data['valuePropTitle'];
    this.$['intro-text'].textContent = data['valuePropIntro'];
    this.$['user-name'].textContent = data['valuePropIdentity'];
    this.$['next-button-text'].textContent = data['valuePropNextButton'];
    this.$['skip-button-text'].textContent = data['valuePropSkipButton'];
    this.$['footer-text'].innerHTML =
        this.sanitizer_.sanitizeHtml(data['valuePropFooter']);

    this.consentStringLoaded_ = true;
    if (this.webViewLoaded_ && this.settingZippyLoaded_) {
      this.onPageLoaded();
    }
  },

  /**
   * Add a setting zippy with the provided data.
   */
  addSettingZippy: function(zippy_data) {
    for (var i in zippy_data) {
      var data = zippy_data[i];
      var zippy = document.createElement('setting-zippy');
      zippy.setAttribute(
          'icon-src',
          'data:text/html;charset=utf-8,' +
              encodeURIComponent(zippy.getWrappedIcon(data['iconUri'])));
      zippy.setAttribute('hide-line', true);
      zippy.setAttribute('popup-style', true);

      var title = document.createElement('div');
      title.className = 'zippy-title';
      title.innerHTML = this.sanitizer_.sanitizeHtml(data['title']);
      zippy.appendChild(title);

      var description = document.createElement('div');
      description.className = 'zippy-description';
      description.innerHTML = this.sanitizer_.sanitizeHtml(data['description']);
      description.innerHTML += '&ensp;';

      var learnMoreLink = document.createElement('a');
      learnMoreLink.className = 'learn-more-link';
      learnMoreLink.textContent = data['popupLink'];
      learnMoreLink.setAttribute('href', 'javascript:void(0)');
      learnMoreLink.onclick = function(title, additionalInfo) {
        this.showLearnMoreOverlay(title, additionalInfo);
      }.bind(this, data['title'], data['additionalInfo']);

      description.appendChild(learnMoreLink);
      zippy.appendChild(description);

      this.$['consents-container'].appendChild(zippy);
    }

    this.settingZippyLoaded_ = true;
    if (this.webViewLoaded_ && this.consentStringLoaded_) {
      this.onPageLoaded();
    }
  },

  /**
   * Handles event when all the page content has been loaded.
   */
  onPageLoaded: function() {
    this.fire('loaded');

    this.buttonsDisabled = false;
    this.$['next-button'].focus();

    if (!this.hidden && !this.screenShown_) {
      chrome.send('login.AssistantOptInFlowScreen.ValuePropScreen.screenShown');
      this.screenShown_ = true;
    }
  },

  /**
   * Signal from host to show the screen.
   */
  onShow: function() {
    var requestFilter = {urls: ['<all_urls>'], types: ['main_frame']};

    this.$['overlay-close-button'].addEventListener(
        'click', this.hideOverlay.bind(this));
    this.valuePropView_ = this.$['value-prop-view'];
    this.locale =
        loadTimeData.getString('locale').replace('-', '_').toLowerCase();

    if (!this.initialized_) {
      this.valuePropView_.request.onErrorOccurred.addListener(
          this.onWebViewErrorOccurred.bind(this), requestFilter);
      this.valuePropView_.request.onHeadersReceived.addListener(
          this.onWebViewHeadersReceived.bind(this), requestFilter);
      this.valuePropView_.addEventListener(
          'contentload', this.onWebViewContentLoad.bind(this));

      this.valuePropView_.addContentScripts([{
        name: 'stripLinks',
        matches: ['<all_urls>'],
        js: {
          code: 'document.querySelectorAll(\'a\').forEach(' +
              'function(anchor){anchor.href=\'javascript:void(0)\';})'
        },
        run_at: 'document_end'
      }]);

      this.reloadPage();
      this.initialized_ = true;
    }
  },
});
