// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Polymer element for displaying material design assistant
 * loading screen.
 *
 * Event 'reload' will be fired when the user click the retry button.
 */

Polymer({
  is: 'assistant-loading',

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
   * Whether an error occurs while the page is loading.
   * @type {boolean}
   * @private
   */
  loadingError_: false,

  /**
   * Timeout ID for loading animation.
   * @type {number}
   * @private
   */
  animationTimeout_: null,

  /**
   * Timeout ID for loading (will fire an error).
   * @type {number}
   * @private
   */
  loadingTimeout_: null,

  /**
   * On-tap event handler for retry button.
   *
   * @private
   */
  onRetryTap_: function() {
    this.fire('reload');
  },

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
    chrome.send('login.AssistantOptInFlowScreen.flowFinished');
  },

  /**
   * Add class to the list of classes of root elements.
   * @param {string} className class to add
   *
   * @private
   */
  addClass_: function(className) {
    this.$['loading-dialog'].classList.add(className);
  },

  /**
   * Remove class to the list of classes of root elements.
   * @param {string} className class to remove
   *
   * @private
   */
  removeClass_: function(className) {
    this.$['loading-dialog'].classList.remove(className);
  },

  /**
   * Reloads the page.
   */
  reloadPage: function() {
    window.clearTimeout(this.animationTimeout_);
    window.clearTimeout(this.loadingTimeout_);
    this.removeClass_('loaded');
    this.removeClass_('error');
    this.addClass_('loading');
    this.buttonsDisabled = true;

    this.animationTimeout_ = window.setTimeout(function() {
      this.addClass_('loading-animation');
    }.bind(this), 500);
    this.loadingTimeout_ = window.setTimeout(function() {
      this.onLoadingTimeout();
    }.bind(this), 15000);
  },

  /**
   * Handles event when page content cannot be loaded.
   */
  onErrorOccurred: function(details) {
    this.loadingError_ = true;
    window.clearTimeout(this.animationTimeout_);
    window.clearTimeout(this.loadingTimeout_);
    this.removeClass_('loading-animation');
    this.removeClass_('loading');
    this.removeClass_('loaded');
    this.addClass_('error');

    this.buttonsDisabled = false;
    this.$['retry-button'].focus();
  },

  /**
   * Handles event when all the page content has been loaded.
   */
  onPageLoaded: function() {
    window.clearTimeout(this.animationTimeout_);
    window.clearTimeout(this.loadingTimeout_);
    this.removeClass_('loading-animation');
    this.removeClass_('loading');
    this.removeClass_('error');
    this.addClass_('loaded');
  },

  /**
   * Called when the loading timeout is triggered.
   */
  onLoadingTimeout: function() {
    chrome.send('login.AssistantOptInFlowScreen.LoadingScreen.timeout');
    this.onErrorOccurred();
  },

  /**
   * Signal from host to show the screen.
   */
  onShow: function() {
    this.reloadPage();
  },
});
