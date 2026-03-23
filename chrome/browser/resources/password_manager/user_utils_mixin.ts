// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {WebUiListenerMixin} from 'chrome://resources/cr_elements/web_ui_listener_mixin.js';
import {assert} from 'chrome://resources/js/assert.js';
import type {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {dedupingMixin} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {PasswordManagerActionableError, PasswordManagerImpl, toMojoActionableError} from './password_manager_proxy.js';
import type {AccountInfo, SyncInfo} from './sync_browser_proxy.js';
import {SyncBrowserProxyImpl} from './sync_browser_proxy.js';

type Constructor<T> = new (...args: any[]) => T;

/**
 * This mixin bundles functionality related to syncing, sign in status and
 * account storage.
 */
export const UserUtilMixin = dedupingMixin(
    <T extends Constructor<PolymerElement>>(superClass: T): T&
    Constructor<UserUtilMixinInterface> => {
      class UserUtilMixin extends WebUiListenerMixin
      (superClass) implements UserUtilMixinInterface {
        static get properties() {
          return {
            /**
             * If true, the edit dialog and removal notification show
             * information about which location(s) a password is stored.
             */
            isAccountStoreUser: {
              type: Boolean,
              value: false,
            },

            isSyncingPasswords: {
              type: Boolean,
              value: true,
              computed: 'computeIsSyncingPasswords_(syncInfo_)',
            },

            actionableError: {
              type: Number,
              value: PasswordManagerActionableError.kNoError,
            },

            /* Email of the primary account. */
            accountEmail: {
              type: String,
              value: '',
              computed: 'computeAccountEmail_(accountInfo_)',
            },

            /* Avatar image of the primary account. */
            avatarImage: {
              type: String,
              value: '',
              computed: 'computeAvatarImage_(accountInfo_)',
            },

            syncInfo_: {
              type: Object,
              value: null,
            },

            accountInfo_: {
              type: Object,
              value: null,
            },
          };
        }

        declare isAccountStoreUser: boolean;
        declare isSyncingPasswords: boolean;
        declare actionableError: PasswordManagerActionableError;
        declare accountEmail: string;
        declare avatarImage: string;
        declare private syncInfo_: SyncInfo|null;
        declare private accountInfo_: AccountInfo|null;

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
              .then(error => this.actionableError = error);
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

        enableAccountStorage() {
          PasswordManagerImpl.getInstance().setAccountStorageEnabled(true);
        }

        disableAccountStorage() {
          PasswordManagerImpl.getInstance().setAccountStorageEnabled(false);
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

      return UserUtilMixin;
    });


export interface UserUtilMixinInterface {
  isAccountStoreUser: boolean;
  isSyncingPasswords: boolean;
  actionableError: PasswordManagerActionableError;
  accountEmail: string;
  avatarImage: string;
  enableAccountStorage(): void;
  disableAccountStorage(): void;
}
