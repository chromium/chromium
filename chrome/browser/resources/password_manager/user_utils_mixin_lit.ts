// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {WebUiListenerMixinLit} from 'chrome://resources/cr_elements/web_ui_listener_mixin_lit.js';
import {assert} from 'chrome://resources/js/assert.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import type {CrLitElement, PropertyValues} from 'chrome://resources/lit/v3_0/lit.rollup.js';
import {dedupingMixin} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {PasswordManagerActionableError} from './password_manager.mojom-webui.js';
import {PasswordManagerImpl, toMojoActionableError} from './password_manager_proxy.js';
import type {AccountInfo, SyncInfo} from './sync_browser_proxy.js';
import {SyncBrowserProxyImpl} from './sync_browser_proxy.js';

type Constructor<T> = new (...args: any[]) => T;

const WebUiListenerMixinLitBase =
    <T extends Constructor<CrLitElement>>(superClass: T) =>
        WebUiListenerMixinLit(superClass);

/**
 * This mixin bundles functionality related to syncing, sign in status and
 * account storage.
 */
export const UserUtilMixinLit = dedupingMixin(
    <T extends Constructor<CrLitElement>>(superClass: T): T&
    Constructor<UserUtilMixinLitInterface> => {
      class UserUtilMixinLit extends WebUiListenerMixinLitBase
      (superClass) implements UserUtilMixinLitInterface {
        static get properties() {
          return {
            /**
             * If true, the edit dialog and removal notification show
             * information about which location(s) a password is stored.
             */
            isAccountStoreUser: {type: Boolean},

            isSyncingPasswords: {type: Boolean},

            actionableError: {type: Number},

            /* Email of the primary account. */
            accountEmail: {type: String},

            /* Avatar image of the primary account. */
            avatarImage: {type: String},

            syncInfo_: {type: Object},

            accountInfo_: {type: Object},
          };
        }

        accessor isAccountStoreUser: boolean = false;
        accessor isSyncingPasswords: boolean = true;
        accessor actionableError: PasswordManagerActionableError|null = null;
        accessor accountEmail: string = '';
        accessor avatarImage: string = '';
        private accessor syncInfo_: SyncInfo|null = null;
        private accessor accountInfo_: AccountInfo|null = null;

        private setIsAccountStorageActiveListener_: ((active: boolean) => void)|
            null = null;
        private setPasswordManagerActionableErrorListener_:
            ((error: chrome.passwordsPrivate.PasswordManagerActionableError) =>
                 void)|null = null;

        override connectedCallback() {
          super.connectedCallback();

          // Create listener functions.
          this.setIsAccountStorageActiveListener_ = (active) =>
              this.isAccountStoreUser = active;
          const syncInfoChanged = (syncInfo: SyncInfo) => this.syncInfo_ =
              syncInfo;
          this.setPasswordManagerActionableErrorListener_ = (error) =>
              this.actionableError = toMojoActionableError(error);
          const accountInfoChanged = (accountInfo: AccountInfo) =>
              this.accountInfo_ = accountInfo;

          // Request initial data.
          PasswordManagerImpl.getInstance().isAccountStorageActive().then(
              this.setIsAccountStorageActiveListener_);
          PasswordManagerImpl.getInstance()
              .getPasswordManagerActionableError()
              .then(error => {
                this.actionableError = error;
              });
          SyncBrowserProxyImpl.getInstance().getSyncInfo().then(
              syncInfoChanged);
          SyncBrowserProxyImpl.getInstance().getAccountInfo().then(
              accountInfoChanged);

          // Listen for changes.
          PasswordManagerImpl.getInstance()
              .addAccountStorageEnabledStateListener(
                  this.setIsAccountStorageActiveListener_);
          PasswordManagerImpl.getInstance()
              .addPasswordManagerActionableErrorChangedListener(
                  this.setPasswordManagerActionableErrorListener_);
          this.addWebUiListener('sync-info-changed', syncInfoChanged);
          this.addWebUiListener('stored-accounts-changed', accountInfoChanged);
        }

        override disconnectedCallback() {
          super.disconnectedCallback();

          assert(this.setIsAccountStorageActiveListener_);
          PasswordManagerImpl.getInstance()
              .removeAccountStorageEnabledStateListener(
                  this.setIsAccountStorageActiveListener_);
          this.setIsAccountStorageActiveListener_ = null;

          assert(this.setPasswordManagerActionableErrorListener_);
          PasswordManagerImpl.getInstance()
              .removePasswordManagerActionableErrorChangedListener(
                  this.setPasswordManagerActionableErrorListener_);
          this.setPasswordManagerActionableErrorListener_ = null;
        }

        override willUpdate(changedProperties: PropertyValues<this>) {
          super.willUpdate(changedProperties);

          const changedPrivateProperties =
              changedProperties as Map<PropertyKey, unknown>;
          if (changedPrivateProperties.has('syncInfo_')) {
            this.isSyncingPasswords = this.computeIsSyncingPasswords_();
          }
          if (changedPrivateProperties.has('accountInfo_')) {
            this.accountEmail = this.computeAccountEmail_();
            this.avatarImage = this.computeAvatarImage_();
          }
        }

        isTrustedVaultKeyNeeded(): boolean {
          return loadTimeData.getBoolean('enableTrustedVaultUnlock') &&
              this.actionableError ===
              PasswordManagerActionableError.kTrustedVaultKeyNeeded;
        }

        /**
         * Executes `action` if the trusted vault is unlocked. If the vault is
         * locked, dispatches an event to display the unlock/recovery dialog.
         */
        executeIfTrustedVaultUnlocked(action: () => void) {
          if (this.isTrustedVaultKeyNeeded()) {
            this.fire('show-trusted-vault-error-dialog');
            return;
          }
          action();
        }

        private computeIsSyncingPasswords_(): boolean {
          return !!(this.syncInfo_?.isSyncingPasswords);
        }

        private computeAccountEmail_(): string {
          return this.accountInfo_?.email || '';
        }

        private computeAvatarImage_(): string {
          return this.accountInfo_?.avatarImage || '';
        }
      }

      return UserUtilMixinLit;
    });


export interface UserUtilMixinLitInterface {
  isAccountStoreUser: boolean;
  isSyncingPasswords: boolean;
  actionableError: PasswordManagerActionableError|null;
  accountEmail: string;
  avatarImage: string;
  isTrustedVaultKeyNeeded(): boolean;
  executeIfTrustedVaultUnlocked(action: () => void): void;
}
