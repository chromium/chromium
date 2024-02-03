// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * Contains utilities that help identify the current way that the lock screen
 * will be displayed.
 */

import {I18nMixin, I18nMixinInterface} from 'chrome://resources/ash/common/cr_elements/i18n_mixin.js';
import {WebUiListenerMixin, WebUiListenerMixinInterface} from 'chrome://resources/ash/common/cr_elements/web_ui_listener_mixin.js';
import {AuthFactorConfig, AuthFactorConfigInterface, PinFactorEditor, PinFactorEditorInterface, RecoveryFactorEditor, RecoveryFactorEditorInterface} from 'chrome://resources/mojo/chromeos/ash/services/auth_factor_config/public/mojom/auth_factor_config.mojom-webui.js';
import {dedupingMixin, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {Constructor} from './common/types.js';

export enum LockScreenUnlockType {
  VALUE_PENDING = 'value_pending',
  PASSWORD = 'password',
  PIN_PASSWORD = 'pin+password',
}

/**
 * Determining if the device supports PIN sign-in takes time, as it may require
 * a cryptohome call. This means incorrect strings may be shown for a brief
 * period, and updating them causes UI flicker.
 *
 * Cache the value since the behavior is instantiated multiple times. Caching
 * is safe because PIN login support depends only on hardware capabilities. The
 * value does not change after discovered.
 */
let cachedHasPinLogin: boolean|undefined = undefined;

export interface LockStateMixinInterface extends I18nMixinInterface,
                                                 WebUiListenerMixinInterface {
  selectedUnlockType: LockScreenUnlockType;
  hasPinLogin: boolean|undefined;
  quickUnlockPrivate: typeof chrome.quickUnlockPrivate;
  authFactorConfig: AuthFactorConfigInterface;
  recoveryFactorEditor: RecoveryFactorEditorInterface;
  pinFactorEditor: PinFactorEditorInterface;

  /**
   * @param authToken The token returned by quickUnlockPrivate.getAuthToken
   * @see quickUnlockPrivate.setLockScreenEnabled
   */
  setLockScreenEnabled(
      authToken: string, enabled: boolean,
      onComplete: (result: boolean) => void): void;
}

export const LockStateMixin = dedupingMixin(
    <T extends Constructor<PolymerElement>>(superClass: T): T&
    Constructor<LockStateMixinInterface> => {
      const superClassBase = WebUiListenerMixin(I18nMixin(superClass));

      class LockStateMixinInternal extends superClassBase implements
          LockStateMixinInterface {
        static get properties() {
          return {
            selectedUnlockType: {
              type: String,
              notify: true,
              value: LockScreenUnlockType.VALUE_PENDING,
            },

            /**
             * True if the PIN backend supports signin. undefined iff the value
             * is still resolving.
             */
            hasPinLogin: {type: Boolean, notify: true},

            /**
             * Interface for chrome.quickUnlockPrivate calls. May be overridden
             * by tests.
             */
            quickUnlockPrivate:
                {type: Object, value: chrome.quickUnlockPrivate},

            /**
             * Interface for calls to the ash AuthFactorConfig service. May be
             * overridden by tests.
             */
            authFactorConfig:
                {type: Object, value: AuthFactorConfig.getRemote()},

            /**
             * Interface for calls to the ash RecoveryFactorEditor service.  May
             * be overridden by tests.
             */
            recoveryFactorEditor:
                {type: Object, value: RecoveryFactorEditor.getRemote()},

            /**
             * Interface for calls to the ash PinFactorEditor service.  May be
             * overridden by tests.
             */
            pinFactorEditor: {type: Object, value: PinFactorEditor.getRemote()},
          };
        }

        selectedUnlockType: LockScreenUnlockType;
        hasPinLogin: boolean|undefined;
        quickUnlockPrivate: typeof chrome.quickUnlockPrivate;
        authFactorConfig: AuthFactorConfigInterface;
        recoveryFactorEditor: RecoveryFactorEditorInterface;
        pinFactorEditor: PinFactorEditorInterface;

        override connectedCallback(): void {
          super.connectedCallback();

          // See comment on |cachedHasPinLogin| declaration.
          if (cachedHasPinLogin === undefined) {
            this.addWebUiListener(
                'pin-login-available-changed',
                this.handlePinLoginAvailableChanged_.bind(this));
            chrome.send('RequestPinLoginState');
          } else {
            this.hasPinLogin = cachedHasPinLogin;
          }
        }

        /**
         * Sets the lock screen enabled state.
         * @see quickUnlockPrivate.setLockScreenEnabled
         */
        setLockScreenEnabled(
            authToken: string, enabled: boolean,
            onComplete: (result: boolean) => void): void {
          this.quickUnlockPrivate.setLockScreenEnabled(
              authToken, enabled, () => {
                let success = true;
                if (chrome.runtime.lastError) {
                  console.warn(
                      'setLockScreenEnabled failed: ' +
                      chrome.runtime.lastError.message);
                  success = false;
                }
                if (onComplete) {
                  onComplete(success);
                }
              });
        }

        private handlePinLoginAvailableChanged_(isAvailable: boolean): void {
          this.hasPinLogin = isAvailable;
          cachedHasPinLogin = this.hasPinLogin;
        }
      }
      return LockStateMixinInternal;
    });
