// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview PasswordRequestorMixin is the mixin which bundles the
 * |requestPlaintextPassword| and |requestCredentialDetails| APIs for
 * conveniency. The mixin creates its own |BlockingRequestManager| in chromeos
 * for handling the authentication. Elements implementing this mixin should
 * include a 'settings-password-prompt-dialog' for Chrome OS.
 */

// <if expr="is_chromeos">
import {assert} from 'chrome://resources/js/assert_ts.js';
// </if>
import {dedupingMixin, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

// <if expr="is_chromeos">
import {loadTimeData} from '../i18n_setup.js';

import {BlockingRequestManager} from './blocking_request_manager.js';
// </if>
import {PasswordManagerImpl} from './password_manager_proxy.js';

type Constructor<T> = new (...args: any[]) => T;
export const PasswordRequestorMixin = dedupingMixin(
    <T extends Constructor<PolymerElement>>(superClass: T): T&
    Constructor<PasswordRequestorMixinInterface> => {
      class PasswordRequestorMixin extends superClass {
        // <if expr="is_chromeos">
        static get properties() {
          return {
            tokenRequestManager: Object,
            showPasswordPromptDialog: Boolean,
            tokenObtained: Boolean,
          };
        }

        tokenRequestManager: BlockingRequestManager;
        showPasswordPromptDialog: boolean;
        tokenObtained: boolean;

        override connectedCallback() {
          super.connectedCallback();
          // If the user's account supports the password check, an auth token
          // will be required in order for them to view or export passwords.
          // Otherwise there is no additional security so |tokenRequestManager|
          // will immediately resolve requests.
          if (loadTimeData.getBoolean('userCannotManuallyEnterPassword')) {
            this.tokenRequestManager = new BlockingRequestManager();
          } else {
            this.tokenRequestManager = new BlockingRequestManager(
                () => this.openPasswordPromptDialog_());
          }
        }
        // </if>

        requestPlaintextPassword(
            id: number,
            reason: chrome.passwordsPrivate.PlaintextReason): Promise<string> {
          return PasswordManagerImpl.getInstance().requestPlaintextPassword(
              id, reason);
        }

        requestCredentialDetails(id: number):
            Promise<chrome.passwordsPrivate.PasswordUiEntry> {
          return PasswordManagerImpl.getInstance()
              .requestCredentialsDetails([id])
              .then(passwords => passwords[0]);
        }

        // <if expr="is_chromeos">
        /**
         * When this event fired, it means that the password-prompt-dialog
         * succeeded in creating a fresh token in the quickUnlockPrivate API.
         * Because new tokens can only ever be created immediately following a
         * GAIA password check, the passwordsPrivate API can now safely grant
         * requests for secure data (i.e. saved passwords) for a limited time.
         * This observer resolves the request, triggering a callback that
         * requires a fresh auth token to succeed and that was provided to the
         * BlockingRequestManager by another DOM element seeking secure data.
         *
         * @param e Contains newly created auth token
         *     chrome.quickUnlockPrivate.TokenInfo. Note that its precise value
         * is not relevant here, only the facts that it's created.
         */
        onTokenObtained(e: CustomEvent<chrome.quickUnlockPrivate.TokenInfo>) {
          assert(e.detail);
          this.tokenRequestManager.resolve();
          this.tokenObtained = true;
        }

        onPasswordPromptClose(_event: CloseEvent) {
          this.showPasswordPromptDialog = false;
        }

        private openPasswordPromptDialog_() {
          this.tokenObtained = false;
          this.showPasswordPromptDialog = true;
        }
        // </if>
      }
      return PasswordRequestorMixin;
    });

export interface PasswordRequestorMixinInterface {
  requestPlaintextPassword(
      id: number,
      reason: chrome.passwordsPrivate.PlaintextReason): Promise<string>;
  requestCredentialDetails(id: number):
      Promise<chrome.passwordsPrivate.PasswordUiEntry>;
  // <if expr="is_chromeos">
  onTokenObtained(e: CustomEvent<chrome.quickUnlockPrivate.TokenInfo>): void;
  onPasswordPromptClose(event: CloseEvent): void;
  tokenRequestManager: BlockingRequestManager;
  showPasswordPromptDialog: boolean;
  tokenObtained: boolean;
  // </if>
}
