// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Polymer element for displaying and modifying cellular sim info.
 */

import '//resources/ash/common/cr_elements/cr_toggle/cr_toggle.js';
import '//resources/ash/common/cr_elements/icons.html.js';
import '//resources/ash/common/cr_elements/cr_button/cr_button.js';
import '//resources/polymer/v3_0/iron-flex-layout/iron-flex-layout-classes.js';
import '//resources/polymer/v3_0/iron-icon/iron-icon.js';
import '//resources/polymer/v3_0/paper-tooltip/paper-tooltip.js';
import './network_password_input.js';
import './network_shared.css.js';
import './sim_lock_dialogs.js';

import {assert} from '//resources/js/assert.js';
import {GlobalPolicy} from '//resources/mojo/chromeos/services/network_config/public/mojom/cros_network_config.mojom-webui.js';
import {PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {CrButtonElement} from 'chrome://resources/ash/common/cr_elements/cr_button/cr_button.js';
import {CrToggleElement} from 'chrome://resources/ash/common/cr_elements/cr_toggle/cr_toggle.js';
import {I18nMixin} from 'chrome://resources/ash/common/cr_elements/i18n_mixin.js';

import {isActiveSim} from './cellular_utils.js';
import {getTemplate} from './network_siminfo.html.js';
import {OncMojo} from './onc_mojo.js';

const TOGGLE_DEBOUNCE_MS = 500;

/**
 * State of the element. <network-siminfo> shows 1 of 2 modes:
 *   SIM_LOCKED: Shows an alert that the SIM is locked and provides an "Unlock"
 *       button which allows the user to unlock the SIM.
 *   SIM_UNLOCKED: Provides an option to lock the SIM if desired. If SIM-lock is
 *       on, this UI also allows the user to update the PIN used.
 */
enum State {
  SIM_LOCKED = 0,
  SIM_UNLOCKED = 1,
}

const NetworkSiminfoElementBase = I18nMixin(PolymerElement);

export class NetworkSiminfoElement extends NetworkSiminfoElementBase {
  static get is() {
    return 'network-siminfo' as const;
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

      networkState: {
        type: Object,
        value: null,
      },

      globalPolicy: Object,

      disabled: {
        type: Boolean,
        value: false,
      },

      /** Used to reference the State enum in HTML. */
      State: {
        type: Object,
        value: State,
      },

      /**
       * Reflects deviceState.simLockStatus.lockEnabled for the
       * toggle button.
       */
      lockEnabled_: {
        type: Boolean,
        value: false,
      },

      isDialogOpen_: {
        type: Boolean,
        value: false,
        observer: 'onDialogOpenChanged_',
      },

      /**
       * If set to true, shows the Change PIN dialog if the device is unlocked.
       */
      showChangePin_: {
        type: Boolean,
        value: false,
      },

      /**
       * Indicates that the current network is on the active sim slot.
       */
      isActiveSim_: {
        type: Boolean,
        value: false,
        computed: 'computeIsActiveSim_(networkState, deviceState)',
      },

      state_: {
        type: Number,
        value: State.SIM_UNLOCKED,
        computed: 'computeState_(networkState, deviceState, deviceState.*,' +
            'isActiveSim_)',
      },

      isSimPinLockRestricted_: {
        type: Boolean,
        value: false,
        computed: 'computeIsSimPinLockRestricted_(globalPolicy,' +
            'globalPolicy.*, lockEnabled_)',
      },

    };
  }

  deviceState: OncMojo.DeviceStateProperties;
  networkState: OncMojo.NetworkStateProperties;
  globalPolicy: GlobalPolicy|undefined;
  disabled: boolean;
  private lockEnabled_: boolean;
  private isDialogOpen_: boolean;
  private showChangePin_: boolean;
  private isActiveSim_: boolean;
  private state_: State;
  private isSimPinLockRestricted_: boolean;
  private setLockEnabled_: boolean|undefined;

  getSimLockToggle(): CrToggleElement {
    const el =
        this.shadowRoot!.querySelector<CrToggleElement>('#simLockButton');
    assert(!!el);
    return el;
  }

  getUnlockButton(): CrButtonElement {
    const el =
        this.shadowRoot!.querySelector<CrButtonElement>('#unlockPinButton');
    assert(!!el);
    return el;
  }

  private onDialogOpenChanged_(): void {
    if (this.isDialogOpen_) {
      return;
    }

    this.delayUpdateLockEnabled_();
    this.updateFocus_();
  }

  /**
   * Sets default focus when dialog is closed.
   */
  private updateFocus_(): void {
    const state = this.computeState_();
    switch (state) {
      case State.SIM_LOCKED:
        const unlockPinButton =
            this.shadowRoot!.querySelector<CrButtonElement>('#unlockPinButton');
        if (unlockPinButton) {
          unlockPinButton.focus();
        }
        break;
      case State.SIM_UNLOCKED:
        const simLockButton =
            this.shadowRoot!.querySelector<CrToggleElement>('#simLockButton');
        if (simLockButton) {
          simLockButton.focus();
        }
        break;
    }
  }

  private deviceStateChanged_(): void {
    if (!this.deviceState) {
      return;
    }
    const simLockStatus = this.deviceState.simLockStatus;
    if (!simLockStatus) {
      return;
    }

    const lockEnabled = this.isActiveSim_ && simLockStatus.lockEnabled;
    if (lockEnabled !== this.lockEnabled_) {
      this.setLockEnabled_ = lockEnabled;
      this.updateLockEnabled_();
    } else {
      this.setLockEnabled_ = undefined;
    }
  }

  /**
   * Wrapper method to prevent changing |lockEnabled_| while a dialog is open
   * to avoid confusion while a SIM operation is in progress. This must be
   * called after closing any dialog (and not opening another) to set the
   * correct state.
   */
  private updateLockEnabled_(): void {
    if (this.setLockEnabled_ === undefined || this.isDialogOpen_) {
      return;
    }
    this.lockEnabled_ = this.setLockEnabled_;
    this.setLockEnabled_ = undefined;
  }

  private delayUpdateLockEnabled_(): void {
    setTimeout(() => {
      this.updateLockEnabled_();
    }, TOGGLE_DEBOUNCE_MS);
  }

  /**
   * Opens the pin dialog when the sim lock enabled state changes.
   */
  private onSimLockEnabledChange_(_event: Event): void {
    if (!this.deviceState) {
      return;
    }
    // Do not change the toggle state after toggle is clicked. The toggle
    // should only be updated when the device state changes or dialog has been
    // closed. Changing the UI toggle before the device state changes or dialog
    // is closed can be confusing to the user, as it indicates the action was
    // successful.
    this.lockEnabled_ = !this.lockEnabled_;
    this.showSimLockDialog_(/*showChangePin=*/ false);
  }

  /**
   * Opens the Change PIN dialog.
   */
  private onChangePinPressed_(event: Event) {
    event.stopPropagation();
    if (!this.deviceState) {
      return;
    }
    this.showSimLockDialog_(true);
  }

  /**
   * Opens the Unlock PIN / PUK dialog.
   */
  private onUnlockPinPressed_(event: Event) {
    event.stopPropagation();
    this.showSimLockDialog_(true);
  }

  private showSimLockDialog_(showChangePin: boolean) {
    this.showChangePin_ = showChangePin;
    this.isDialogOpen_ = true;
  }

  private computeIsActiveSim_(): boolean {
    return isActiveSim(this.networkState, this.deviceState);
  }

  private showChangePinButton_(): boolean {
    if (this.isSimPinLockRestricted_) {
      return false;
    }

    if (!this.deviceState || !this.deviceState.simLockStatus) {
      return false;
    }

    return this.deviceState.simLockStatus.lockEnabled && this.isActiveSim_;
  }

  private isSimLockButtonDisabled_(): boolean {
    // If SIM PIN locking is restricted by admin, and the SIM does not have SIM
    // PIN lock enabled, users should not be able to enable PIN locking.
    if (this.isSimPinLockRestricted_ && !this.lockEnabled_) {
      return true;
    }

    return this.disabled || !this.isActiveSim_;
  }

  private computeState_(): State {
    const simLockStatus = this.deviceState && this.deviceState.simLockStatus;

    // If a lock is set and the network in question is the active SIM, show the
    // "locked SIM" UI. Note that we can only detect the locked state of the
    // active SIM, so it is possible that we fall through to the SIM_UNLOCKED
    // case below even for a locked SIM if that SIM is not the active one.
    if (this.isActiveSim_ && simLockStatus && !!simLockStatus.lockType) {
      return State.SIM_LOCKED;
    }

    // Note that if this is not the active SIM, we cannot read to lock state, so
    // we default to showing the "unlocked" UI unless we know otherwise.
    return State.SIM_UNLOCKED;
  }

  private isSimCarrierLocked_(): boolean {
    const simLockStatus = this.deviceState && this.deviceState.simLockStatus;

    if (this.isActiveSim_ && simLockStatus &&
        simLockStatus.lockType === 'network-pin') {
      return true;
    }

    return false;
  }

  private shouldShowPolicyIndicator_(): boolean {
    return this.isSimPinLockRestricted_ && this.isActiveSim_;
  }

  private computeIsSimPinLockRestricted_(): boolean {
    return !!this.globalPolicy && !this.globalPolicy.allowCellularSimLock;
  }

  private eq_(state1: State, state2: State): boolean {
    return state1 === state2;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [NetworkSiminfoElement.is]: NetworkSiminfoElement;
  }
}

customElements.define(NetworkSiminfoElement.is, NetworkSiminfoElement);
