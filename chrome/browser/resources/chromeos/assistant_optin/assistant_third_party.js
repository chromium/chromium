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

Polymer({
  is: 'assistant-third-party',

  behaviors: [OobeDialogHostBehavior],

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
        'login.AssistantOptInFlowScreen.ThirdPartyScreen.userActed',
        ['next-pressed']);
  },

  /**
   * Click event handler for information links.
   * @param {MouseEvent} e click event.
   */
  urlClickHandler: function(e) {
    if (!e.target.localName == 'a') {
      return;
    }
    e.preventDefault();
    this.showThirdPartyOverlay(e.target.href);
  },

  /**
   * Shows third party information links in overlay dialog.
   * @param {string} url URL to show.
   */
  showThirdPartyOverlay: function(url) {
    this.$['webview-container'].classList.add('overlay-loading');
    this.$['overlay-webview'].src = url;

    var overlay = this.$['third-party-overlay'];
    overlay.hidden = false;
  },

  /**
   * Hides overlay dialog.
   */
  hideOverlay: function() {
    this.$['third-party-overlay'].hidden = true;
  },

  /**
   * Reloads the page.
   */
  reloadPage: function() {
    this.fire('loading');
    this.buttonsDisabled = true;
  },

  /**
   * Reload the page with the given consent string text data.
   */
  reloadContent: function(data) {
    this.$['title-text'].textContent = data['thirdPartyTitle'];
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
  addSettingZippy: function(zippy_data) {
    for (var i in zippy_data) {
      var data = zippy_data[i];
      var zippy = document.createElement('setting-zippy');
      zippy.setAttribute(
          'icon-src',
          'data:text/html;charset=utf-8,' +
              encodeURIComponent(zippy.getWrappedIcon(data['iconUri'])));
      zippy.setAttribute('expand-style', true);

      var title = document.createElement('div');
      title.className = 'zippy-title';
      title.innerHTML = this.sanitizer_.sanitizeHtml(data['title']);
      zippy.appendChild(title);

      var description = document.createElement('div');
      description.className = 'zippy-description';
      description.innerHTML = this.sanitizer_.sanitizeHtml(data['description']);
      zippy.appendChild(description);

      var additional = document.createElement('div');
      additional.className = 'zippy-additional';
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
  onPageLoaded: function() {
    this.fire('loaded');
    this.buttonsDisabled = false;
    this.$['next-button'].focus();
    if (!this.hidden && !this.screenShown_) {
      chrome.send(
          'login.AssistantOptInFlowScreen.ThirdPartyScreen.screenShown');
      this.screenShown_ = true;
    }
  },

  /**
   * Signal from host to show the screen.
   */
  onShow: function() {
    this.$['overlay-close-button'].addEventListener(
        'click', this.hideOverlay.bind(this));
    var webviewContainer = this.$['webview-container'];
    this.$['overlay-webview'].addEventListener('contentload', function() {
      webviewContainer.classList.remove('overlay-loading');
    });

    if (!this.settingZippyLoaded_ || !this.consentStringLoaded_) {
      this.reloadPage();
    } else {
      this.$['next-button'].focus();
      chrome.send(
          'login.AssistantOptInFlowScreen.ThirdPartyScreen.screenShown');
      this.screenShown_ = true;
    }
  },
});
