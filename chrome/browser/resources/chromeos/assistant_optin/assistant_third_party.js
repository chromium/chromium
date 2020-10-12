// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Polymer element for displaying material design assistant
 * third party screen.
 *
 * Event 'loading' will be fired when the page is loading/reloading.
 * Event 'loaded' will be fired when the page has been successfully loaded.
 */

/**
 * Name of the screen.
 * @type {string}
 */
const THIRD_PARTY_SCREEN_ID = 'ThirdPartyScreen';

Polymer({
  is: 'assistant-third-party',

  behaviors: [OobeI18nBehavior, OobeDialogHostBehavior],

  properties: {
    /**
     * Buttons are disabled when the page content is loading.
     */
    buttonsDisabled: {
      type: Boolean,
      value: true,
    },
  },

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

  /** @private {?assistant.BrowserProxy} */
  browserProxy_: null,

  /** @private {Object} */
  webview_: null,

  /**
   * On-tap event handler for next button.
   *
   * @private
   */
  onNextTap_() {
    if (this.buttonsDisabled) {
      return;
    }
    this.buttonsDisabled = true;
    this.browserProxy_.userActed(THIRD_PARTY_SCREEN_ID, ['next-pressed']);
  },

  /** @override */
  created() {
    this.browserProxy_ = assistant.BrowserProxyImpl.getInstance();
  },

  /**
   * Reset the webview and add load complete handler.
   */
  resetWebview() {
    this.webview_ = document.createElement('webview');
    this.webview_.id = 'overlay-webview';
    this.webview_.classList.add('flex');
    var webviewContainer = this.$['webview-container'];
    this.webview_.onloadstop = function() {
      webviewContainer.classList.remove('overlay-loading');
    };
    this.$$('#overlay-webview').replaceWith(this.webview_);
  },

  /**
   * Click event handler for information links.
   * @param {MouseEvent} e click event.
   */
  urlClickHandler(e) {
    if (e.target.localName !== 'a') {
      return;
    }
    e.preventDefault();
    this.lastFocusedElement = e.target;
    this.showThirdPartyOverlay(e.target.href, e.target.innerText);
  },

  /**
   * Shows third party information links in overlay dialog.
   * @param {string} url URL to show.
   * @param {string} title Title of the dialog.
   */
  showThirdPartyOverlay(url, title) {
    this.$['webview-container'].classList.add('overlay-loading');
    this.webview_.src = url;
    this.$['third-party-overlay'].setTitleAriaLabel(title);
    this.$['third-party-overlay'].showModal();
    this.$['overlay-close-button'].focus();
  },

  /**
   * Hides overlay dialog.
   */
  hideOverlay() {
    this.resetWebview();
    this.$['third-party-overlay'].close();
    if (this.lastFocusedElement) {
      this.lastFocusedElement.focus();
      this.lastFocusedElement = null;
    }
  },

  /**
   * Reloads the page.
   */
  reloadPage() {
    this.fire('loading');
    this.buttonsDisabled = true;
  },

  /**
   * Reload the page with the given consent string text data.
   */
  reloadContent(data) {
    this.$['third-party-dialog'].setAttribute(
        'aria-label', data['thirdPartyTitle']);
    this.$['title-text'].textContent = data['thirdPartyTitle'];
    this.$['next-button'].labelForAria = data['thirdPartyContinueButton'];
    this.$['next-button-text'].textContent = data['thirdPartyContinueButton'];
    this.$['footer-text'].innerHTML =
        this.sanitizer_.sanitizeHtml(data['thirdPartyFooter']);

    this.$['footer-text'].onclick = this.urlClickHandler.bind(this);

    this.consentStringLoaded_ = true;
    if (this.settingZippyLoaded_) {
      this.onPageLoaded();
    }
  },

  /**
   * Add a setting zippy with the provided data.
   */
  addSettingZippy(zippy_data) {
    if (this.settingZippyLoaded_) {
      if (this.consentStringLoaded_) {
        this.onPageLoaded();
      }
      return;
    }

    for (var i in zippy_data) {
      var data = zippy_data[i];
      var zippy = document.createElement('setting-zippy');
      zippy.setAttribute(
          'icon-src',
          'data:text/html;charset=utf-8,' +
              encodeURIComponent(
                  zippy.getWrappedIcon(data['iconUri'], data['title'])));
      zippy.setAttribute('expand-style', true);

      var title = document.createElement('div');
      title.slot = 'title';
      title.innerHTML = this.sanitizer_.sanitizeHtml(data['title']);
      zippy.appendChild(title);

      var description = document.createElement('div');
      description.slot = 'content';
      description.innerHTML = this.sanitizer_.sanitizeHtml(data['description']);
      zippy.appendChild(description);

      var additional = document.createElement('div');
      additional.slot = 'additional';
      additional.innerHTML =
          this.sanitizer_.sanitizeHtml(data['additionalInfo']);
      zippy.appendChild(additional);

      additional.onclick = this.urlClickHandler.bind(this);

      this.$['insertion-point'].appendChild(zippy);
    }

    this.settingZippyLoaded_ = true;
    if (this.consentStringLoaded_) {
      this.onPageLoaded();
    }
  },

  /**
   * Handles event when all the page content has been loaded.
   */
  onPageLoaded() {
    this.fire('loaded');
    this.buttonsDisabled = false;
    this.$['next-button'].focus();
    if (!this.hidden && !this.screenShown_) {
      this.browserProxy_.screenShown(THIRD_PARTY_SCREEN_ID);
      this.screenShown_ = true;
    }
  },

  /**
   * Signal from host to show the screen.
   */
  onShow() {
    this.$['overlay-close-button'].addEventListener(
        'click', this.hideOverlay.bind(this));
    this.resetWebview();

    if (!this.settingZippyLoaded_ || !this.consentStringLoaded_) {
      this.reloadPage();
    } else {
      Polymer.RenderStatus.afterNextRender(
          this, () => this.$['next-button'].focus());
      this.browserProxy_.screenShown(THIRD_PARTY_SCREEN_ID);
      this.screenShown_ = true;
    }
  },
});
