// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview This behavior bundles the functionality for retrieving
 * saved passwords from the password manager and deduplicating eventual copies
 * existing stored both on the device and in the account.
 */

import {assert} from 'chrome://resources/js/assert_ts.js';
import {ListPropertyUpdateMixin, ListPropertyUpdateMixinInterface} from 'chrome://resources/cr_elements/list_property_update_mixin.js';
import {dedupingMixin, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {PasswordManagerImpl} from './password_manager_proxy.js';

type Constructor<T> = new (...args: any[]) => T;

export const MergePasswordsStoreCopiesMixin = dedupingMixin(
    <T extends Constructor<PolymerElement>>(superClass: T): T&
    Constructor<MergePasswordsStoreCopiesMixinInterface> => {
      const superClassBase = ListPropertyUpdateMixin(superClass);

      class MergePasswordsStoreCopiesMixin extends superClassBase {
        static get properties() {
          return {
            /**
             * Saved passwords after deduplicating versions that are repeated in
             * the account and on the device.
             */
            savedPasswords: {
              type: Array,
              value: () => [],
            },
          };
        }

        savedPasswords: chrome.passwordsPrivate.PasswordUiEntry[] = [];
        private setSavedPasswordsListener_:
            ((entries: chrome.passwordsPrivate.PasswordUiEntry[]) =>
                 void)|null = null;

        override connectedCallback() {
          super.connectedCallback();
          this.setSavedPasswordsListener_ = passwordList => {
            for (const item of passwordList) {
              item.password = '';
            }
            this.savedPasswords = passwordList;
            this.notifySplices('savedPasswords', passwordList);
          };

          PasswordManagerImpl.getInstance().getSavedPasswordList().then(
              this.setSavedPasswordsListener_);
          PasswordManagerImpl.getInstance().addSavedPasswordListChangedListener(
              this.setSavedPasswordsListener_);
          this.notifySplices('savedPasswords', []);
        }

        override disconnectedCallback() {
          super.disconnectedCallback();
          assert(this.setSavedPasswordsListener_);
          PasswordManagerImpl.getInstance()
              .removeSavedPasswordListChangedListener(
                  this.setSavedPasswordsListener_);
          this.setSavedPasswordsListener_ = null;
        }
      }

      return MergePasswordsStoreCopiesMixin;
    });

export interface MergePasswordsStoreCopiesMixinInterface extends
    ListPropertyUpdateMixinInterface {
  savedPasswords: chrome.passwordsPrivate.PasswordUiEntry[];
}
