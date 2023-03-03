// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from 'chrome://resources/js/assert_ts.js';
import {dedupingMixin, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {PasswordManagerImpl} from './password_manager_proxy.js';

type Constructor<T> = new (...args: any[]) => T;

/**
 * This mixin bundles functionality related to syncing, sign in status and
 * account storage.
 */
export const UserUtilMixin = dedupingMixin(
    <T extends Constructor<PolymerElement>>(superClass: T): T&
    Constructor<UserUtilMixinInterface> => {
      class UserUtilMixin extends superClass {
        static get properties() {
          return {
            /**
             * Indicates whether user opted in using passwords stored on
             * their account.
             */
            isOptedInForAccountStorage: Boolean,
          };
        }

        isOptedInForAccountStorage: boolean;

        private setIsOptedInForAccountStorageListener_:
            ((isOptedIn: boolean) => void)|null = null;

        override connectedCallback() {
          super.connectedCallback();

          // Create listener functions.
          this.setIsOptedInForAccountStorageListener_ = (optedIn) => {
            this.isOptedInForAccountStorage = optedIn;
          };
          // Request initial data.
          PasswordManagerImpl.getInstance().isOptedInForAccountStorage().then(
              this.setIsOptedInForAccountStorageListener_);

          // Listen for changes.
          PasswordManagerImpl.getInstance().addAccountStorageOptInStateListener(
              this.setIsOptedInForAccountStorageListener_);
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
          // TODO(crbug.com/1420548): Show move passwords dialog.
        }

        optOutFromAccountStorage() {
          // TODO(crbug.com/1420548): Show move passwords dialog.
        }
      }

      return UserUtilMixin;
    });


export interface UserUtilMixinInterface {
  isOptedInForAccountStorage: boolean;
  optInForAccountStorage(): void;
  optOutFromAccountStorage(): void;
}
