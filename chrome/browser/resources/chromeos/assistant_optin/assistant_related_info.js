// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Polymer element for displaying material design assistant
 * related info screen.
 *
 * Event 'loading' will be fired when the page is loading/reloading.
 * Event 'loaded' will be fired when the page has been successfully loaded.
 */

/**
 * Name of the screen.
 * @type {string}
 */
const RELATED_INFO_SCREEN_ID = 'RelatedInfoScreen';

Polymer({
  is: 'assistant-related-info',

  behaviors: [OobeI18nBehavior, OobeDialogHostBehavior],

  properties: {
    /**
     * Whether page content is loading.
     */
    loading: {
      type: Boolean,
      value: true,
    },

    /**
     * Title key of the screen.
     */
    titleKey_: {
      type: String,
      value: 'assistantRelatedInfoTitle',
    },

    /**
     * Whether activity control consent is skipped.
     */
    skipActivityControl_: {
      type: Boolean,
      value: false,
    },
  },

  setUrlTemplateForTesting(url) {
    this.urlTemplate_ = url;
  },

  /**
   * The animation URL template - loaded from loadTimeData.
   * The template is expected to have '$' instead of the locale.
   * @private {string}
   */
  urlTemplate_:
      'https://www.gstatic.com/opa-android/oobe/a02187e41eed9e42/v3_omni_$.html',

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
   * The animation webview object.
   * @type {Object}
   * @private
   */
  webview_: null,

  /**
   * Whether the screen has been initialized.
   * @type {boolean}
   * @private
   */
  initialized_: false,

  /**
   * Whether the response header has been received for the animation webview.
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

  /** @private {?assistant.BrowserProxy} */
  browserProxy_: null,

  /**
   * On-tap event handler for next button.
   *
   * @private
   */
  onNextTap_() {
    if (this.loading) {
      return;
    }
    this.loading = true;
    this.browserProxy_.userActed(RELATED_INFO_SCREEN_ID, ['next-pressed']);
  },

  /**
   * On-tap event handler for skip button.
   *
   * @private
   */
  onSkipTap_() {
    if (this.loading) {
      return;
    }
    this.loading = true;
    this.browserProxy_.userActed(RELATED_INFO_SCREEN_ID, ['skip-pressed']);
  },

  /** @override */
  created() {
    this.browserProxy_ = assistant.BrowserProxyImpl.getInstance();
  },

  /**
   * Reloads the page.
   */
  reloadPage() {
    this.fire('loading');
    this.loading = true;
    this.loadingError_ = false;
    this.headerReceived_ = false;
    let locale = this.locale.replace('-', '_').toLowerCase();
    this.webview_.src = this.urlTemplate_.replace('$', locale);
  },

  /**
   * Handles event when animation webview cannot be loaded.
   */
  onWebViewErrorOccurred(details) {
    this.fire('error');
    this.loadingError_ = true;
  },

  /**
   * Handles event when animation webview is loaded.
   */
  onWebViewContentLoad(details) {
    if (details == null) {
      return;
    }
    if (this.loadingError_ || !this.headerReceived_) {
      return;
    }
    if (this.reloadWithDefaultUrl_) {
      this.webview_.src = this.getDefaultAnimationUrl_();
      this.headerReceived_ = false;
      this.reloadWithDefaultUrl_ = false;
      return;
    }

    this.webViewLoaded_ = true;
    if (this.consentStringLoaded_) {
      this.onPageLoaded();
    }
  },

  /**
   * Handles event when webview request headers received.
   */
  onWebViewHeadersReceived(details) {
    if (details == null) {
      return;
    }
    this.headerReceived_ = true;
    if (details.statusCode == '404') {
      if (details.url != this.getDefaultAnimationUrl_()) {
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
  reloadContent(data) {
    if (data['activityControlNeeded']) {
      this.titleKey_ = 'assistantRelatedInfoTitle';
    } else {
      this.titleKey_ = 'assistantRelatedInfoReturnedUserTitle';
    }
    this.skipActivityControl_ = !data['activityControlNeeded'];
    this.$.zippy.setAttribute(
        'icon-src',
        'data:text/html;charset=utf-8,' +
            encodeURIComponent(this.$.zippy.getWrappedIcon(
                'https://www.gstatic.com/images/icons/material/system/2x/' +
                    'info_outline_grey600_24dp.png',
                this.i18n('assistantScreenContextTitle'))));
    this.consentStringLoaded_ = true;
    if (this.webViewLoaded_) {
      this.onPageLoaded();
    }
  },

  /**
   * Handles event when all the page content has been loaded.
   */
  onPageLoaded() {
    this.fire('loaded');
    this.loading = false;
    this.$['next-button'].focus();
    if (!this.hidden && !this.screenShown_) {
      this.browserProxy_.screenShown(RELATED_INFO_SCREEN_ID);
      this.screenShown_ = true;
    }
  },

  /**
   * Signal from host to show the screen.
   */
  onShow() {
    if (!this.initialized_) {
      this.webview_ = this.$['assistant-animation-webview'];
      this.initializeWebview_(this.webview_);
      this.reloadPage();
      this.initialized_ = true;
    } else {
      Polymer.RenderStatus.afterNextRender(
          this, () => this.$['next-button'].focus());
      this.browserProxy_.screenShown(RELATED_INFO_SCREEN_ID);
      this.screenShown_ = true;
    }
  },

  initializeWebview_(webview) {
    const requestFilter = {urls: ['<all_urls>'], types: ['main_frame']};
    webview.request.onErrorOccurred.addListener(
        this.onWebViewErrorOccurred.bind(this), requestFilter);
    webview.request.onHeadersReceived.addListener(
        this.onWebViewHeadersReceived.bind(this), requestFilter);
    webview.addEventListener(
        'contentload', this.onWebViewContentLoad.bind(this));
    webview.addContentScripts([webviewStripLinksContentScript]);
  },

  /**
   * Get default animation url for locale en.
   */
  getDefaultAnimationUrl_() {
    return this.urlTemplate_.replace('$', 'en_us');
  },

  /**
   * Returns the text for subtitle.
   */
  getSubtitleMessage_(locale) {
    return this.i18nAdvanced('assistantRelatedInfoMessage');
  },

  /**
   * Returns the webview animation container.
   */
  getAnimationContainer() {
    return this.$['animation-container'];
  },
});
