// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Polymer element for displaying material design reset screen.
 */

(function() {
/** @enum {number} */
const RESET_SCREEN_STATE = {
  'RESTART_REQUIRED': 0,
  'REVERT_PROMISE': 1,
  'POWERWASH_PROPOSAL': 2,  // supports 2 ui-states - With or without rollback
  'ERROR': 3,
};

// When the screen is in the powerwash proposal state, it depends on the mode
/** @enum {number} */
const POWERWASH_MODE = {
  'POWERWASH_WITH_ROLLBACK': 0,
  'POWERWASH_ONLY': 1,
};

// Powerwash mode details. Used by the UI for the two different modes
/** @type {Map<POWERWASH_MODE, Object<string,string>>} */
const POWERWASH_MODE_DETAILS = new Map([
  [
    POWERWASH_MODE.POWERWASH_WITH_ROLLBACK, {
      subtitleText: 'resetPowerwashRollbackWarningDetails',
      dialogTitle: 'confirmRollbackTitle',
      dialogContent: 'confirmRollbackMessage',
      buttonTextKey: 'resetButtonPowerwashAndRollback',
    }
  ],
  [
    POWERWASH_MODE.POWERWASH_ONLY, {
      subtitleText: 'resetPowerwashWarningDetails',
      dialogTitle: 'confirmPowerwashTitle',
      dialogContent: 'confirmPowerwashMessage',
      buttonTextKey: 'resetButtonPowerwash',
    }
  ],
]);

Polymer({
  is: 'oobe-reset-element',

  behaviors: [OobeI18nBehavior, OobeDialogHostBehavior, LoginScreenBehavior],

  EXTERNAL_API: [
    'setIsRollbackAvailable',
    'setIsRollbackRequested',
    'setIsTpmFirmwareUpdateAvailable',
    'setIsTpmFirmwareUpdateChecked',
    'setIsTpmFirmwareUpdateEditable',
    'setTpmFirmwareUpdateMode',
    'setShouldShowConfirmationDialog',
    'setScreenState',
  ],

  properties: {

    /* The current state of the screen as set from the C++ side. */
    /** @type {RESET_SCREEN_STATE} */
    screenState_: {
      type: Number,
      value: RESET_SCREEN_STATE.RESTART_REQUIRED,
      observer: 'onScreenStateChanged_',
    },

    /** @type {boolean}  Whether rollback is available */
    isRollbackAvailable_: {
      type: Boolean,
      value: false,
      observer: 'updatePowerwashModeBasedOnRollbackOptions_',
    },

    /**
     * @type {boolean}
     *  Whether the rollback option was chosen by the user.
     */
    isRollbackRequested_: {
      type: Boolean,
      value: false,
      observer: 'updatePowerwashModeBasedOnRollbackOptions_',
    },

    /**
     * Whether to show the TPM firmware update checkbox.
     */
    tpmUpdateAvailable_: Boolean,

    /**
     * If the checkbox to request a TPM firmware update is checked.
     */
    tpmUpdateChecked_: Boolean,

    /**
     * If the checkbox to request a TPM firmware update is editable.
     */
    tpmUpdateEditable_: Boolean,

    /**
     * The current TPM update mode.
     */
    tpmUpdateMode_: String,

    // Title to be shown on the confirmation dialog.
    confirmationDialogTitle_: {
      type: String,
      computed: 'getConfirmationDialogTitle_(locale, powerwashMode_)',
    },

    // Content to be shown on the confirmation dialog.
    confirmationDialogText_: {
      type: String,
      computed: 'getConfirmationDialogText_(locale, powerwashMode_)',
    },

    // The subtitle to be shown while the screen is in POWERWASH_PROPOSAL
    powerwashStateSubtitle_: {
      type: String,
      computed: 'getPowerwashStateSubtitle_(locale, powerwashMode_)'
    },

    // The text shown on the powerwash button. (depends on powerwash mode)
    powerwashButtonTextKey_: {
      type: String,
      computed: 'getPowerwashButtonTextKey_(locale, powerwashMode_)'
    },

    // Whether the powerwash button is disabled.
    powerwashButtonDisabled_: {
      type: Boolean,
      computed: 'isPowerwashDisabled_(powerwashMode_, tpmUpdateChecked_)',
    },

    // The chosen powerwash mode
    /**@type {POWERWASH_MODE} */
    powerwashMode_: {
      type: Number,
      value: POWERWASH_MODE.POWERWASH_ONLY,
    },

    // Simple variables that reflect the current screen state
    // Only modified by the observer of 'screenState_'
    inRestartRequiredState_: {
      type: Boolean,
      value: true,
    },

    inRevertState_: {
      type: Boolean,
      value: false,
    },

    inPowerwashState_: {
      type: Boolean,
      value: false,
    },
  },

  /** @override */
  ready() {
    this.initializeLoginScreen('ResetScreen', {
      resetAllowed: false,
    });
  },

  focus() {
    this.$.resetDialog.focus();
  },

  reset() {
    this.screenState_ = RESET_SCREEN_STATE.RESTART_REQUIRED;
    this.thispowerwashMode_ = POWERWASH_MODE.POWERWASH_ONLY;
    this.tpmUpdateAvailable_ = false;
    this.isRollbackAvailable_ = false;
    this.isRollbackRequested_ = false;
  },

  /* ---------- EXTERNAL API BEGIN ---------- */
  /** @param {boolean} rollbackAvailable  */
  setIsRollbackAvailable(rollbackAvailable) {
    this.isRollbackAvailable_ = rollbackAvailable;
  },

  /**
   * @param {boolean} rollbackRequested
   */
  setIsRollbackRequested(rollbackRequested) {
    this.isRollbackRequested_ = rollbackRequested;
  },

  /** @param {boolean} value  */
  setIsTpmFirmwareUpdateAvailable(value) {
    this.tpmUpdateAvailable_ = value;
  },

  /** @param {boolean} value  */
  setIsTpmFirmwareUpdateChecked(value) {
    this.tpmUpdateChecked_ = value;
  },

  /** @param {boolean} value  */
  setIsTpmFirmwareUpdateEditable(value) {
    this.tpmUpdateEditable_ = value;
  },

  /** @param {string} value  */
  setTpmFirmwareUpdateMode(value) {
    this.tpmUpdateMode_ = value;
  },

  /** @param {boolean} should_show  */
  setShouldShowConfirmationDialog(should_show) {
    if (should_show) {
      this.$.confirmationDialog.showDialog();
    } else {
      this.$.confirmationDialog.hideDialog();
    }
  },

  /** @param {RESET_SCREEN_STATE} state  */
  setScreenState(state) {
    this.screenState_ = state;
  },
  /* ---------- EXTERNAL API END ---------- */

  /**
   *  When rollback is available and requested, the powerwash mode changes
   *  to POWERWASH_WITH_ROLLBACK.
   *  @private
   */
  updatePowerwashModeBasedOnRollbackOptions_() {
    if (this.isRollbackAvailable_ && this.isRollbackRequested_) {
      this.powerwashMode_ = POWERWASH_MODE.POWERWASH_WITH_ROLLBACK;
      this.classList.add('rollback-proposal-view');
    } else {
      this.powerwashMode_ = POWERWASH_MODE.POWERWASH_ONLY;
      this.classList.remove('rollback-proposal-view');
    }
  },

  /** @private */
  onScreenStateChanged_() {
    if (this.screenState_ == RESET_SCREEN_STATE.REVERT_PROMISE) {
      announceAccessibleMessage(this.i18n('resetRevertSpinnerMessage'));
      this.classList.add('revert-promise-view');
    } else {
      this.classList.remove('revert-promise-view');
    }

    this.inRevertState_ =
        (this.screenState_ == RESET_SCREEN_STATE.REVERT_PROMISE);
    this.inRestartRequiredState_ =
        (this.screenState_ == RESET_SCREEN_STATE.RESTART_REQUIRED);
    this.inPowerwashState_ =
        (this.screenState_ == RESET_SCREEN_STATE.POWERWASH_PROPOSAL);
  },

  /**
   * Determines the subtitle based on the current powerwash mode
   * @param {*} locale
   * @param {POWERWASH_MODE} mode
   * @private
   */
  getPowerwashStateSubtitle_(locale, mode) {
    if (this.powerwashMode_ === undefined)
      return '';
    const modeDetails = POWERWASH_MODE_DETAILS.get(this.powerwashMode_);
    return this.i18n(modeDetails.subtitleText);
  },

  /**
   * The powerwash button text depends on the powerwash mode
   * @param {*} locale
   * @param {POWERWASH_MODE} mode
   * @private
   */
  getPowerwashButtonTextKey_(locale, mode) {
    if (this.powerwashMode_ === undefined)
      return '';
    return POWERWASH_MODE_DETAILS.get(this.powerwashMode_).buttonTextKey;
  },

  /**
   * Cannot powerwash with rollback when the TPM update checkbox is checked
   * @param {POWERWASH_MODE} mode
   * @param {boolean} tpmUpdateChecked
   * @private
   */
  isPowerwashDisabled_(mode, tpmUpdateChecked) {
    return this.tpmUpdateChecked_ &&
        (this.powerwashMode_ == POWERWASH_MODE.POWERWASH_WITH_ROLLBACK);
  },

  /* ---------- CONFIRMATION DIALOG ---------- */

  /**
   * Determines the confirmation dialog title.
   * @param {*} locale
   * @param {POWERWASH_MODE} mode
   * @private
   */
  getConfirmationDialogTitle_(locale, mode) {
    if (this.powerwashMode_ === undefined)
      return '';
    const modeDetails = POWERWASH_MODE_DETAILS.get(this.powerwashMode_);
    return this.i18n(modeDetails.dialogTitle);
  },

  /**
   * Determines the confirmation dialog content
   * @param {*} locale
   * @param {POWERWASH_MODE} mode
   * @private
   */
  getConfirmationDialogText_(locale, mode) {
    if (this.powerwashMode_ === undefined)
      return '';
    const modeDetails = POWERWASH_MODE_DETAILS.get(this.powerwashMode_);
    return this.i18n(modeDetails.dialogContent);
  },

  /**
   * On-tap event handler for confirmation dialog continue button.
   * @private
   */
  onDialogContinueTap_() {
    this.userActed('powerwash-pressed');
  },

  /**
   * On-tap event handler for confirmation dialog cancel button.
   * @private
   */
  onDialogCancelTap_() {
    this.$.confirmationDialog.hideDialog();
    this.userActed('reset-confirm-dismissed');
  },

  /**
   * Catch 'close' event through escape key
   * @private
   */
  onDialogClosed_() {
    this.userActed('reset-confirm-dismissed');
  },

  /* ---------- SIMPLE EVENT HANDLERS ---------- */
  /**
   * On-tap event handler for cancel button.
   * @private
   */
  onCancelTap_() {
    this.userActed('cancel-reset');
  },

  /**
   * On-tap event handler for restart button.
   * @private
   */
  onRestartTap_() {
    this.userActed('restart-pressed');
  },

  /**
   * On-tap event handler for powerwash button.
   * @private
   */
  onPowerwashTap_() {
    this.userActed('show-confirmation');
  },

  /**
   * On-tap event handler for learn more link.
   * @private
   */
  onLearnMoreTap_() {
    this.userActed('learn-more-link');
  },

  /**
   * Change handler for TPM firmware update checkbox.
   * @private
   */
  onTPMFirmwareUpdateChanged_() {
    const checked = this.$.tpmFirmwareUpdateCheckbox.checked;
    chrome.send('ResetScreen.setTpmFirmwareUpdateChecked', [checked]);
  },

  /**
   * On-tap event handler for the TPM firmware update learn more link.
   * @param {!Event} event
   * @private
   */
  onTPMFirmwareUpdateLearnMore_(event) {
    this.userActed('tpm-firmware-update-learn-more-link');
    event.stopPropagation();
  },
});
})();
