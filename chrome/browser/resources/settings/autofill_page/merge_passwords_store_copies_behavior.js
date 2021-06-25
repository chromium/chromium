// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview This behavior bundles the functionality for retrieving
 * saved passwords from the password manager and deduplicating eventual copies
 * existing stored both on the device and in the account.
 */

import {assert} from 'chrome://resources/js/assert.m.js';
import {ListPropertyUpdateBehavior} from 'chrome://resources/js/list_property_update_behavior.m.js';

import {MultiStorePasswordUiEntry} from './multi_store_password_ui_entry.js';
import {PasswordManagerImpl, PasswordManagerProxy} from './password_manager_proxy.js';

/**
 * @polymerBehavior
 */
const MergePasswordsStoreCopiesBehaviorImpl = {

  properties: {
    /**
     * Saved passwords after deduplicating versions that are repeated in the
     * account and on the device.
     * @type {!Array<!MultiStorePasswordUiEntry>}
     */
    savedPasswords: {
      type: Array,
      value: () => [],
    },
  },

  /**
   * @type {?function(!Array<PasswordManagerProxy.PasswordUiEntry>):void}
   * @private
   */
  setSavedPasswordsListener_: null,

  /** @override */
  attached() {
    this.setSavedPasswordsListener_ = passwordList => {
      const mergedPasswordList =
          this.mergePasswordsStoreDuplicates_(passwordList);

      // getCombinedId() is unique for each |entry|. If both copies are removed,
      // updateList() will consider this a removal. If only one copy is removed,
      // this will be treated as a removal plus an insertion.
      const getCombinedId =
          entry => [entry.deviceId, entry.accountId].join('_');
      this.updateList('savedPasswords', getCombinedId, mergedPasswordList);
    };

    PasswordManagerImpl.getInstance().getSavedPasswordList(
        this.setSavedPasswordsListener_);
    PasswordManagerImpl.getInstance().addSavedPasswordListChangedListener(
        this.setSavedPasswordsListener_);
    this.notifySplices('savedPasswords', []);
  },

  /** @override */
  detached() {
    PasswordManagerImpl.getInstance().removeSavedPasswordListChangedListener(
        assert(this.setSavedPasswordsListener_));
  },

  /**
   * @param {!Array<!PasswordManagerProxy.PasswordUiEntry>} passwordList
   * @return {!Array<!MultiStorePasswordUiEntry>}
   * @private
   */
  mergePasswordsStoreDuplicates_(passwordList) {
    /** @type {!Array<!MultiStorePasswordUiEntry>} */
    const multiStoreEntries = [];
    /** @type {!Map<number, !MultiStorePasswordUiEntry>} */
    const frontendIdToMergedEntry = new Map();
    for (const entry of passwordList) {
      if (frontendIdToMergedEntry.has(entry.frontendId)) {
        const mergeSucceded =
            frontendIdToMergedEntry.get(entry.frontendId).mergeInPlace(entry);
        if (mergeSucceded) {
          // The merge is in-place, so nothing to be done.
        } else {
          // The merge can fail in weird cases despite |frontendId| matching.
          // If so, just create another entry in the UI for |entry|. See also
          // crbug.com/1114697.
          multiStoreEntries.push(new MultiStorePasswordUiEntry(entry));
        }
      } else {
        const multiStoreEntry = new MultiStorePasswordUiEntry(entry);
        frontendIdToMergedEntry.set(entry.frontendId, multiStoreEntry);
        multiStoreEntries.push(multiStoreEntry);
      }
    }
    return multiStoreEntries;
  },
};

/**
 * @polymerBehavior
 */
export const MergePasswordsStoreCopiesBehavior =
    [ListPropertyUpdateBehavior, MergePasswordsStoreCopiesBehaviorImpl];


/** @interface */
export class MergePasswordsStoreCopiesBehaviorInterface {
  /** @return {!Array<!MultiStorePasswordUiEntry>} */
  get savedPasswords() {}
}
