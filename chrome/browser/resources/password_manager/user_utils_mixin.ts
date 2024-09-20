// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {WebUiListenerMixin} from 'chrome://resources/cr_elements/web_ui_listener_mixin.js';
import {assert} from 'chrome://resources/js/assert.js';
import type {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {dedupingMixin} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {PasswordManagerImpl} from './password_manager_proxy.js';
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
             * Indicates whether the account-scoped password storage is enabled.
             */
            isAccountStorageEnabled: {
              type: Boolean,
              value: false,
            },

            /* Account storage eligibility. */
            isEligibleForAccountStorage: {
              type: Boolean,
              value: false,
              computed: 'computeIsEligibleForAccountStorage_(syncInfo_)',
            },

            /**
             * If true, the edit dialog and removal notification show
             * information about which location(s) a password is stored.
             */
            isAccountStoreUser: {
              type: Boolean,
              computed: 'computeIsAccountStoreUser_(' +
                  'isAccountStorageEnabled, isEligibleForAccountStorage)',
            },

            isSyncingPasswords: {
              type: Boolean,
              value: true,
              computed: 'computeIsSyncingPasswords_(syncInfo_)',
            },

            /* Email of the primary account. */
            accountEmail: {
              type: String,
              value: '',
              computed: 'computeAccountEmail_(accountInfo_)',
            },

            /* Email of the primary account. */
            avatarImage: {
              type: String,
              value: '',
              computed: 'computeAvatarImage_(accountInfo_)',
            },
          };
        }

      isAccountStorageEnabled:
        boolean;
      isEligibleForAccountStorage:
        boolean;
      // Whether account storage is enabled and the default storage is account.
      isAccountStoreUser:
        boolean;
      isSyncingPasswords:
        boolean;
      accountEmail:
        string;
      avatarImage:
        string;
      private syncInfo_:
        SyncInfo;
      private accountInfo_:
        AccountInfo;

      private setIsAccountStorageEnabledListener_:
        ((enabled: boolean) => void)|null = null;

        override connectedCallback() {
          super.connectedCallback();

          // Create listener functions.
          this.setIsAccountStorageEnabledListener_ = (enabled) =>
              this.isAccountStorageEnabled = enabled;
          const syncInfoChanged = (syncInfo: SyncInfo) => this.syncInfo_ =
              syncInfo;
          const accountInfoChanged = (accountInfo: AccountInfo) =>
              this.accountInfo_ = accountInfo;

          // Request initial data.
          PasswordManagerImpl.getInstance().isAccountStorageEnabled().then(
              this.setIsAccountStorageEnabledListener_);
          SyncBrowserProxyImpl.getInstance().getSyncInfo().then(
              syncInfoChanged);
          SyncBrowserProxyImpl.getInstance().getAccountInfo().then(
              accountInfoChanged);

          // Listen for changes.
          PasswordManagerImpl.getInstance()
              .addAccountStorageEnabledStateListener(
                  this.setIsAccountStorageEnabledListener_);
          this.addWebUiListener('sync-info-changed', syncInfoChanged);
          this.addWebUiListener('stored-accounts-changed', accountInfoChanged);
        }

        override disconnectedCallback() {
          super.disconnectedCallback();

          assert(this.setIsAccountStorageEnabledListener_);
          PasswordManagerImpl.getInstance()
              .removeAccountStorageEnabledStateListener(
                  this.setIsAccountStorageEnabledListener_);
          this.setIsAccountStorageEnabledListener_ = null;
        }

        enableAccountStorage() {
          PasswordManagerImpl.getInstance().setAccountStorageEnabled(true);
        }

        disableAccountStorage() {
          PasswordManagerImpl.getInstance().setAccountStorageEnabled(false);
        }

        private computeIsEligibleForAccountStorage_(): boolean {
          return !!this.syncInfo_ && this.syncInfo_.isEligibleForAccountStorage;
        }

        private computeIsSyncingPasswords_(): boolean {
          return !!this.syncInfo_ && this.syncInfo_.isSyncingPasswords;
        }

        private computeAccountEmail_(): string {
          return (this.accountInfo_ ? this.accountInfo_.email : '');
        }

        private computeAvatarImage_(): string {
          return this.accountInfo_.avatarImage || '';
        }

        private computeIsAccountStoreUser_(): boolean {
          return this.isEligibleForAccountStorage &&
              this.isAccountStorageEnabled;
        }
      }

      return UserUtilMixin;
    });


export interface UserUtilMixinInterface {
  isAccountStorageEnabled: boolean;
  isEligibleForAccountStorage: boolean;
  isAccountStoreUser: boolean;
  isSyncingPasswords: boolean;
  accountEmail: string;
  avatarImage: string;
  enableAccountStorage(): void;
  disableAccountStorage(): void;
}
