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

import {assert, assertNotReached} from '//resources/js/assert.js';
import {CellularSimState, CrosNetworkConfigInterface, GlobalPolicy} from '//resources/mojo/chromeos/services/network_config/public/mojom/cros_network_config.mojom-webui.js';
import {mixinBehaviors, PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {CrDialogElement} from 'chrome://resources/ash/common/cr_elements/cr_dialog/cr_dialog.js';
import {I18nMixin} from 'chrome://resources/ash/common/cr_elements/i18n_mixin.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';

import {strictQuery} from '../typescript_utils/strict_query.js';

import {MojoInterfaceProvider, MojoInterfaceProviderImpl} from './mojo_interface_provider.js';
import {NetworkPasswordInputElement} from './network_password_input.js';
import {OncMojo} from './onc_mojo.js';
import {getTemplate} from './sim_lock_dialogs.html.js';

enum ErrorType {
  NONE = 'none',
  INCORRECT_PIN = 'incorrect-pin',
  INCORRECT_PUK = 'incorrect-puk',
  MISMATCHED_PIN = 'mismatched-pin',
  INVALID_PIN = 'invalid-pin',
  INVALID_PUK = 'invalid-puk',
}

const DIGITS_ONLY_REGEX = /^[0-9]+$/;
const PIN_MIN_LENGTH = 4;
const PUK_MIN_LENGTH = 8;

const SimLockDialogsElementBase = I18nMixin(PolymerElement);

export class SimLockDialogsElement extends SimLockDialogsElementBase {
  static get is() {
    return 'sim-lock-dialogs' as const;
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      deviceState: {
        type: Object,
        value: null,
        observer: 'deviceStateChanged_',
      },

      globalPolicy: Object,

      /**
       * Set to true when there is an open dialog.
       */
      isDialogOpen: {
        type: Boolean,
        value: false,
        notify: true,
      },

      /**
       * Set to true if sim lockEnabled is changed.
       */
      showChangePin: {
        type: Boolean,
        value: false,
      },

      /**
       * Set to true when a SIM operation is in progress. Used to disable
       * buttons.
       */
      inProgress_: {
        type: Boolean,
        value: false,
        observer: 'updateSubmitButtonEnabled_',
      },

      /**
       * Set to an ErrorType value after an incorrect PIN or PUK entry.
       */
      error_: {
        type: Object,
        value: ErrorType.NONE,
        observer: 'updateSubmitButtonEnabled_',
      },

      hasErrorText_: {
        type: Boolean,
        computed: 'computeHasErrorText_(error_, deviceState)',
        reflectToAttribute: true,
      },

      /**
       * Error, if defined, that error_ should be set as the next time
       * deviceState updates.
       */
      pendingError_: {
        type: Object,
      },

      /**
       * Used to enable enter button in |enterPin| dialog.
       */
      enterPinEnabled_: Boolean,

      /**
       * Used to enable change button in |changePinDialog| dialog.
       */
      changePinEnabled_: Boolean,

      /**
       * Used to enable unlock button in |unlockPukDialog| or |unlockPinDialog|
       * dialog.
       */
      enterPukEnabled_: Boolean,

      /**
       * Current network pin.
       */
      pin_: {
        type: String,
        observer: 'pinOrPukChange_',
      },

      /**
       * New network pin.Property reflecting a new pin when a new pin is
       * created.
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
       */
      pin_new2_: {
        type: String,
        observer: 'pinOrPukChange_',
      },

      /**
       * Code provided by carrier, used when unlocking a locked cellular SIM or
       * eSIM profile.
       */
      puk_: {
        type: String,
        observer: 'pinOrPukChange_',
      },

      isSimPinLockRestricted_: {
        type: Boolean,
        value: false,
        computed:
            'computeIsSimPinLockRestricted_(globalPolicy, globalPolicy.*)',
      },
    };
  }

  deviceState: OncMojo.DeviceStateProperties|null;
  globalPolicy: GlobalPolicy|undefined;
  isDialogOpen: boolean;
  showChangePin: boolean;
  private inProgress_: boolean;
  private error_: ErrorType;
  private hasErrorText_: boolean;
  private pendingError_: ErrorType|undefined;
  private enterPinEnabled_: boolean;
  private changePinEnabled_: boolean;
  private enterPukEnabled_: boolean;
  private pin_: string;
  private pin_new1_: string;
  private pin_new2_: string;
  private puk_: string;
  private isSimPinLockRestricted_: boolean;
  private networkConfig_: CrosNetworkConfigInterface;

  constructor() {
    super();

    this.networkConfig_ =
        MojoInterfaceProviderImpl.getInstance().getMojoServiceRemote();
  }

  override connectedCallback() {
    super.connectedCallback();

    if (!this.deviceState) {
      return;
    }

    this.updateDialogVisibility_();
  }

  private deviceStateChanged_(
      newDeviceState: OncMojo.DeviceStateProperties|undefined,
      oldDeviceState: OncMojo.DeviceStateProperties|undefined): void {
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
  }

  private updateDialogVisibility_(): void {
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
      if (strictQuery('#unlockPukDialog', this.shadowRoot, CrDialogElement)
              .open) {
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
  }

  /** @private */
  showEnterPinDialog_() {
    if (strictQuery('#enterPinDialog', this.shadowRoot, CrDialogElement).open) {
      return;
    }

    strictQuery('#enterPin', this.shadowRoot, NetworkPasswordInputElement)
        .value = '';
    strictQuery('#enterPinDialog', this.shadowRoot, CrDialogElement)
        .showModal();
    requestAnimationFrame(() => {
      this.focusDialogInput_();
    });
  }

  /** @private */
  showChangePinDialog_() {
    if (strictQuery('#changePinDialog', this.shadowRoot, CrDialogElement)
            .open) {
      return;
    }

    strictQuery('#changePinOld', this.shadowRoot, NetworkPasswordInputElement)
        .value = '';
    strictQuery('#changePinNew1', this.shadowRoot, NetworkPasswordInputElement)
        .value = '';
    strictQuery('#changePinNew2', this.shadowRoot, NetworkPasswordInputElement)
        .value = '';
    strictQuery('#changePinDialog', this.shadowRoot, CrDialogElement)
        .showModal();
    requestAnimationFrame(() => {
      this.focusDialogInput_();
    });
  }

  /** @private */
  showUnlockPukDialog_() {
    if (strictQuery('#unlockPukDialog', this.shadowRoot, CrDialogElement)
            .open) {
      return;
    }

    this.error_ = ErrorType.NONE;
    strictQuery('#unlockPuk', this.shadowRoot, NetworkPasswordInputElement)
        .value = '';
    strictQuery('#unlockPin1', this.shadowRoot, NetworkPasswordInputElement)
        .value = '';
    strictQuery('#unlockPin2', this.shadowRoot, NetworkPasswordInputElement)
        .value = '';
    strictQuery('#unlockPukDialog', this.shadowRoot, CrDialogElement)
        .showModal();
    requestAnimationFrame(() => {
      strictQuery('#unlockPuk', this.shadowRoot, NetworkPasswordInputElement)
          .focus();
    });
  }

  /** @private */
  showUnlockPinDialog_() {
    if (strictQuery('#unlockPinDialog', this.shadowRoot, CrDialogElement)
            .open) {
      return;
    }

    this.error_ = ErrorType.NONE;
    strictQuery('#unlockPin', this.shadowRoot, NetworkPasswordInputElement)
        .value = '';
    strictQuery('#unlockPinDialog', this.shadowRoot, CrDialogElement)
        .showModal();
    requestAnimationFrame(() => {
      strictQuery('#unlockPin', this.shadowRoot, NetworkPasswordInputElement)
          .focus();
    });
  }

  private computeIsSimPinLockRestricted_(): boolean {
    return !!this.globalPolicy && !this.globalPolicy.allowCellularSimLock;
  }

  /**
   * Clears error message on user interaction.
   */
  private pinOrPukChange_(): void {
    this.error_ = ErrorType.NONE;
    this.updateSubmitButtonEnabled_();
  }

  /**
   * Sends the PIN value from the Enter PIN dialog.
   */
  private sendEnterPin_(event: Event) {
    event.stopPropagation();
    if (!this.enterPinEnabled_) {
      return;
    }
    const pin =
        strictQuery('#enterPin', this.shadowRoot, NetworkPasswordInputElement)
            .value;
    if (!this.validatePin_(pin)) {
      return;
    }

    const isPinRequired = !!this.deviceState &&
        !!this.deviceState.simLockStatus &&
        !this.deviceState.simLockStatus.lockEnabled;

    const simState = {
      currentPinOrPuk: pin,
      requirePin: isPinRequired,
      newPin: null,
    };

    this.setCellularSimState_(simState);
  }

  /**
   * Sends the old and new PIN values from the Change PIN dialog.
   */
  private sendChangePin_(event: Event) {
    event.stopPropagation();
    const newPin =
        strictQuery(
            '#changePinNew1', this.shadowRoot, NetworkPasswordInputElement)
            .value;
    if (!this.validatePin_(
            newPin,
            strictQuery(
                '#changePinNew2', this.shadowRoot, NetworkPasswordInputElement)
                .value)) {
      return;
    }
    const simState = {
      currentPinOrPuk:
          strictQuery(
              '#changePinOld', this.shadowRoot, NetworkPasswordInputElement)
              .value,
      newPin: newPin,
      requirePin: true,
    };
    this.setCellularSimState_(simState);
  }

  /**
   * Sends the PUK value and new PIN value from the Unblock PUK dialog.
   */
  private sendUnlockPuk_(event: Event) {
    event.stopPropagation();
    const puk =
        strictQuery('#unlockPuk', this.shadowRoot, NetworkPasswordInputElement)
            .value;
    if (!this.validatePuk_(puk)) {
      return;
    }

    if (this.isSimPinLockRestricted_) {
      this.unlockCellularSim_('', puk);
      return;
    }

    const pin =
        strictQuery('#unlockPin1', this.shadowRoot, NetworkPasswordInputElement)
            .value;
    if (!this.validatePin_(
            pin,
            strictQuery(
                '#unlockPin2', this.shadowRoot, NetworkPasswordInputElement)
                .value)) {
      return;
    }
    this.unlockCellularSim_(pin, puk);
  }

  /**
   * Sends the PIN value from the Unlock PIN dialog.
   */
  private sendUnlockPin_(event: Event) {
    event.stopPropagation();
    const pin =
        strictQuery('#unlockPin', this.shadowRoot, NetworkPasswordInputElement)
            .value;
    if (!this.validatePin_(pin)) {
      return;
    }
    this.unlockCellularSim_(pin);
  }

  private setCellularSimState_(cellularSimState: CellularSimState): void {
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
  }

  /**
   * Closes current dialog and sets the current state of dialogs
   * |skipIsDialogOpenUpdate| is optional because in some cases we do
   * not want to update the current dialog open state
   * @param {?boolean=} skipIsDialogOpenUpdate
   * @private
   */
  closeDialogs_(skipIsDialogOpenUpdate: boolean|undefined = undefined) {
    if (strictQuery('#enterPinDialog', this.shadowRoot, CrDialogElement).open) {
      strictQuery('#enterPinDialog', this.shadowRoot, CrDialogElement).close();
    }
    if (strictQuery('#changePinDialog', this.shadowRoot, CrDialogElement)
            .open) {
      strictQuery('#changePinDialog', this.shadowRoot, CrDialogElement).close();
    }
    if (strictQuery('#unlockPinDialog', this.shadowRoot, CrDialogElement)
            .open) {
      strictQuery('#unlockPinDialog', this.shadowRoot, CrDialogElement).close();
    }
    if (strictQuery('#unlockPukDialog', this.shadowRoot, CrDialogElement)
            .open) {
      strictQuery('#unlockPukDialog', this.shadowRoot, CrDialogElement).close();
    }
    this.isDialogOpen = skipIsDialogOpenUpdate ? skipIsDialogOpenUpdate : false;
  }

  /**
   * Used by test to simulate dialog cancel click.
   */
  closeDialogsForTest(): void {
    this.closeDialogs_();
  }

  private onCancel_(event: Event): void {
    event.stopPropagation();
    this.closeDialogs_();
  }

  private setInProgress_(): void {
    this.error_ = ErrorType.NONE;
    this.pendingError_ = ErrorType.NONE;
    this.inProgress_ = true;
  }

  private updateSubmitButtonEnabled_(): void {
    const hasError = this.error_ !== ErrorType.NONE;
    this.enterPinEnabled_ = !this.inProgress_ && !!this.pin_ && !hasError;
    this.changePinEnabled_ = !this.inProgress_ && !!this.pin_ &&
        !!this.pin_new1_ && !!this.pin_new2_ && !hasError;
    this.enterPukEnabled_ = !this.inProgress_ && !!this.puk_ && !hasError &&
        (this.isSimPinLockRestricted_ ||
         (!!this.pin_new1_ && !!this.pin_new2_));
  }

  private unlockCellularSim_(
      pin: string, opt_puk: string|undefined = undefined): void {
    this.setInProgress_();
    const cellularSimState = {
      currentPinOrPuk: opt_puk || pin,
      requirePin: false,
      newPin: opt_puk ? pin : null,
    };

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
  }

  private focusDialogInput_(): void {
    if (strictQuery('#enterPinDialog', this.shadowRoot, CrDialogElement).open) {
      strictQuery('#enterPin', this.shadowRoot, NetworkPasswordInputElement)
          .focus();
    } else if (strictQuery('#changePinDialog', this.shadowRoot, CrDialogElement)
                   .open) {
      if (this.isSecondNewPinInvalid_()) {
        strictQuery(
            '#changePinNew2', this.shadowRoot, NetworkPasswordInputElement)
            .focus();
      } else {
        strictQuery(
            '#changePinOld', this.shadowRoot, NetworkPasswordInputElement)
            .focus();
      }
    } else if (strictQuery('#unlockPinDialog', this.shadowRoot, CrDialogElement)
                   .open) {
      strictQuery('#unlockPin', this.shadowRoot, NetworkPasswordInputElement)
          .focus();
    } else if (strictQuery('#unlockPukDialog', this.shadowRoot, CrDialogElement)
                   .open) {
      strictQuery('#unlockPuk', this.shadowRoot, NetworkPasswordInputElement)
          .focus();
    }
  }

  /**
   * Checks whether |pin1| is of the proper length and contains only digits.
   * If opt_pin2 is not undefined, then it also checks whether pin1 and
   * opt_pin2 match. On any failure, sets |this.error_|, focuses the invalid
   * PIN, and returns false.
   * @return True if the pins match and are of minimum length.
   */
  private validatePin_(pin1: string, opt_pin2: string|undefined = undefined):
      boolean {
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
  }

  /**
   * Checks whether |puk| is of the proper length and contains only digits.
   * If not, sets |this.error_| and returns false.
   * @return True if the puk is of minimum length.
   */
  private validatePuk_(puk: string): boolean {
    if (puk.length < PUK_MIN_LENGTH || !DIGITS_ONLY_REGEX.test(puk)) {
      this.error_ = ErrorType.INVALID_PUK;
      return false;
    }
    return true;
  }

  private getEnterPinDescription_(): string {
    return this.isSimPinLockRestricted_ ?
        this.i18n('networkSimLockPolicyAdminSubtitle') :
        this.i18n('networkSimEnterPinDescription');
  }

  private getErrorMsg_(): string {
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
  }

  private getNumRetriesLeft_(): number {
    if (!this.deviceState || !this.deviceState.simLockStatus) {
      return 0;
    }

    return this.deviceState.simLockStatus.retriesLeft;
  }

  private computeHasErrorText_(): boolean {
    return !!this.getErrorMsg_();
  }

  private getPinEntrySubtext_(): string {
    const errorMessage = this.getErrorMsg_();
    if (errorMessage) {
      return errorMessage;
    }

    return this.i18n('networkSimEnterPinSubtext');
  }

  private isOldPinInvalid_(): boolean {
    return this.error_ === ErrorType.INCORRECT_PIN ||
        this.error_ === ErrorType.INVALID_PIN;
  }

  private getOldPinErrorMessage_(): string {
    if (this.isOldPinInvalid_()) {
      return this.getErrorMsg_();
    }

    return '';
  }

  private isSecondNewPinInvalid_(): boolean {
    return this.error_ === ErrorType.MISMATCHED_PIN;
  }

  private getSecondNewPinErrorMessage_(): string {
    if (this.isSecondNewPinInvalid_()) {
      return this.getErrorMsg_();
    }

    return '';
  }

  private isPukInvalid_(): boolean {
    return this.error_ === ErrorType.INCORRECT_PUK ||
        this.error_ === ErrorType.INVALID_PUK;
  }

  private getPukErrorMessage_(): string {
    if (this.isPukInvalid_()) {
      return this.getErrorMsg_();
    }

    return '';
  }

  private getPukWarningMessage_(): string {
    return this.isSimPinLockRestricted_ ?
        this.getPukWarningSimPinRestrictedMessage_() :
        this.getPukWarningSimPinUnrestrictedMessage_();
  }

  private getNetworkSimPukDialogString_(): string {
    return this.isSimPinLockRestricted_ ?
        this.i18n('networkSimPukDialogManagedSubtitle') :
        this.i18n('networkSimPukDialogSubtitle');
  }

  private getPukWarningSimPinUnrestrictedMessage_(): string {
    if (this.isPukInvalid_()) {
      const retriesLeft = this.getNumRetriesLeft_();
      if (retriesLeft === 1) {
        return this.i18n('networkSimPukDialogWarningWithFailure', retriesLeft);
      }

      return this.i18n('networkSimPukDialogWarningWithFailures', retriesLeft);
    }

    return this.i18n('networkSimPukDialogWarningNoFailures');
  }

  private getPukWarningSimPinRestrictedMessage_(): string {
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
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [SimLockDialogsElement.is]: SimLockDialogsElement;
  }
}

customElements.define(SimLockDialogsElement.is, SimLockDialogsElement);
