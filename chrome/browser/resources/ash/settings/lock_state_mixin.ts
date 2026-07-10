// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * Contains utilities that help identify the current way that the lock screen
 * will be displayed.
 */


import type {I18nMixinInterface} from 'chrome://resources/ash/common/cr_elements/i18n_mixin.js';
import {I18nMixin} from 'chrome://resources/ash/common/cr_elements/i18n_mixin.js';
import type {WebUiListenerMixinInterface} from 'chrome://resources/ash/common/cr_elements/web_ui_listener_mixin.js';
import {WebUiListenerMixin} from 'chrome://resources/ash/common/cr_elements/web_ui_listener_mixin.js';
import {assert} from 'chrome://resources/js/assert.js';
import type {AuthFactorConfigInterface, PinFactorEditorInterface, RecoveryFactorEditorInterface} from 'chrome://resources/mojo/chromeos/ash/services/auth_factor_config/public/mojom/auth_factor_config.mojom-webui.js';
import {AuthFactorConfig, PinFactorEditor, RecoveryFactorEditor} from 'chrome://resources/mojo/chromeos/ash/services/auth_factor_config/public/mojom/auth_factor_config.mojom-webui.js';
import type {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {dedupingMixin} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import type {Constructor, PrefsState} from './common/types.js';
import type {FingerprintBrowserProxy} from './os_people_page/fingerprint_browser_proxy.js';
import {FingerprintBrowserProxyImpl} from './os_people_page/fingerprint_browser_proxy.js';
import type {QuickUnlockBrowserProxy} from './os_people_page/quick_unlock_browser_proxy.js';
import {QuickUnlockBrowserProxyImpl} from './os_people_page/quick_unlock_browser_proxy.js';

export enum LockScreenUnlockType {
  NO_AUTH = 'no_auth',
  LOCK_SCREEN_NONE = 'lock_screen_none',
  PASSWORD = 'password',
  PIN_ONLY = 'pin_only',
  PASSWORD_PIN = 'password+pin',
  PASSWORD_FINGERPRINT = 'password+fingerprint',
  PIN_FINGERPRINT = 'pin+fingerprint',
  PASSWORD_PIN_FINGERPRINT = 'password+pin+fingerprint',
}

// Use a Record to enforce that every LockScreenUnlockType has a corresponding
// i18n string ID.
const LOCK_SCREEN_UNLOCK_TYPE_LABELS: Record<LockScreenUnlockType, string> = {
  [LockScreenUnlockType.LOCK_SCREEN_NONE]: 'lockScreenNone',
  [LockScreenUnlockType.NO_AUTH]: 'lockScreenNoAuthFactor',
  [LockScreenUnlockType.PASSWORD]: 'lockScreenPasswordOnly',
  [LockScreenUnlockType.PIN_ONLY]: 'lockScreenPinOnly',
  [LockScreenUnlockType.PASSWORD_PIN]: 'lockScreenPinOrPassword',
  [LockScreenUnlockType.PASSWORD_FINGERPRINT]:
      'lockScreenPasswordOrFingerprint',
  [LockScreenUnlockType.PIN_FINGERPRINT]: 'lockScreenPinOrFingerprint',
  [LockScreenUnlockType.PASSWORD_PIN_FINGERPRINT]:
      'lockScreenPasswordOrPinOrFingerprint',
};

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

  initializeLockState(): void;

  determineUnlockType(
      hasPassword: boolean, hasPin: boolean, hasFingerprint: boolean): void;
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
              value: LockScreenUnlockType.NO_AUTH,
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

            /**
             * Preferences state.
             * This is expected to be provided by the element consuming the
             * mixin.
             * @see {OsSettingsPrivacyPageElement} for example.
             */
            prefs: {
              type: Object,
            },

            /**
             * The translated string for the current unlock type.
             */
            unlockStatusLabel_: {
              type: String,
              notify: true,
            },
          };
        }

        static get observers() {
          return ['onSelectedUnlockTypeChanged_(selectedUnlockType)'];
        }

        selectedUnlockType: LockScreenUnlockType;
        hasPinLogin: boolean|undefined;
        quickUnlockPrivate: typeof chrome.quickUnlockPrivate;
        authFactorConfig: AuthFactorConfigInterface;
        recoveryFactorEditor: RecoveryFactorEditorInterface;
        pinFactorEditor: PinFactorEditorInterface;
        prefs: PrefsState;
        private unlockStatusLabel_: string;
        private quickUnlockBrowserProxy_: QuickUnlockBrowserProxy;
        private fingerprintBrowserProxy_: FingerprintBrowserProxy;

        constructor() {
          super();
          this.quickUnlockBrowserProxy_ =
              QuickUnlockBrowserProxyImpl.getInstance();
          this.fingerprintBrowserProxy_ =
              FingerprintBrowserProxyImpl.getInstance();
        }

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

        initializeLockState(): Promise<void> {
          this.addWebUiListener(
              'settings.enable_screen_lock.changed', (isEnabled: boolean) => {
                // When the screen lock setting changes, if it's disabled,
                // we immediately set the unlock type to NONE. Otherwise,
                // we re-evaluate the configured factors to determine the
                // correct unlock type.
                if (!isEnabled) {
                  this.selectedUnlockType =
                      LockScreenUnlockType.LOCK_SCREEN_NONE;
                } else {
                  this.updateUnlockType_();
                }
              });

          return this.updateUnlockType_();
        }

        private updateUnlockType_(): Promise<void> {
          return Promise
              .all([
                this.fingerprintBrowserProxy_.getNumFingerprints(),
                this.quickUnlockBrowserProxy_.requestActiveAuthFactors(),
              ])
              .then(([numFingerprints, factors]) => {
                this.determineUnlockType(
                    factors.password, factors.pin, numFingerprints > 0);
              });
        }

        determineUnlockType(
            hasPassword: boolean, hasPin: boolean,
            hasFingerprint: boolean): void {
          // Guard 1: Settings disabled
          if (this.prefs && !this.prefs['settings'].enable_screen_lock.value) {
            this.selectedUnlockType = LockScreenUnlockType.LOCK_SCREEN_NONE;
            return;
          }

          // Guard 2: No credentials
          if (!hasPassword && !hasPin) {
            this.selectedUnlockType = LockScreenUnlockType.NO_AUTH;
            return;
          }

          // Logic 3: Handle Password combinations
          if (hasPassword) {
            if (hasPin) {
              this.selectedUnlockType = hasFingerprint ?
                  LockScreenUnlockType.PASSWORD_PIN_FINGERPRINT :
                  LockScreenUnlockType.PASSWORD_PIN;
            } else {
              this.selectedUnlockType = hasFingerprint ?
                  LockScreenUnlockType.PASSWORD_FINGERPRINT :
                  LockScreenUnlockType.PASSWORD;
            }
            return;
          }

          // Logic 4: Handle Pin combinations (We know !hasPassword here)
          this.selectedUnlockType = hasFingerprint ?
              LockScreenUnlockType.PIN_FINGERPRINT :
              LockScreenUnlockType.PIN_ONLY;
        }

        private onSelectedUnlockTypeChanged_(): void {
          const labelId =
              LOCK_SCREEN_UNLOCK_TYPE_LABELS[this.selectedUnlockType];
          assert(labelId !== undefined, 'Unknown LockScreenUnlockType');
          this.unlockStatusLabel_ = this.i18n(labelId);
        }

        private handlePinLoginAvailableChanged_(isAvailable: boolean): void {
          this.hasPinLogin = isAvailable;
          cachedHasPinLogin = this.hasPinLogin;
        }
      }
      return LockStateMixinInternal;
    });
