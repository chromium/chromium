// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * Subpage of settings-multidevice-notification-access-setup-dialog for setting
 * up screen lock.
 */

import 'chrome://resources/ash/common/cr_elements/cr_shared_vars.css.js';
import '../os_people_page/lock_screen_password_prompt_dialog.js';
import '../os_people_page/setup_pin_dialog.js';

import {fireAuthTokenInvalidEvent} from 'chrome://resources/ash/common/quick_unlock/utils.js';
import {assert} from 'chrome://resources/js/assert.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {AuthFactor, AuthFactorConfig, ConfigureResult, FactorObserverReceiver, PinFactorEditor} from 'chrome://resources/mojo/chromeos/ash/services/auth_factor_config/public/mojom/auth_factor_config.mojom-webui.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {LockScreenUnlockType, LockStateMixin} from '../lock_state_mixin.js';

import {getTemplate} from './multidevice_screen_lock_subpage.html.js';

import TokenInfo = chrome.quickUnlockPrivate.TokenInfo;

const SettingsMultideviceScreenLockSubpageElementBase =
    LockStateMixin(PolymerElement);

export class SettingsMultideviceScreenLockSubpageElement extends
    SettingsMultideviceScreenLockSubpageElementBase {
  static get is() {
    return 'settings-multidevice-screen-lock-subpage' as const;
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      /**
       * Authentication token.
       */
      authTokenInfo_: Object,

      /**
       * True if quick unlock settings are disabled by policy.
       */
      quickUnlockDisabledByPolicy_: {
        type: Boolean,
        value() {
          return loadTimeData.getBoolean('quickUnlockDisabledByPolicy');
        },
        readOnly: true,
      },

      shouldPromptPasswordDialog_: Boolean,

      /** Reflects whether the screen lock is enabled. */
      isScreenLockEnabled: {
        type: Boolean,
        value: false,
        notify: true,
      },

      /** Reflects the password sub-dialog property. */
      isPasswordDialogShowing: {
        type: Boolean,
        value: false,
        notify: true,
      },

      /** Reflects whether the pin dialog should show. */
      showSetupPinDialog: {
        type: Boolean,
        value: false,
        notify: true,
      },

      hasPin: {
        type: Boolean,
        value: false,
      },
    };
  }

  isPasswordDialogShowing: boolean;
  isScreenLockEnabled: boolean;
  showSetupPinDialog: boolean;
  private authTokenInfo_: TokenInfo|undefined;
  private quickUnlockDisabledByPolicy_: boolean;
  private shouldPromptPasswordDialog_: boolean;
  hasPin: boolean;

  static get observers() {
    return [
      'selectedUnlockTypeChanged_(selectedUnlockType)',
      'updatePinState_(authTokenInfo_)',
    ];
  }

  constructor() {
    super();

    if (this.authTokenInfo_ === undefined) {
      this.shouldPromptPasswordDialog_ = true;
    }
  }

  override ready(): void {
    super.ready();

    // Register this object as listener to factor change events (via
    // |onFactorChanged|):
    const receiver = new FactorObserverReceiver(this);
    const remote = receiver.$.bindNewPipeAndPassRemote();
    AuthFactorConfig.getRemote().observeFactorChanges(remote);
  }

  async onFactorChanged(factor: AuthFactor): Promise<void> {
    if (factor !== AuthFactor.kPin) {
      return;
    }
    if (!this.authTokenInfo_) {
      return;
    }

    await this.updatePinState_(this.authTokenInfo_, /*factorChanged=*/ true);
  }

  /**
   * Fetches the state of the PIN factor and updates the corresponding
   * property.
   * @param authTokenInfo Must be equal to `this.authTokenInfo_`. This is
   *     passed as parameter so that this function can be used as callback for
   *     changes of the `authTokenInfo_` property.
   * @param factorChanged Should be `true` if this function is called in
   *     response to a PIN change (as opposed to e.g. during initialization).
   */
  private async updatePinState_(
      authTokenInfo: TokenInfo, factorChanged: boolean = false): Promise<void> {
    if (!authTokenInfo) {
      return;
    }
    const authToken = authTokenInfo.token;
    assert(this.authTokenInfo_ && this.authTokenInfo_.token === authToken);

    const {configured} = await AuthFactorConfig.getRemote().isConfigured(
        authToken, AuthFactor.kPin);
    if (configured) {
      this.hasPin = true;
      this.selectedUnlockType = LockScreenUnlockType.PIN_PASSWORD;
      return;
    }
    assert(!configured);

    // A race condition can occur:
    // (1) User selects PIN_PASSSWORD, and successfully sets a pin, adding
    //     QuickUnlockMode.PIN to active modes.
    // (2) User selects PASSWORD, QuickUnlockMode.PIN capability is cleared
    //     from the active modes. This notifies this class via
    //     |onFactorChanged| and prompts us to fetch the current state of the
    //     PIN asynchronously.
    // (3) User selects PIN_PASSWORD, but the process from step 2 has not yet
    //     completed.
    // In this case, do not forcibly select the PASSWORD radio button even
    // though the unlock type is still PASSWORD (|hasPin| is false). If the
    // user wishes to set a pin, they will have to click the set pin button.
    // See https://crbug.com/1054327 for details.
    if (factorChanged && !this.hasPin &&
        this.selectedUnlockType === LockScreenUnlockType.PIN_PASSWORD) {
      return;
    }
    this.hasPin = false;
    this.selectedUnlockType = LockScreenUnlockType.PASSWORD;
  }

  /**
   * Called when the unlock type has changed.
   * @param selected The current unlock type.
   */
  private async selectedUnlockTypeChanged_(selected: string): Promise<void> {
    const pinNumberEvent = new CustomEvent('pin-number-selected', {
      bubbles: true,
      composed: true,
      detail: {
        isPinNumberSelected: (selected === LockScreenUnlockType.PIN_PASSWORD),
      },
    });
    this.dispatchEvent(pinNumberEvent);
    if (selected === LockScreenUnlockType.PASSWORD && this.authTokenInfo_) {
      // If the user selects PASSWORD only (which sends an asynchronous
      // removePin call to clear the quick unlock capability), indicate to the
      // user immediately that the quick unlock capability is cleared by setting
      // |hasPin| to false. If there is an error clearing quick unlock, revert
      // |hasPin| to true. This prevents setupPinButton UI delays, except in the
      // small chance that CrOS fails to remove the quick unlock capability. See
      // https://crbug.com/1054327 for details.
      this.hasPin = false;
      const {result} = await PinFactorEditor.getRemote().removePin(
          this.authTokenInfo_.token);
      if (result !== ConfigureResult.kSuccess) {
        this.hasPin = true;
      }

      switch (result) {
        case ConfigureResult.kSuccess:
          break;
        case ConfigureResult.kInvalidTokenError:
          fireAuthTokenInvalidEvent(this);
          break;
        case ConfigureResult.kFatalError:
          console.error('Error removing PIN');
          break;
      }
    }
  }

  private onPasswordPromptDialogClose_(): void {
    this.shouldPromptPasswordDialog_ = false;
  }

  private onAuthTokenObtained_(e: CustomEvent<TokenInfo>): void {
    this.authTokenInfo_ = e.detail;
    this.setLockScreenEnabled(
        this.authTokenInfo_.token, true, (_success: boolean) => {});
    this.isScreenLockEnabled = true;
    // Avoid dialog.close() of password_prompt_dialog.ts to close main dialog
    this.isPasswordDialogShowing = true;
  }

  /**
   * Returns true if the setup pin section should be shown.
   * @param selectedUnlockType The current unlock type. Used to let
   *     Polymer know about the dependency.
   */
  private showConfigurePinButton_(selectedUnlockType: string): boolean {
    return selectedUnlockType === LockScreenUnlockType.PIN_PASSWORD;
  }

  private onSetupPinDialogClose_(): void {
    this.showSetupPinDialog = false;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [SettingsMultideviceScreenLockSubpageElement.is]:
        SettingsMultideviceScreenLockSubpageElement;
  }
}

customElements.define(
    SettingsMultideviceScreenLockSubpageElement.is,
    SettingsMultideviceScreenLockSubpageElement);
