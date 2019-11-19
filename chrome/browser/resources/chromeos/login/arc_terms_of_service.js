// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Polymer element for displaying material design for ARC Terms Of
 * Service screen.
 */

Polymer({
  is: 'arc-tos-root',

  behaviors: [OobeDialogHostBehavior],

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
     *     onNext: function(),
     *     onSkip: function(),
     *     reloadPlayStoreToS: function(),
     * }}
     */
    screen: {
      type: Object,
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

  /** Called when dialog is shown */
  onBeforeShow: function() {
    this.behaviors.forEach((behavior) => {
      if (behavior.onBeforeShow)
        behavior.onBeforeShow.call(this);
    });
    this.is_shown_ = true;
    window.setTimeout(this.applyOobeConfiguration_.bind(this), 0);
  },

  /**
   * Returns element by its id.
   */
  getElement: function(id) {
    return this.$[id];
  },

  /**
   * Returns focused element inside this element.
   */
  getActiveElement: function(id) {
    return this.shadowRoot.activeElement;
  },

  /**
   * Called when dialog is shown for the first time.
   *
   * @private
   */
  applyOobeConfiguration_: function() {
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
  buttonsDisabledStateChanged_: function(newValue, oldValue) {
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
  onAccept_: function() {
    this.screen.onAccept();
  },

  /**
   * On-tap event handler for Next button.
   *
   * @private
   */
  onNext_: function() {
    this.screen.onNext();
  },

  /**
   * On-tap event handler for Retry button.
   *
   * @private
   */
  onRetry_: function() {
    this.screen.reloadPlayStoreToS();
  },

  /**
   * On-tap event handler for Skip button.
   *
   * @private
   */
  onSkip_: function() {
    this.screen.onSkip();
  },

  /**
   * On-tap event handler for Back button.
   *
   * @private
   */
  onBack_: function() {
    chrome.send('login.ArcTermsOfServiceScreen.userActed', ['go-back']);
  }
});
