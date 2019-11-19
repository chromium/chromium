// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Polymer element for displaying material design assistant
 * get more screen. It contains screen context and email opt-in.
 *
 * Event 'loading' will be fired when the page is loading/reloading.
 * Event 'loaded' will be fired when the page has been successfully loaded.
 */

Polymer({
  is: 'assistant-get-more',

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
   * On-tap event handler for next button.
   *
   * @private
   */
  onNextTap_: function() {
    if (this.buttonsDisabled) {
      return;
    }
    this.buttonsDisabled = true;

    var screenContext = this.$$('#toggle-context').hasAttribute('checked');
    var toggleEmail = this.$$('#toggle-email');
    var emailOptedIn =
        toggleEmail != null && toggleEmail.hasAttribute('checked');

    // TODO(updowndota): Wrap chrome.send() calls with a proxy object.
    chrome.send(
        'login.AssistantOptInFlowScreen.GetMoreScreen.userActed',
        [screenContext, emailOptedIn]);
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
    this.consentStringLoaded_ = true;
    if (this.settingZippyLoaded_) {
      this.onPageLoaded();
    }
  },

  /**
   * Add a setting zippy with the provided data.
   */
  addSettingZippy: function(zippy_data) {
    assert(zippy_data.length <= 3);

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
      zippy.setAttribute('hide-line', true);
      zippy.setAttribute('toggle-style', true);
      zippy.id = 'zippy-' + data['id'];
      var title = document.createElement('div');
      title.className = 'zippy-title';
      title.textContent = data['title'];
      zippy.appendChild(title);

      var toggle = document.createElement('cr-toggle');
      toggle.className = 'zippy-toggle';
      toggle.id = 'toggle-' + data['id'];
      if (data['defaultEnabled']) {
        toggle.setAttribute('checked', '');
      }
      if (data['toggleDisabled']) {
        toggle.disabled = true;
      }
      zippy.appendChild(toggle);

      var description = document.createElement('div');
      description.className = 'zippy-description';
      description.textContent = data['description'];
      if (data['legalText']) {
        var legalText = document.createElement('p');
        legalText.textContent = data['legalText'];
        description.appendChild(legalText);
      }
      zippy.appendChild(description);

      Polymer.dom(this.$['toggles-container']).appendChild(zippy);
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
      chrome.send('login.AssistantOptInFlowScreen.GetMoreScreen.screenShown');
      this.screenShown_ = true;
    }
  },

  /**
   * Signal from host to show the screen.
   */
  onShow: function() {
    if (!this.settingZippyLoaded_ || !this.consentStringLoaded_) {
      this.reloadPage();
    } else {
      this.$['next-button'].focus();
      chrome.send('login.AssistantOptInFlowScreen.GetMoreScreen.screenShown');
      this.screenShown_ = true;
    }
  },
});
