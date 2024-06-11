// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Polymer element containing all Sim lock dialogs
 */

import '//resources/ash/common/cr_elements/cr_button/cr_button.js';
import '//resources/ash/common/cr_elements/cr_dialog/cr_dialog.js';
import '//resources/ash/common/cr_elements/icons.html.js';
import '//resources/ash/common/cr_elements/cr_shared_style.css.js';
import '//resources/polymer/v3_0/iron-flex-layout/iron-flex-layout-classes.js';
import '//resources/polymer/v3_0/iron-icon/iron-icon.js';
import './network_password_input.js';
import './network_shared.css.js';

import {assertNotReached} from '//resources/ash/common/assert.js';
import {I18nBehavior} from '//resources/ash/common/i18n_behavior.js';
import {loadTimeData} from '//resources/ash/common/load_time_data.m.js';
import {CellularSimState, CrosNetworkConfigInterface, GlobalPolicy} from '//resources/mojo/chromeos/services/network_config/public/mojom/cros_network_config.mojom-webui.js';
import {Polymer} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {MojoInterfaceProvider, MojoInterfaceProviderImpl} from './mojo_interface_provider.js';
import {OncMojo} from './onc_mojo.js';
import {getTemplate} from './sim_lock_dialogs.html.js';

/** @enum {string} */
const ErrorType = {
  NONE: 'none',
  INCORRECT_PIN: 'incorrect-pin',
  INCORRECT_PUK: 'incorrect-puk',
  MISMATCHED_PIN: 'mismatched-pin',
  INVALID_PIN: 'invalid-pin',
  INVALID_PUK: 'invalid-puk',
};

const DIGITS_ONLY_REGEX = /^[0-9]+$/;
const PIN_MIN_LENGTH = 4;
const PUK_MIN_LENGTH = 8;

