// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Polymer element for displaying material design reset screen.
 */

import '//resources/cr_elements/chromeos/cros_color_overrides.css.js';
import '//resources/cr_elements/cr_checkbox/cr_checkbox.js';
import '//resources/js/action_link.js';
import '//resources/polymer/v3_0/iron-icon/iron-icon.js';
import '../../components/oobe_icons.html.js';
import '../../components/common_styles/oobe_common_styles.css.js';
import '../../components/common_styles/oobe_dialog_host_styles.css.js';
import '../../components/buttons/oobe_text_button.js';

import {announceAccessibleMessage} from '//resources/ash/common/util.js';
import {html, mixinBehaviors, PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {LoginScreenBehavior, LoginScreenBehaviorInterface} from '../../components/behaviors/login_screen_behavior.js';
import {OobeDialogHostBehavior} from '../../components/behaviors/oobe_dialog_host_behavior.js';
import {OobeI18nBehavior, OobeI18nBehaviorInterface} from '../../components/behaviors/oobe_i18n_behavior.js';
import {OobeAdaptiveDialog} from '../../components/dialogs/oobe_adaptive_dialog.js';
import {OobeModalDialog} from '../../components/dialogs/oobe_modal_dialog.js';


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
/** @type {Map<number, Object<string,string>>} */
const POWERWASH_MODE_DETAILS = new Map([
  [
    POWERWASH_MODE.POWERWASH_WITH_ROLLBACK,
    {
      subtitleText: 'resetPowerwashRollbackWarningDetails',
      dialogTitle: 'confirmRollbackTitle',
      dialogContent: 'confirmRollbackMessage',
      buttonTextKey: 'resetButtonPowerwashAndRollback',
    },
  ],
  [
    POWERWASH_MODE.POWERWASH_ONLY,
    {
      subtitleText: 'resetPowerwashWarningDetails',
      dialogTitle: 'confirmPowerwashTitle',
      dialogContent: 'confirmPowerwashMessage',
      buttonTextKey: 'resetButtonPowerwash',
    },
  ],
]);

/**
 * @constructor
 * @extends {PolymerElement}
 * @implements {LoginScreenBehaviorInterface}
 * @implements {OobeI18nBehaviorInterface}
 */
const ResetScreenElementBase = mixinBehaviors(
    [OobeI18nBehavior, OobeDialogHostBehavior, LoginScreenBehavior],
    PolymerElement);

/**
 * @typedef {{
 *   resetDialog:  OobeAdaptiveDialog,
 *   confirmationDialog:  OobeModalDialog,
 *   tpmFirmwareUpdateCheckbox, CrCheckBox,
 * }}
 */
ResetScreenElementBase.$;

/**
 * @polymer
 */
class OobeReset extends ResetScreenElementBase {
  static get is() {
    return 'oobe-reset-element';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      /* The current state of the screen as set from the C++ side. */
      screenState_: {
        type: Number,
        value: RESET_SCREEN_STATE.RESTART_REQUIRED,
        observer: 'onScreenStateChanged_',
      },

      /** Whether rollback is available */
      isRollbackAvailable_: {
        type: Boolean,
        value: false,
        observer: 'updatePowerwashModeBasedOnRollbackOptions_',
      },

      /**
       * Whether the rollback option was chosen by the user.
       */
      isRollbackRequested_: {
        type: Boolean,
        value: false,
        observer: 'updatePowerwashModeBasedOnRollbackOptions_',
      },

      /**
       * Whether to show the TPM firmware update checkbox.
       */
      tpmUpdateAvailable_: {
        type: Boolean,
      },

      /**
       * If the checkbox to request a TPM firmware update is checked.
       */
      tpmUpdateChecked_: {
        type: Boolean,
      },

      /**
       * If the checkbox to request a TPM firmware update is editable.
       */
      tpmUpdateEditable_: {
        type: Boolean,
      },

      /**
       * The current TPM update mode.
       */
      tpmUpdateMode_: {
        type: String,
      },

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
        computed: 'getPowerwashStateSubtitle_(locale, powerwashMode_)',
      },

      // The text shown on the powerwash button. (depends on powerwash mode)
      powerwashButtonTextKey_: {
        type: String,
        computed: 'getPowerwashButtonTextKey_(locale, powerwashMode_)',
      },

      // Whether the powerwash button is disabled.
      powerwashButtonDisabled_: {
        type: Boolean,
        computed: 'isPowerwashDisabled_(powerwashMode_, tpmUpdateChecked_)',
      },

      // The chosen powerwash mode
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

      inErrorState_: {
        type: Boolean,
        value: false,
      },
    };
  }

  /** Overridden from LoginScreenBehavior. */
  // clang-format off
  get EXTERNAL_API() {
    return ['setIsRollbackAvailable',
            'setIsRollbackRequested',
            'setIsTpmFirmwareUpdateAvailable',
            'setIsTpmFirmwareUpdateChecked',
            'setIsTpmFirmwareUpdateEditable',
            'setTpmFirmwareUpdateMode',
            'setShouldShowConfirmationDialog',
            'setScreenState',
    ];
  }

  // clang-format on

  /** @override */
  ready() {
    super.ready();
    this.initializeLoginScreen('ResetScreen');
  }

  /**
   * Returns the control which should receive initial focus.
   */
  get defaultControl() {
    return this.$.resetDialog;
  }

  focus() {
    this.$.resetDialog.focus();
  }

  reset() {
    this.screenState_ = RESET_SCREEN_STATE.RESTART_REQUIRED;
    this.powerwashMode_ = POWERWASH_MODE.POWERWASH_ONLY;
    this.tpmUpdateAvailable_ = false;
    this.isRollbackAvailable_ = false;
    this.isRollbackRequested_ = false;
  }

  /* ---------- EXTERNAL API BEGIN ---------- */
  /** @param {boolean} rollbackAvailable  */
  setIsRollbackAvailable(rollbackAvailable) {
    this.isRollbackAvailable_ = rollbackAvailable;
  }

  /**
   * @param {boolean} rollbackRequested
   */
  setIsRollbackRequested(rollbackRequested) {
    this.isRollbackRequested_ = rollbackRequested;
  }

  /** @param {boolean} value  */
  setIsTpmFirmwareUpdateAvailable(value) {
    this.tpmUpdateAvailable_ = value;
  }

  /** @param {boolean} value  */
  setIsTpmFirmwareUpdateChecked(value) {
    this.tpmUpdateChecked_ = value;
  }

  /** @param {boolean} value  */
  setIsTpmFirmwareUpdateEditable(value) {
    this.tpmUpdateEditable_ = value;
  }

  /** @param {string} value  */
  setTpmFirmwareUpdateMode(value) {
    this.tpmUpdateMode_ = value;
  }

  /**
   * @param {boolean} should_show
   */
  setShouldShowConfirmationDialog(should_show) {
    if (should_show) {
      this.$.confirmationDialog.showDialog();
    } else {
      this.$.confirmationDialog.hideDialog();
    }
  }

  /** @param {RESET_SCREEN_STATE} state  */
  setScreenState(state) {
    this.screenState_ = state;
  }
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
  }

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
    this.inErrorState_ = (this.screenState_ == RESET_SCREEN_STATE.ERROR);
  }

  /**
   * Determines the subtitle based on the current powerwash mode
   * @param {*} locale
   * @param {POWERWASH_MODE} mode
   * @private
   */
  getPowerwashStateSubtitle_(locale, mode) {
    if (this.powerwashMode_ === undefined) {
      return '';
    }
    const modeDetails = POWERWASH_MODE_DETAILS.get(this.powerwashMode_);
    return this.i18n(modeDetails.subtitleText);
  }

  /**
   * The powerwash button text depends on the powerwash mode
   * @param {*} locale
   * @param {POWERWASH_MODE} mode
   * @private
   */
  getPowerwashButtonTextKey_(locale, mode) {
    if (this.powerwashMode_ === undefined) {
      return '';
    }
    return POWERWASH_MODE_DETAILS.get(this.powerwashMode_).buttonTextKey;
  }

  /**
   * Cannot powerwash with rollback when the TPM update checkbox is checked
   * @param {POWERWASH_MODE} mode
   * @param {boolean} tpmUpdateChecked
   * @private
   */
  isPowerwashDisabled_(mode, tpmUpdateChecked) {
    return this.tpmUpdateChecked_ &&
        (this.powerwashMode_ == POWERWASH_MODE.POWERWASH_WITH_ROLLBACK);
  }

  /* ---------- CONFIRMATION DIALOG ---------- */

  /**
   * Determines the confirmation dialog title.
   * @param {*} locale
   * @param {POWERWASH_MODE} mode
   * @private
   */
  getConfirmationDialogTitle_(locale, mode) {
    if (this.powerwashMode_ === undefined) {
      return '';
    }
    const modeDetails = POWERWASH_MODE_DETAILS.get(this.powerwashMode_);
    return this.i18n(modeDetails.dialogTitle);
  }

  /**
   * Determines the confirmation dialog content
   * @param {*} locale
   * @param {POWERWASH_MODE} mode
   * @private
   */
  getConfirmationDialogText_(locale, mode) {
    if (this.powerwashMode_ === undefined) {
      return '';
    }
    const modeDetails = POWERWASH_MODE_DETAILS.get(this.powerwashMode_);
    return this.i18n(modeDetails.dialogContent);
  }

  /**
   * On-tap event handler for confirmation dialog continue button.
   * @private
   */
  onDialogContinueTap_() {
    this.userActed('powerwash-pressed');
  }

  /**
   * On-tap event handler for confirmation dialog cancel button.
   * @private
   * @suppress {missingProperties}
   */
  onDialogCancelTap_() {
    this.$.confirmationDialog.hideDialog();
    this.userActed('reset-confirm-dismissed');
  }

  /**
   * Catch 'close' event through escape key
   * @private
   */
  onDialogClosed_() {
    this.userActed('reset-confirm-dismissed');
  }

  /* ---------- SIMPLE EVENT HANDLERS ---------- */
  /**
   * On-tap event handler for cancel button.
   * @private
   */
  onCancelTap_() {
    this.userActed('cancel-reset');
  }

  /**
   * On-tap event handler for restart button.
   * @private
   */
  onRestartTap_() {
    this.userActed('restart-pressed');
  }

  /**
   * On-tap event handler for powerwash button.
   * @private
   */
  onPowerwashTap_() {
    this.userActed('show-confirmation');
  }

  /**
   * On-tap event handler for learn more link.
   * @private
   */
  onLearnMoreTap_() {
    this.userActed('learn-more-link');
  }

  /**
   * Change handler for TPM firmware update checkbox.
   * @private
   */
  onTPMFirmwareUpdateChanged_() {
    const checked = this.$.tpmFirmwareUpdateCheckbox.checked;
    this.userActed(['tpmfirmware-update-checked', checked]);
  }

  /**
   * On-tap event handler for the TPM firmware update learn more link.
   * @param {!Event} event
   * @private
   */
  onTPMFirmwareUpdateLearnMore_(event) {
    this.userActed('tpm-firmware-update-learn-more-link');
    event.stopPropagation();
  }
}

customElements.define(OobeReset.is, OobeReset);
