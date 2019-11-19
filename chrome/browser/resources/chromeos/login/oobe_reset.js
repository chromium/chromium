// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Polymer element for displaying material design reset screen.
 */

Polymer({
  is: 'oobe-reset-md',

  behaviors: [I18nBehavior, OobeDialogHostBehavior],

  properties: {
    /**
     * State of the screen corresponding to the css style set by
     * oobe_screen_reset.js.
     */
    uiState_: String,

    /**
     * Flag that determines whether help link is shown.
     */
    isOfficial_: Boolean,

    /**
     * Whether to show the TPM firmware update checkbox.
     */
    tpmFirmwareUpdateAvailable_: Boolean,

    /**
     * If the checkbox to request a TPM firmware update is checked.
     */
    tpmFirmwareUpdateChecked_: Boolean,

    /**
     * If the checkbox to request a TPM firmware update is editable.
     */
    tpmFirmwareUpdateEditable_: Boolean,

    /**
     * Reference to OOBE screen object.
     * @type {!OobeTypes.Screen}
     */
    screen: {
      type: Object,
    },
  },

  focus: function() {
    this.$.resetDialog.focus();
  },

  /** @private */
  isState_: function(uiState_, state_) {
    return uiState_ == state_;
  },

  /** @private */
  isCancelHidden_: function(uiState_) {
    return uiState_ == 'revert-promise-view';
  },

  /** @private */
  isHelpLinkHidden_: function(uiState_, isOfficial_) {
    return !isOfficial_ || (uiState_ == 'revert-promise-view');
  },

  /** @private */
  isTPMFirmwareUpdateHidden_: function(uiState_, tpmFirmwareUpdateAvailable_) {
    var inProposalView = [
      'powerwash-proposal-view', 'rollback-proposal-view'
    ].includes(uiState_);
    return !(tpmFirmwareUpdateAvailable_ && inProposalView);
  },

  /**
   * On-tap event handler for cancel button.
   *
   * @private
   */
  onCancelTap_: function() {
    chrome.send('login.ResetScreen.userActed', ['cancel-reset']);
  },

  /**
   * On-tap event handler for restart button.
   *
   * @private
   */
  onRestartTap_: function() {
    chrome.send('login.ResetScreen.userActed', ['restart-pressed']);
  },

  /**
   * On-tap event handler for powerwash button.
   *
   * @private
   */
  onPowerwashTap_: function() {
    chrome.send('login.ResetScreen.userActed', ['show-confirmation']);
  },

  /**
   * On-tap event handler for learn more link.
   *
   * @private
   */
  onLearnMoreTap_: function() {
    chrome.send('login.ResetScreen.userActed', ['learn-more-link']);
  },

  /**
   * Change handler for TPM firmware update checkbox.
   *
   * @private
   */
  onTPMFirmwareUpdateChanged_: function() {
    this.screen.onTPMFirmwareUpdateChanged_(
        this.$.tpmFirmwareUpdateCheckbox.checked);
  },

  /**
   * On-tap event handler for the TPM firmware update learn more link.
   *
   * @param {!Event} event
   * @private
   */
  onTPMFirmwareUpdateLearnMore_: function(event) {
    chrome.send(
        'login.ResetScreen.userActed', ['tpm-firmware-update-learn-more-link']);
    event.stopPropagation();
  },

});
