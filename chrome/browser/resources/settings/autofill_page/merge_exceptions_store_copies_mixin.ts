// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview This behavior bundles the functionality for retrieving password
 * exceptions (websites where the user has chosen never to save passwords) from
 * the password manager, and deduplicating eventual copies stored both on the
 * device and in the account.
 */

import {assert} from 'chrome://resources/js/assert.m.js';
import {dedupingMixin, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {MultiStoreExceptionEntry} from './multi_store_exception_entry.js';
import {PasswordExceptionListChangedListener, PasswordManagerImpl} from './password_manager_proxy.js';

type Constructor<T> = new (...args: any[]) => T;

export interface MergeExceptionsStoreCopiesMixinInterface {
  passwordExceptions: Array<MultiStoreExceptionEntry>;
}

export const MergeExceptionsStoreCopiesMixin = dedupingMixin(
    <T extends Constructor<PolymerElement>>(superClass: T): T&
    Constructor<MergeExceptionsStoreCopiesMixinInterface> => {
      class MergeExceptionsStoreCopiesMixin extends superClass {
        static get properties() {
          return {
            /** An array of sites to display. */
            passwordExceptions: {
              type: Array,
              value: () => [],
            },
          };
        }

        passwordExceptions: Array<MultiStoreExceptionEntry>;
        private setPasswordExceptionsListener_:
            PasswordExceptionListChangedListener|null = null;

        override connectedCallback() {
          super.connectedCallback();

          this.setPasswordExceptionsListener_ = list => {
            this.passwordExceptions = mergeExceptionsStoreDuplicates(list);
          };

          PasswordManagerImpl.getInstance().getExceptionList(
              this.setPasswordExceptionsListener_);
          PasswordManagerImpl.getInstance().addExceptionListChangedListener(
              this.setPasswordExceptionsListener_);
        }

        override disconnectedCallback() {
          super.disconnectedCallback();

          PasswordManagerImpl.getInstance().removeExceptionListChangedListener(
              assert(this.setPasswordExceptionsListener_!));
          this.setPasswordExceptionsListener_ = null;
        }
      }

      return MergeExceptionsStoreCopiesMixin;
    });

function mergeExceptionsStoreDuplicates(
    exceptionList: Array<chrome.passwordsPrivate.ExceptionEntry>):
    Array<MultiStoreExceptionEntry> {
  const multiStoreEntries: Array<MultiStoreExceptionEntry> = [];
  const frontendIdToMergedEntry: Map<number, MultiStoreExceptionEntry> =
      new Map();
  for (const entry of exceptionList) {
    if (frontendIdToMergedEntry.has(entry.frontendId)) {
      const mergeSucceded =
          frontendIdToMergedEntry.get(entry.frontendId)!.mergeInPlace(entry);
      if (mergeSucceded) {
        // The merge is in-place, so nothing to be done.
      } else {
        // The merge can fail in weird cases despite |frontendId| matching.
        // If so, just create another entry in the UI for |entry|. See also
        // crbug.com/1114697.
        multiStoreEntries.push(new MultiStoreExceptionEntry(entry));
      }
    } else {
      const multiStoreEntry = new MultiStoreExceptionEntry(entry);
      frontendIdToMergedEntry.set(entry.frontendId, multiStoreEntry);
      multiStoreEntries.push(multiStoreEntry);
    }
  }
  return multiStoreEntries;
}
