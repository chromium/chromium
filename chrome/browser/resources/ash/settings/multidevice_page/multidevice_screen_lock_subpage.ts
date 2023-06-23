// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * Subpage of settings-multidevice-notification-access-setup-dialog for setting
 * up screen lock.
 */

import 'chrome://resources/cr_elements/cr_shared_vars.css.js';
import '../os_people_page/lock_screen_password_prompt_dialog.js';
import '../os_people_page/setup_pin_dialog.js';

import {assert} from 'chrome://resources/js/assert_ts.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {LockScreenUnlockType, LockStateMixin} from '../lock_state_mixin.js';

import {getTemplate} from './multidevice_screen_lock_subpage.html.js';

import TokenInfo = chrome.quickUnlockPrivate.TokenInfo;
import QuickUnlockMode = chrome.quickUnlockPrivate.QuickUnlockMode;

const SettingsMultideviceScreenLockSubpageElementBase =
    LockStateMixin(PolymerElement);

class SettingsMultideviceScreenLockSubpageElement extends
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
       * setModes_ is a partially applied function of
       * {@link chrome.quickUnlockPrivate.setModes} that stores the current auth
       * token. It's defined only when the user has entered a valid password.
       */
      setModes_: {
        type: Object,
      },

      /**
       * Authentication token.
       */
      authToken_: {
        type: Object,
        observer: 'onAuthTokenChanged_',
      },

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
    };
  }

  isPasswordDialogShowing: boolean;
  isScreenLockEnabled: boolean;
  showSetupPinDialog: boolean;
  private authToken_: TokenInfo|undefined;
  private quickUnlockDisabledByPolicy_: boolean;
  private setModes_:
      ((modes: QuickUnlockMode[], credentials: string[],
        onComplete: (success: boolean) => void) => void)|undefined;
  private shouldPromptPasswordDialog_: boolean;


  static get observers() {
    return ['selectedUnlockTypeChanged_(selectedUnlockType)'];
  }

  constructor() {
    super();

    if (this.authToken_ === undefined) {
      this.shouldPromptPasswordDialog_ = true;
    }
  }

  /**
   * Called when the unlock type has changed.
   * @param selected The current unlock type.
   */
  private selectedUnlockTypeChanged_(selected: string): void {
    const pinNumberEvent = new CustomEvent('pin-number-selected', {
      bubbles: true,
      composed: true,
      detail: {
        isPinNumberSelected: (selected === LockScreenUnlockType.PIN_PASSWORD),
      },
    });
    this.dispatchEvent(pinNumberEvent);
    if (selected === LockScreenUnlockType.PASSWORD && this.setModes_) {
      // If the user selects PASSWORD only (which sends an asynchronous
      // setModes_.call() to clear the quick unlock capability), indicate to the
      // user immediately that the quick unlock capability is cleared by setting
      // |hasPin| to false. If there is an error clearing quick unlock, revert
      // |hasPin| to true. This prevents setupPinButton UI delays, except in the
      // small chance that CrOS fails to remove the quick unlock capability. See
      // https://crbug.com/1054327 for details.
      this.hasPin = false;
      this.setModes_.call(null, [], [], (result: boolean) => {
        // Revert |hasPin| to true in the event setModes_ fails to set lock
        // state to PASSWORD only.
        if (!result) {
          this.hasPin = true;
        }

        assert(result, 'Failed to clear quick unlock modes');
      });
    }
  }

  private onAuthTokenChanged_(): void {
    if (this.authToken_ === undefined) {
      this.setModes_ = undefined;
    } else {
      const token = this.authToken_.token;
      this.setModes_ = (modes, credentials, onComplete) => {
        this.quickUnlockPrivate.setModes(token, modes, credentials, () => {
          let result = true;
          if (chrome.runtime.lastError) {
            console.error(
                `setModes failed: ${chrome.runtime.lastError.message}`);
            result = false;
          }
          onComplete(result);
        });
      };
    }
  }

  private onPasswordPromptDialogClose_(): void {
    this.shouldPromptPasswordDialog_ = false;
  }

  private onAuthTokenObtained_(e: CustomEvent<TokenInfo>): void {
    this.authToken_ = e.detail;
    this.setLockScreenEnabled(
        this.authToken_.token, true, (_success: boolean) => {});
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
