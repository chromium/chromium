// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Polymer element for displaying material design for ARC Terms Of
 * Service screen.
 */

Polymer({
  is: 'arc-tos-element',

  behaviors: [OobeI18nBehavior, OobeDialogHostBehavior],

  properties: {
    /**
     * Accept, Skip and Retry buttons are disabled until content is loaded.
     */
    arcTosButtonsDisabled: {
      type: Boolean,
      value: true,
      observer: 'buttonsDisabledStateChanged_',
    },

    /**
     * Reference to OOBE screen object.
     * @type {!{
     *     onAccept: function(),
     *     reloadPlayStoreToS: function(),
     * }}
     */
    screen: {
      type: Object,
    },

    /**
     * Indicates whether metrics text should be hidden.
     */
    isMetricsHidden: {
      type: Boolean,
      value: false,
    },

    /**
     * String id for metrics collection text.
     */
    metricsTextKey: {
      type: String,
      value: 'arcTextMetricsEnabled',
    },

    /**
     * String id of Google service confirmation text.
     */
    googleServiceConfirmationTextKey: {
      type: String,
      value: 'arcTextGoogleServiceConfirmation',
    },

    /**
     * String id of text for Accept button.
     */
    acceptTextKey: {
      type: String,
      value: 'arcTermsOfServiceAcceptButton',
    },

    /**
     * Indicates whether backup and restore should be enabled.
     */
    backupRestore: {
      type: Boolean,
      value: true,
    },

    /**
     * Indicates whether backup and restore is managed.
     * If backup and restore is managed, the checkbox will be disabled.
     */
    backupRestoreManaged: {
      type: Boolean,
      value: false,
    },

    /**
     * Indicates whether current account is child account.
     */
    isChild: {
      type: Boolean,
      value: false,
    },

    /**
     * Indicates whether location service should be enabled.
     */
    locationService: {
      type: Boolean,
      value: true,
    },

    /**
     * Indicates whether location service is managed.
     * If location service is managed, the checkbox will be disabled.
     */
    locationServiceManaged: {
      type: Boolean,
      value: false,
    },

    /**
     * Indicates whether user will review Arc settings after login.
     */
    reviewSettings: {
      type: Boolean,
      value: false,
    },

    /**
     * Indicates whether user sees full content of terms of service.
     */
    showFullDialog: {
      type: Boolean,
      value: false,
    },

    /**
     * Indicates whether currently under demo mode.
     */
    demoMode: {
      type: Boolean,
      value: false,
    },

    /**
     * Indicates whether popup overlay webview is loading.
     */
    overlayLoading_: {
      type: Boolean,
      value: true,
    },
  },

  /**
   * Flag that ensures that OOBE configuration is applied only once.
   * @private {boolean}
   */
  configuration_applied_: false,

  /**
   * Flag indicating if screen was shown.
   * @private {boolean}
   */
  is_shown_: false,

  /**
   * Last focused element when overlay is shown. Used to resume focus when
   * overlay is dismissed.
   * @private {Object|null}
   */
  lastFocusedElement_: null,

  /** Called when dialog is shown */
  onBeforeShow() {
    this.is_shown_ = true;
    window.setTimeout(this.applyOobeConfiguration_.bind(this), 0);
  },

  /** Setups overlay webview loading callback */
  setupOverlay() {
    var self = this;
    this.$.arcTosOverlayWebview.addEventListener(
        'contentload', function() {
      self.overlayLoading_ = false;
    });
  },

  /**
   * Opens external URL in popup overlay.
   * @param {string} targetUrl to show in overlay webview.
   * @param {boolean} isUsingOfflineTerm whether to use offline url.
   */
  showUrlOverlay(targetUrl, isUsingOfflineTerm) {
    if (!isUsingOfflineTerm) {
      this.$.arcTosOverlayWebview.src = targetUrl;
    }
    this.lastFocusedElement_ = this.shadowRoot.activeElement;

    this.overlayLoading_ = true;
    this.$.arcTosOverlayPrivacyPolicy.showDialog();
  },

  /**
   * Returns element by its id.
   */
  getElement(id) {
    return this.$[id];
  },

  /**
   * Called when dialog is shown for the first time.
   *
   * @private
   */
  applyOobeConfiguration_() {
    if (this.configuration_applied_)
      return;
    var configuration = Oobe.getInstance().getOobeConfiguration();
    if (!configuration)
      return;
    if (this.arcTosButtonsDisabled)
      return;
    if (configuration.arcTosAutoAccept) {
      this.onAccept_();
    }
    this.configuration_applied_ = true;
  },

  /**
   * Called whenever buttons state is updated.
   *
   * @private
   */
  buttonsDisabledStateChanged_(newValue, oldValue) {
    // Trigger applyOobeConfiguration_ if buttons are enabled and dialog is
    // visible.
    if (this.arcTosButtonsDisabled)
      return;
    if (!this.is_shown_)
      return;
    if (this.is_configuration_applied_)
      return;
    window.setTimeout(this.applyOobeConfiguration_.bind(this), 0);
  },

  /**
   * On-tap event handler for Accept button.
   *
   * @private
   */
  onAccept_() {
    chrome.send('login.ArcTermsOfServiceScreen.userActed', ['accept']);
    this.screen.onAccept();
  },

  /**
   * On-tap event handler for Next button.
   *
   * @private
   */
  onNext_() {
    chrome.send('login.ArcTermsOfServiceScreen.userActed', ['next']);
    this.showFullDialog = true;
    this.$.arcTosDialog.scrollToBottom();
    this.$.arcTosAcceptButton.focus();
  },

  /**
   * On-tap event handler for Retry button.
   *
   * @private
   */
  onRetry_() {
    chrome.send('login.ArcTermsOfServiceScreen.userActed', ['retry']);
    this.screen.reloadPlayStoreToS();
  },

  /**
   * On-tap event handler for Back button.
   *
   * @private
   */
  onBack_() {
    chrome.send('login.ArcTermsOfServiceScreen.userActed', ['go-back']);
  },

  /**
   * On-tap event handler for metrics learn more link
   * @private
   */
  onMetricsLearnMoreTap_() {
    chrome.send(
        'login.ArcTermsOfServiceScreen.userActed', ['metrics-learn-more']);
    this.lastFocusedElement_ = this.shadowRoot.activeElement;
    this.$.arcMetricsPopup.showDialog();
  },

  /**
   * On-tap event handler for backup and restore learn more link
   * @private
   */
  onBackupRestoreLearnMoreTap_() {
    chrome.send(
        'login.ArcTermsOfServiceScreen.userActed',
        ['backup-restore-learn-more']);
    this.lastFocusedElement_ = this.shadowRoot.activeElement;
    if (this.isChild) {
      this.$.arcBackupRestoreChildPopup.showDialog();
    } else {
      this.$.arcBackupRestorePopup.showDialog();
    }
  },

  /**
   * On-tap event handler for location service learn more link
   * @private
   */
  onLocationServiceLearnMoreTap_() {
    chrome.send(
        'login.ArcTermsOfServiceScreen.userActed',
        ['location-service-learn-more']);
    this.lastFocusedElement_ = this.shadowRoot.activeElement;
    this.$.arcLocationServicePopup.showDialog();
  },

  /**
   * On-tap event handler for Play auto install learn more link
   * @private
   */
  onPaiLearnMoreTap_() {
    chrome.send(
        'login.ArcTermsOfServiceScreen.userActed',
        ['play-auto-install-learn-more']);
    this.lastFocusedElement_ = this.shadowRoot.activeElement;
    this.$.arcPaiPopup.showDialog();
  },

  /*
   * Callback when overlay is closed.
   * @private
   */
  onOverlayClosed_() {
    if (this.lastFocusedElement_) {
      this.lastFocusedElement_.focus();
      this.lastFocusedElement_ = null;
    }
  }
});
