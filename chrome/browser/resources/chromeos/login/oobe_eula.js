// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Polymer element for displaying material design Terms Of Service
 * screen.
 */

Polymer({
  is: 'oobe-eula-md',

  behaviors: [I18nBehavior, OobeDialogHostBehavior],

  properties: {
    /**
     * Shows "Loading..." section.
     */
    eulaLoadingScreenShown: {
      type: Boolean,
      value: false,
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

    /**
     * The TPM password shown on the installation settings page.
     */
    password: {
      type: String,
      value: null,
    },

    /**
     * Reference to OOBE screen object.
     * @type {!{
     *     loadEulaToWebview_: function(Element),
     *     onUsageStatsClicked_: function(boolean),
     * }}
     */
    screen: {
      type: Object,
    },
  },

  /**
   * Flag that ensures that eula screen set up once.
   * @private {boolean}
   */
  initialized_: false,

  focus: function() {
    if (this.eulaLoadingScreenShown) {
      this.$.eulaLoadingDialog.show();
    } else {
      this.$.eulaDialog.show();
    }
  },

  /** Called when dialog is shown */
  onBeforeShow: function() {
    this.behaviors.forEach((behavior) => {
      if (behavior.onBeforeShow)
        behavior.onBeforeShow.call(this);
    });
    window.setTimeout(this.initializeScreen_.bind(this), 0);
  },

  /**
   * Set up dialog before shown it for the first time.
   * @private
   */
  initializeScreen_: function() {
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
  applyOobeConfiguration_: function() {
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
   * Event handler that is invoked when 'chrome://terms' is loaded.
   */
  onFrameLoad_: function() {
    this.acceptButtonDisabled = false;
  },

  /**
   * This is called when strings are updated.
   */
  updateLocalizedContent: function(event) {
    // This forces frame to reload.
    this.screen.loadEulaToWebview_(this.$.crosEulaFrame);
    this.i18nUpdateLocale();
  },

  /**
   * This is 'on-tap' event handler for 'Accept' button.
   *
   * @private
   */
  eulaAccepted_: function() {
    chrome.send('login.EulaScreen.userActed', ['accept-button']);
  },

  /**
   * On-change event handler for usageStats.
   *
   * @private
   */
  onUsageChanged_: function() {
    this.screen.onUsageStatsClicked_(this.$.usageStats.checked);
  },

  /**
   * On-tap event handler for installationSettings.
   *
   * @private
   */
  onInstallationSettingsClicked_: function() {
    chrome.send('eulaOnInstallationSettingsPopupOpened');
    this.$.eulaDialog.hidden = true;
    this.$.installationSettingsDialog.hidden = false;
    this.$['settings-close-button'].focus();
  },

  /**
   * On-tap event handler for the close button on installation settings page.
   *
   * @private
   */
  onInstallationSettingsCloseClicked_: function() {
    this.$.installationSettingsDialog.hidden = true;
    this.$.eulaDialog.hidden = false;
  },

  /**
   * On-tap event handler for stats-help-link.
   *
   * @private
   */
  onUsageStatsHelpLinkClicked_: function(e) {
    chrome.send('eulaOnLearnMore');
    e.stopPropagation();
  },

  /**
   * On-tap event handler for back button.
   *
   * @private
   */
  onEulaBackButtonPressed_: function() {
    chrome.send('login.EulaScreen.userActed', ['back-button']);
  },

  /**
   * Returns true if the TPM password hasn't been received.
   *
   * @private
   */
  isWaitingForPassword_: function(password) {
    return password == null;
  },

  /**
   * Returns true if the TPM password has been received but it's empty.
   *
   * @private
   */
  isPasswordEmpty_: function(password) {
    return password != null && password.length == 0;
  },

  /**
   * Switches usage stats toggle state.
   *
   * @private
   */
  usageStatsLabelClicked_: function() {
    this.usageStatsChecked = !this.usageStatsChecked;
  },
});
