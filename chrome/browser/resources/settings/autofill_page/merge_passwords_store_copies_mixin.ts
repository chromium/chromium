// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview This behavior bundles the functionality for retrieving
 * saved passwords from the password manager and deduplicating eventual copies
 * existing stored both on the device and in the account.
 */

import {assert} from 'chrome://resources/js/assert_ts.js';
import {ListPropertyUpdateMixin, ListPropertyUpdateMixinInterface} from 'chrome://resources/js/list_property_update_mixin.js';
import {dedupingMixin, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {MultiStorePasswordUiEntry} from './multi_store_password_ui_entry.js';
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

        savedPasswords: Array<MultiStorePasswordUiEntry> = [];
        private setSavedPasswordsListener_:
            ((entries: Array<chrome.passwordsPrivate.PasswordUiEntry>) =>
                 void)|null = null;

        override connectedCallback() {
          super.connectedCallback();
          this.setSavedPasswordsListener_ = passwordList => {
            const mergedPasswordList =
                this.mergePasswordsStoreDuplicates_(passwordList);

            // getCombinedId() is unique for each |entry|. If both copies are
            // removed, updateList() will consider this a removal. If only one
            // copy is removed, this will be treated as a removal plus an
            // insertion.
            const getCombinedId =
                (entry: MultiStorePasswordUiEntry) => [entry.deviceId,
                                                       entry.accountId]
                                                          .join('_');
            this.updateList(
                'savedPasswords', getCombinedId, mergedPasswordList);
          };

          PasswordManagerImpl.getInstance().getSavedPasswordList(
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

        private mergePasswordsStoreDuplicates_(
            passwordList: Array<chrome.passwordsPrivate.PasswordUiEntry>):
            Array<MultiStorePasswordUiEntry> {
          const multiStoreEntries: MultiStorePasswordUiEntry[] = [];
          const frontendIdToMergedEntry =
              new Map<number, MultiStorePasswordUiEntry>();
          for (const entry of passwordList) {
            if (frontendIdToMergedEntry.has(entry.frontendId)) {
              const mergeSucceded =
                  frontendIdToMergedEntry.get(entry.frontendId)!.mergeInPlace(
                      entry);
              if (mergeSucceded) {
                // The merge is in-place, so nothing to be done.
              } else {
                // The merge can fail in weird cases despite |frontendId|
                // matching. If so, just create another entry in the UI for
                // |entry|. See also crbug.com/1114697.
                multiStoreEntries.push(new MultiStorePasswordUiEntry(entry));
              }
            } else {
              const multiStoreEntry = new MultiStorePasswordUiEntry(entry);
              frontendIdToMergedEntry.set(entry.frontendId, multiStoreEntry);
              multiStoreEntries.push(multiStoreEntry);
            }
          }
          return multiStoreEntries;
        }
      }

      return MergePasswordsStoreCopiesMixin;
    });

export interface MergePasswordsStoreCopiesMixinInterface extends
    ListPropertyUpdateMixinInterface {
  savedPasswords: Array<MultiStorePasswordUiEntry>;
}
