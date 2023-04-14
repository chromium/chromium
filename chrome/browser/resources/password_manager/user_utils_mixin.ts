// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {WebUiListenerMixin} from 'chrome://resources/cr_elements/web_ui_listener_mixin.js';
import {assert} from 'chrome://resources/js/assert_ts.js';
import {dedupingMixin, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {PasswordManagerImpl} from './password_manager_proxy.js';
import {AccountInfo, SyncBrowserProxyImpl, SyncInfo} from './sync_browser_proxy.js';

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
             * Indicates whether user opted in using passwords stored on
             * their account.
             */
            isOptedInForAccountStorage: {
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
                  'isOptedInForAccountStorage, isEligibleForAccountStorage)',
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

        isOptedInForAccountStorage: boolean;
        isEligibleForAccountStorage: boolean;
        isAccountStoreUser: boolean;
        isSyncingPasswords: boolean;
        accountEmail: string;
        avatarImage: string;
        private syncInfo_: SyncInfo;
        private accountInfo_: AccountInfo;

        private setIsOptedInForAccountStorageListener_:
            ((isOptedIn: boolean) => void)|null = null;

        override connectedCallback() {
          super.connectedCallback();

          // Create listener functions.
          this.setIsOptedInForAccountStorageListener_ = (optedIn) =>
              this.isOptedInForAccountStorage = optedIn;
          const syncInfoChanged = (syncInfo: SyncInfo) => this.syncInfo_ =
              syncInfo;
          const accountInfoChanged = (accountInfo: AccountInfo) =>
              this.accountInfo_ = accountInfo;

          // Request initial data.
          PasswordManagerImpl.getInstance().isOptedInForAccountStorage().then(
              this.setIsOptedInForAccountStorageListener_);
          SyncBrowserProxyImpl.getInstance().getSyncInfo().then(
              syncInfoChanged);
          SyncBrowserProxyImpl.getInstance().getAccountInfo().then(
              accountInfoChanged);

          // Listen for changes.
          PasswordManagerImpl.getInstance().addAccountStorageOptInStateListener(
              this.setIsOptedInForAccountStorageListener_);
          this.addWebUiListener('sync-info-changed', syncInfoChanged);
          this.addWebUiListener('stored-accounts-changed', accountInfoChanged);
        }

        override disconnectedCallback() {
          super.disconnectedCallback();

          assert(this.setIsOptedInForAccountStorageListener_);
          PasswordManagerImpl.getInstance()
              .removeAccountStorageOptInStateListener(
                  this.setIsOptedInForAccountStorageListener_);
          this.setIsOptedInForAccountStorageListener_ = null;
        }

        optInForAccountStorage() {
          PasswordManagerImpl.getInstance().optInForAccountStorage(true);
        }

        optOutFromAccountStorage() {
          PasswordManagerImpl.getInstance().optInForAccountStorage(false);
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
              this.isOptedInForAccountStorage;
        }
      }

      return UserUtilMixin;
    });


export interface UserUtilMixinInterface {
  isOptedInForAccountStorage: boolean;
  isEligibleForAccountStorage: boolean;
  isAccountStoreUser: boolean;
  isSyncingPasswords: boolean;
  accountEmail: string;
  avatarImage: string;
  optInForAccountStorage(): void;
  optOutFromAccountStorage(): void;
}