Polymer({
  _template: getTemplate(),
  is: 'sim-lock-dialogs',

  behaviors: [I18nBehavior],

  properties: {
    /** @type {?OncMojo.DeviceStateProperties} */
    deviceState: {
      type: Object,
      value: null,
      observer: 'deviceStateChanged_',
    },

    /** @type {!GlobalPolicy|undefined} */
    globalPolicy: Object,

    /**
     * Set to true when there is an open dialog.
     * @type {boolean}
     */
    isDialogOpen: {
      type: Boolean,
      value: false,
      notify: true,
    },

    /**
     * Set to true if sim lockEnabled is changed.
     * @type {boolean}
     */
    showChangePin: {
      type: Boolean,
      value: false,
    },

    /**
     * Set to true when a SIM operation is in progress. Used to disable buttons.
     * @private
     */
    inProgress_: {
      type: Boolean,
      value: false,
      observer: 'updateSubmitButtonEnabled_',
    },

    /**
     * Set to an ErrorType value after an incorrect PIN or PUK entry.
     * @private {ErrorType}
     */
    error_: {
      type: Object,
      value: ErrorType.NONE,
      observer: 'updateSubmitButtonEnabled_',
    },

    /** @private */
    hasErrorText_: {
      type: Boolean,
      computed: 'computeHasErrorText_(error_, deviceState)',
      reflectToAttribute: true,
    },

    /**
     * Error, if defined, that error_ should be set as the next time deviceState
     * updates.
     * @private {ErrorType|undefined}
     */
    pendingError_: {
      type: Object,
    },

    /**
     * Used to enable enter button in |enterPin| dialog.
     * @private
     */
    enterPinEnabled_: Boolean,

    /**
     * Used to enable change button in |changePinDialog| dialog.
     * @private
     */
    changePinEnabled_: Boolean,

    /**
     * Used to enable unlock button in |unlockPukDialog| or |unlockPinDialog|
     * dialog.
     * @private
     */
    enterPukEnabled_: Boolean,

    /**
     * Current network pin.
     * @private
     */
    pin_: {
      type: String,
      observer: 'pinOrPukChange_',
    },

    /**
     * New network pin.Property reflecting a new pin when a new pin is
     * created.
     * @private
     */
    pin_new1_: {
      type: String,
      observer: 'pinOrPukChange_',
    },

    /**
     * New network pin. Property used when reenter pin is required. This
     * happens when a new pin is being created. When a user is choosing a new
     * pin, the new pin needs to be entered twice to confirm it was entered
     * correctly. |pin_new2_| is the second entry for confirmation, it is
     * checked against |pin_new1_|, if they match the new pin is set.
     * @private
     */
    pin_new2_: {
      type: String,
      observer: 'pinOrPukChange_',
    },

    /**
     * Code provided by carrier, used when unlocking a locked cellular SIM or
     * eSIM profile.
     * @private
     */
    puk_: {
      type: String,
      observer: 'pinOrPukChange_',
    },

    /** @private {boolean} */
    isSimPinLockRestricted_: {
      type: Boolean,
      value: false,
      computed: 'computeIsSimPinLockRestricted_(globalPolicy, globalPolicy.*)',
    },
  },

  /** @private {?CrosNetworkConfigInterface} */
  networkConfig_: null,

  /** @override */
  created() {
    this.networkConfig_ =
        MojoInterfaceProviderImpl.getInstance().getMojoServiceRemote();
  },

  /** @override */
  attached() {
    if (!this.deviceState) {
      return;
    }

    this.updateDialogVisibility_();
  },

  /**
   * @param {?OncMojo.DeviceStateProperties} newDeviceState
   * @param {?OncMojo.DeviceStateProperties} oldDeviceState
   * @private
   */
  deviceStateChanged_(newDeviceState, oldDeviceState) {
    // Do not attempt to show a dialog if the current deviceState is invalid,
    // or it is set for the first time.
    if (!oldDeviceState || !newDeviceState) {
      return;
    }
    if (this.pendingError_) {
      // If pendingError_ is defined, we were waiting for the next deviceState
      // change to set error_ to the same value as pendingError_.
      this.error_ = this.pendingError_;
      this.pendingError_ = undefined;
    }
    this.updateDialogVisibility_();
  },

  /** @private */
  updateDialogVisibility_() {
    const simLockStatus = this.deviceState.simLockStatus;

    if (!simLockStatus) {
      this.isDialogOpen = false;
      return;
    }

    // If device is carrier locked, don't show any dialog
    // Device could only be unlocked by carrier
    if (simLockStatus.lockType === 'network-pin') {
      this.isDialogOpen = false;
      return;
    }

    // If lock is not enabled. Show enter pin to toggle it on.
    if (!simLockStatus.lockEnabled) {
      this.showEnterPinDialog_();
      this.isDialogOpen = true;
      return;
    }

    // If lock is enabled and PIN/PUK is required show unlock dialog
    // else it's either a change PIN or toggle PIN.
    if (simLockStatus.lockType === 'sim-puk') {
      if (this.$.unlockPukDialog.open) {
        return;
      }
      // If the PUK was activated while attempting to enter or change a pin,
      // close the dialog and open the unlock PUK dialog.
      this.closeDialogs_(/*skipIsDialogOpenUpdate=*/ true);
      this.showUnlockPukDialog_();
    } else if (simLockStatus.lockType === 'sim-pin') {
      this.showUnlockPinDialog_();
    } else if (this.showChangePin) {
      this.showChangePinDialog_();
    } else {
      this.showEnterPinDialog_();
    }
    this.isDialogOpen = true;
  },

  /** @private */
  showEnterPinDialog_() {
    if (this.$.enterPinDialog.open) {
      return;
    }

    this.$.enterPin.value = '';
    this.$.enterPinDialog.showModal();
    requestAnimationFrame(() => {
      this.focusDialogInput_();
    });
  },

  /** @private */
  showChangePinDialog_() {
    if (this.$.changePinDialog.open) {
      return;
    }

    this.$.changePinOld.value = '';
    this.$.changePinNew1.value = '';
    this.$.changePinNew2.value = '';
    this.$.changePinDialog.showModal();
    requestAnimationFrame(() => {
      this.focusDialogInput_();
    });
  },

  /** @private */
  showUnlockPukDialog_() {
    if (this.$.unlockPukDialog.open) {
      return;
    }

    this.error_ = ErrorType.NONE;
    this.$.unlockPuk.value = '';
    this.$.unlockPin1.value = '';
    this.$.unlockPin2.value = '';
    this.$.unlockPukDialog.showModal();
    requestAnimationFrame(() => {
      this.$.unlockPuk.focus();
    });
  },

  /** @private */
  showUnlockPinDialog_() {
    if (this.$.unlockPinDialog.open) {
      return;
    }

    this.error_ = ErrorType.NONE;
    this.$.unlockPin.value = '';
    this.$.unlockPinDialog.showModal();
    requestAnimationFrame(() => {
      this.$.unlockPin.focus();
    });
  },

  /**
   * @return {boolean}
   * @private
   */
  computeIsSimPinLockRestricted_() {
    return !!this.globalPolicy && !this.globalPolicy.allowCellularSimLock;
  },

  /**
   * Clears error message on user interacion.
   * @private
   */
  pinOrPukChange_() {
    this.error_ = ErrorType.NONE;
    this.updateSubmitButtonEnabled_();
  },

  /**
   * Sends the PIN value from the Enter PIN dialog.
   * @param {!Event} event
   * @private
   */
  sendEnterPin_(event) {
    event.stopPropagation();
    if (!this.enterPinEnabled_) {
      return;
    }
    const pin = this.$.enterPin.value;
    if (!this.validatePin_(pin)) {
      return;
    }

    const isPinRequired = !!this.deviceState &&
        !!this.deviceState.simLockStatus &&
        !this.deviceState.simLockStatus.lockEnabled;

    const simState = {
      currentPinOrPuk: pin,
      requirePin: isPinRequired,
    };

    this.setCellularSimState_(simState);
  },

  /**
   * Sends the old and new PIN values from the Change PIN dialog.
   * @param {!Event} event
   * @private
   */
  sendChangePin_(event) {
    event.stopPropagation();
    const newPin = this.$.changePinNew1.value;
    if (!this.validatePin_(newPin, this.$.changePinNew2.value)) {
      return;
    }
    const simState = {
      currentPinOrPuk: this.$.changePinOld.value,
      newPin: newPin,
      requirePin: true,
    };
    this.setCellularSimState_(simState);
  },

  /**
   * Sends the PUK value and new PIN value from the Unblock PUK dialog.
   * @param {!Event} event
   * @private
   */
  sendUnlockPuk_(event) {
    event.stopPropagation();
    const puk = this.$.unlockPuk.value;
    if (!this.validatePuk_(puk)) {
      return;
    }

    if (this.isSimPinLockRestricted_) {
      this.unlockCellularSim_('', puk);
      return;
    }

    const pin = this.$.unlockPin1.value;
    if (!this.validatePin_(pin, this.$.unlockPin2.value)) {
      return;
    }
    this.unlockCellularSim_(pin, puk);
  },

  /**
   * Sends the PIN value from the Unlock PIN dialog.
   * @param {!Event} event
   * @private
   */
  sendUnlockPin_(event) {
    event.stopPropagation();
    const pin = this.$.unlockPin.value;
    if (!this.validatePin_(pin)) {
      return;
    }
    this.unlockCellularSim_(pin);
  },

  /**
   * @param {!CellularSimState} cellularSimState
   * @private
   */
  setCellularSimState_(cellularSimState) {
    this.setInProgress_();
    this.networkConfig_.setCellularSimState(cellularSimState).then(response => {
      this.inProgress_ = false;
      if (!response.success) {
        // deviceState is not updated with the new cellularSimState when the
        // response returns, set pendingError_ as the value error_ should be set
        // as on the next deviceState change.
        this.pendingError_ = ErrorType.INCORRECT_PIN;
        this.focusDialogInput_();
      } else {
        this.error_ = ErrorType.NONE;
        this.closeDialogs_();
      }
    });
    this.fire('user-action-setting-change');
  },

  /**
   * Closes current dialog and sets the current state of dialogs
   * |skipIsDialogOpenUpdate| is optional because in some cases we do
   * not want to update the current dialog open state
   * @param {?boolean=} skipIsDialogOpenUpdate
   * @private
   */
  closeDialogs_(skipIsDialogOpenUpdate) {
    if (this.$.enterPinDialog.open) {
      this.$.enterPinDialog.close();
    }
    if (this.$.changePinDialog.open) {
      this.$.changePinDialog.close();
    }
    if (this.$.unlockPinDialog.open) {
      this.$.unlockPinDialog.close();
    }
    if (this.$.unlockPukDialog.open) {
      this.$.unlockPukDialog.close();
    }
    this.isDialogOpen = skipIsDialogOpenUpdate ? skipIsDialogOpenUpdate : false;
  },

  /**
   * Used by test to simulate dialog cancel click.
   */
  closeDialogsForTest() {
    this.closeDialogs_();
  },

  /**
   * @param {!Event} event
   * @private
   */
  onCancel_(event) {
    event.stopPropagation();
    this.closeDialogs_();
  },

  /** @private */
  setInProgress_() {
    this.error_ = ErrorType.NONE;
    this.pendingError_ = ErrorType.NONE;
    this.inProgress_ = true;
  },

  /** @private */
  updateSubmitButtonEnabled_() {
    const hasError = this.error_ !== ErrorType.NONE;
    this.enterPinEnabled_ = !this.inProgress_ && !!this.pin_ && !hasError;
    this.changePinEnabled_ = !this.inProgress_ && !!this.pin_ &&
        !!this.pin_new1_ && !!this.pin_new2_ && !hasError;
    this.enterPukEnabled_ = !this.inProgress_ && !!this.puk_ && !hasError &&
        (this.isSimPinLockRestricted_ ||
         (!!this.pin_new1_ && !!this.pin_new2_));
  },

  /**
   * @param {string} pin
   * @param {string=} opt_puk
   * @private
   */
  unlockCellularSim_(pin, opt_puk) {
    this.setInProgress_();
    const cellularSimState = {
      currentPinOrPuk: opt_puk || pin,
      requirePin: false,
    };
    if (opt_puk) {
      cellularSimState.newPin = pin;
    }

    this.networkConfig_.setCellularSimState(cellularSimState).then(response => {
      this.inProgress_ = false;
      if (!response.success) {
        // deviceState is not updated with the new cellularSimState when the
        // response returns, set pendingError_ as the value error_ should be set
        // as on the next deviceState change.
        this.pendingError_ =
            opt_puk ? ErrorType.INCORRECT_PUK : ErrorType.INCORRECT_PIN;
        this.focusDialogInput_();
      } else {
        this.error_ = ErrorType.NONE;
        this.closeDialogs_();
      }
    });
  },

  /** @private */
  focusDialogInput_() {
    if (this.$.enterPinDialog.open) {
      this.$.enterPin.focus();
    } else if (this.$.changePinDialog.open) {
      if (this.isSecondNewPinInvalid_()) {
        this.$.changePinNew2.focus();
      } else {
        this.$.changePinOld.focus();
      }
    } else if (this.$.unlockPinDialog.open) {
      this.$.unlockPin.focus();
    } else if (this.$.unlockPukDialog.open) {
      this.$.unlockPuk.focus();
    }
  },

  /**
   * Checks whether |pin1| is of the proper length and contains only digits.
   * If opt_pin2 is not undefined, then it also checks whether pin1 and
   * opt_pin2 match. On any failure, sets |this.error_|, focuses the invalid
   * PIN, and returns false.
   * @param {string} pin1
   * @param {string=} opt_pin2
   * @return {boolean} True if the pins match and are of minimum length.
   * @private
   */
  validatePin_(pin1, opt_pin2) {
    if (!pin1.length) {
      return false;
    }
    if (pin1.length < PIN_MIN_LENGTH || !DIGITS_ONLY_REGEX.test(pin1)) {
      this.error_ = ErrorType.INVALID_PIN;
      this.focusDialogInput_();
      return false;
    }
    if (opt_pin2 !== undefined && pin1 !== opt_pin2) {
      this.error_ = ErrorType.MISMATCHED_PIN;
      this.focusDialogInput_();
      return false;
    }
    return true;
  },

  /**
   * Checks whether |puk| is of the proper length and contains only digits.
   * If not, sets |this.error_| and returns false.
   * @param {string} puk
   * @return {boolean} True if the puk is of minimum length.
   * @private
   */
  validatePuk_(puk) {
    if (puk.length < PUK_MIN_LENGTH || !DIGITS_ONLY_REGEX.test(puk)) {
      this.error_ = ErrorType.INVALID_PUK;
      return false;
    }
    return true;
  },

  /**
   * @return {string}
   * @private
   */
  getEnterPinDescription_() {
    return this.isSimPinLockRestricted_ ?
        this.i18n('networkSimLockPolicyAdminSubtitle') :
        this.i18n('networkSimEnterPinDescription');
  },

  /**
   * @return {string}
   * @private
   */
  getErrorMsg_() {
    if (this.error_ === ErrorType.NONE) {
      return '';
    } else if (this.error_ === ErrorType.MISMATCHED_PIN) {
      return this.i18n('networkSimErrorPinMismatch');
    }

    let errorStringId = '';
    switch (this.error_) {
      case ErrorType.INCORRECT_PIN:
        errorStringId = 'networkSimErrorIncorrectPin';
        break;
      case ErrorType.INCORRECT_PUK:
        errorStringId = 'networkSimErrorIncorrectPuk';
        break;
      case ErrorType.INVALID_PIN:
        errorStringId = 'networkSimErrorInvalidPin';
        break;
      case ErrorType.INVALID_PUK:
        errorStringId = 'networkSimErrorInvalidPuk';
        break;
      default:
        assertNotReached();
    }

    // Invalid PIN errors show a separate string based on whether there is 1
    // retry left or not.
    const retriesLeft = this.getNumRetriesLeft_();
    if (retriesLeft !== 1 &&
        (this.error_ === ErrorType.INCORRECT_PIN ||
         this.error_ === ErrorType.INVALID_PIN)) {
      errorStringId += 'Plural';
    }

    return this.i18n(errorStringId, retriesLeft);
  },

  /**
   * @return {number}
   * @private
   */
  getNumRetriesLeft_() {
    if (!this.deviceState || !this.deviceState.simLockStatus) {
      return 0;
    }

    return this.deviceState.simLockStatus.retriesLeft;
  },

  /**
   * @return {boolean}
   * @private
   */
  computeHasErrorText_() {
    return !!this.getErrorMsg_();
  },

  /**
   * @return {string}
   * @private
   */
  getPinEntrySubtext_() {
    const errorMessage = this.getErrorMsg_();
    if (errorMessage) {
      return errorMessage;
    }

    return this.i18n('networkSimEnterPinSubtext');
  },

  /**
   * @return {boolean}
   * @private
   */
  isOldPinInvalid_() {
    return this.error_ === ErrorType.INCORRECT_PIN ||
        this.error_ === ErrorType.INVALID_PIN;
  },

  /**
   * @return {string}
   * @private
   */
  getOldPinErrorMessage_() {
    if (this.isOldPinInvalid_()) {
      return this.getErrorMsg_();
    }

    return '';
  },

  /**
   * @return {boolean}
   * @private
   */
  isSecondNewPinInvalid_() {
    return this.error_ === ErrorType.MISMATCHED_PIN;
  },

  /**
   * @return {string}
   * @private
   */
  getSecondNewPinErrorMessage_() {
    if (this.isSecondNewPinInvalid_()) {
      return this.getErrorMsg_();
    }

    return '';
  },

  /**
   * @return {boolean}
   * @private
   */
  isPukInvalid_() {
    return this.error_ === ErrorType.INCORRECT_PUK ||
        this.error_ === ErrorType.INVALID_PUK;
  },

  /**
   * @return {string}
   * @private
   */
  getPukErrorMessage_() {
    if (this.isPukInvalid_()) {
      return this.getErrorMsg_();
    }

    return '';
  },

  /**
   * @return {string}
   * @private
   */
  getPukWarningMessage_() {
    return this.isSimPinLockRestricted_ ?
        this.getPukWarningSimPinRestrictedMessage_() :
        this.getPukWarningSimPinUnrestrictedMessage_();
  },

  /**
   * @return {string}
   * @private
   */
  getNetworkSimPukDialogString_() {
    return this.isSimPinLockRestricted_ ?
        this.i18n('networkSimPukDialogManagedSubtitle') :
        this.i18n('networkSimPukDialogSubtitle');
  },

  /**
   * @return {string}
   * @private
   */
  getPukWarningSimPinUnrestrictedMessage_() {
    if (this.isPukInvalid_()) {
      const retriesLeft = this.getNumRetriesLeft_();
      if (retriesLeft === 1) {
        return this.i18n('networkSimPukDialogWarningWithFailure', retriesLeft);
      }

      return this.i18n('networkSimPukDialogWarningWithFailures', retriesLeft);
    }

    return this.i18n('networkSimPukDialogWarningNoFailures');
  },

  /**
   * @return {string}
   * @private
   */
  getPukWarningSimPinRestrictedMessage_() {
    if (this.isPukInvalid_()) {
      const retriesLeft = this.getNumRetriesLeft_();
      if (retriesLeft === 1) {
        return this.i18n(
            'networkSimPukDialogManagedWarningWithFailure', retriesLeft);
      }

      return this.i18n(
          'networkSimPukDialogManagedWarningWithFailures', retriesLeft);
    }

    return this.i18n('networkSimPukDialogManagedWarningNoFailures');
  },
});
