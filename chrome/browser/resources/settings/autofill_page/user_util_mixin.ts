// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {WebUiListenerMixin, WebUiListenerMixinInterface} from 'chrome://resources/cr_elements/web_ui_listener_mixin.js';
import {assert} from 'chrome://resources/js/assert_ts.js';
import {dedupingMixin, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {StoredAccount, SyncBrowserProxyImpl, SyncPrefs, SyncStatus} from '../people_page/sync_browser_proxy.js';

import {PasswordManagerImpl, PasswordManagerProxy} from './password_manager_proxy.js';

type Constructor<T> = new (...args: any[]) => T;

/**
 * This mixin bundles functionality related to syncing, sign in status and
 * account storage. It is used by <passwords-section> and <passwords-check>.
 */
export const UserUtilMixin = dedupingMixin(
    <T extends Constructor<PolymerElement>>(superClass: T): T&
    Constructor<UserUtilMixinInterface>&
    Constructor<WebUiListenerMixinInterface> => {
      class UserUtilMixin extends WebUiListenerMixin
      (superClass) implements UserUtilMixinInterface {
        static get properties() {
          return {
            /**
             * The list of stored accounts.
             */
            storedAccounts_: Array,

            /**
             * Indicates whether user opted in using passwords stored on
             * their account.
             */
            isOptedInForAccountStorage: Boolean,
            syncPrefs: Object,
            syncStatus: Object,

            signedIn: {
              type: Boolean,
              computed: 'computeSignedIn_(syncStatus, storedAccounts_)',
            },

            isSyncingPasswords: {
              type: Boolean,
              computed: 'computeIsSyncingPasswords_(syncPrefs, syncStatus)',
            },

            /**
             * Indicates whether user is eligible to using passwords stored
             * on their account.
             */
            eligibleForAccountStorage: {
              type: Boolean,
              computed: 'computeEligibleForAccountStorage_(' +
                  'syncStatus, signedIn, syncPrefs)',
            },

            /**
             * If true, the edit dialog and removal notification show
             * information about which location(s) a password is stored.
             */
            isAccountStoreUser: {
              type: Boolean,
              computed: 'computeIsAccountStoreUser_(' +
                  'eligibleForAccountStorage, isOptedInForAccountStorage)',
            },

            profileEmail: {
              type: String,
              value: '',
              computed: 'getFirstStoredAccountEmail_(storedAccounts_)',
            },
          };
        }

        isAccountStoreUser: boolean;
        isOptedInForAccountStorage: boolean;
        syncPrefs: SyncPrefs;
        syncStatus: SyncStatus;
        profileEmail: string;
        signedIn: boolean;
        isSyncingPasswords: boolean;
        eligibleForAccountStorage: boolean;

        private passwordManager_: PasswordManagerProxy =
            PasswordManagerImpl.getInstance();
        private storedAccounts_: StoredAccount[];
        private setIsOptedInForAccountStorageListener_:
            ((isOptedIn: boolean) => void)|null = null;

        override connectedCallback() {
          super.connectedCallback();

          // Create listener functions.
          this.setIsOptedInForAccountStorageListener_ = (optedIn) => {
            this.isOptedInForAccountStorage = optedIn;
          };
          // Request initial data.
          this.passwordManager_.isOptedInForAccountStorage().then(
              this.setIsOptedInForAccountStorageListener_);

          // Listen for changes.
          this.passwordManager_.addAccountStorageOptInStateListener(
              this.setIsOptedInForAccountStorageListener_!);

          const syncBrowserProxy = SyncBrowserProxyImpl.getInstance();

          const syncStatusChanged = (syncStatus: SyncStatus) => {
            this.syncStatus = syncStatus;
          };
          syncBrowserProxy.getSyncStatus().then(syncStatusChanged);
          this.addWebUiListener('sync-status-changed', syncStatusChanged);

          const syncPrefsChanged = (syncPrefs: SyncPrefs) => {
            this.syncPrefs = syncPrefs;
          };
          this.addWebUiListener('sync-prefs-changed', syncPrefsChanged);
          syncBrowserProxy.sendSyncPrefsChanged();

          const storedAccountsChanged = (accounts: StoredAccount[]) => {
            this.storedAccounts_ = accounts;
          };
          syncBrowserProxy.getStoredAccounts().then(storedAccountsChanged);
          this.addWebUiListener(
              'stored-accounts-updated', storedAccountsChanged);
        }

        override disconnectedCallback() {
          super.disconnectedCallback();

          assert(this.setIsOptedInForAccountStorageListener_);
          this.passwordManager_.removeAccountStorageOptInStateListener(
              this.setIsOptedInForAccountStorageListener_);
          this.setIsOptedInForAccountStorageListener_ = null;
        }

        optInForAccountStorage() {
          this.passwordManager_.optInForAccountStorage(true);
        }

        optOutFromAccountStorage() {
          this.passwordManager_.optInForAccountStorage(false);
        }

        private computeSignedIn_(): boolean {
          return !!this.syncStatus && !!this.syncStatus.signedIn ?
              !this.syncStatus.hasError :
              (!!this.storedAccounts_ && this.storedAccounts_.length > 0);
        }

        /**
         * @return true iff the user is syncing passwords.
         */
        private computeIsSyncingPasswords_(): boolean {
          return !!this.syncStatus && !!this.syncStatus.signedIn &&
              !this.syncStatus.hasError && !!this.syncPrefs &&
              this.syncPrefs.passwordsSynced;
        }

        private computeEligibleForAccountStorage_(): boolean {
          // The user must have signed in but should have sync disabled
          // (|!this.syncStatus_.signedin|). They should not be using a
          // custom passphrase to encrypt their sync data, since there's no
          // way for account storage users to input their passphrase and
          // decrypt the passwords.
          return (!!this.syncStatus && !this.syncStatus.signedIn) &&
              this.signedIn &&
              (!this.syncPrefs || !this.syncPrefs.encryptAllData);
        }

        private computeIsAccountStoreUser_(): boolean {
          return this.eligibleForAccountStorage &&
              this.isOptedInForAccountStorage;
        }

        /**
         * Return the first available stored account. This is useful when
         * trying to figure out the account logged into the content area
         * which seems to always be first even if multiple accounts are
         * available.
         * @return The email address of the first stored account or an empty
         *     string.
         */
        private getFirstStoredAccountEmail_(): string {
          return !!this.storedAccounts_ && this.storedAccounts_.length > 0 ?
              this.storedAccounts_[0].email :
              '';
        }
      }

      return UserUtilMixin;
    });


export interface UserUtilMixinInterface {
  isAccountStoreUser: boolean;
  isOptedInForAccountStorage: boolean;
  syncPrefs: SyncPrefs;
  syncStatus: SyncStatus;
  profileEmail: string;
  signedIn: boolean;
  isSyncingPasswords: boolean;
  eligibleForAccountStorage: boolean;
  optInForAccountStorage(): void;
  optOutFromAccountStorage(): void;
}
