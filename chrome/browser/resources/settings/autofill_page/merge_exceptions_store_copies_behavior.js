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

import {MultiStoreExceptionEntry} from './multi_store_exception_entry.js';
import {PasswordManagerImpl, PasswordManagerProxy} from './password_manager_proxy.js';

/**
 * @polymerBehavior
 */
export const MergeExceptionsStoreCopiesBehavior = {

  properties: {
    /**
     * An array of sites to display.
     * @type {!Array<!MultiStoreExceptionEntry>}
     */
    passwordExceptions: {
      type: Array,
      value: () => [],
    },
  },

  /**
   * @type {?function(!Array<PasswordManagerProxy.ExceptionEntry>):void}
   * @private
   */
  setPasswordExceptionsListener_: null,

  /** @override */
  attached() {
    this.setPasswordExceptionsListener_ = list => {
      this.passwordExceptions = this.mergeExceptionsStoreDuplicates_(list);
    };

    PasswordManagerImpl.getInstance().getExceptionList(
        this.setPasswordExceptionsListener_);
    PasswordManagerImpl.getInstance().addExceptionListChangedListener(
        this.setPasswordExceptionsListener_);
  },

  /** @override */
  detached() {
    PasswordManagerImpl.getInstance().removeExceptionListChangedListener(
        assert(this.setPasswordExceptionsListener_));
  },

  /**
   * @param {!Array<!PasswordManagerProxy.ExceptionEntry>} exceptionList
   * @return {!Array<!MultiStoreExceptionEntry>}
   * @private
   */
  mergeExceptionsStoreDuplicates_(exceptionList) {
    /** @type {!Array<!MultiStoreExceptionEntry>} */
    const multiStoreEntries = [];
    /** @type {!Map<number, !MultiStoreExceptionEntry>} */
    const frontendIdToMergedEntry = new Map();
    for (const entry of exceptionList) {
      if (frontendIdToMergedEntry.has(entry.frontendId)) {
        const mergeSucceded =
            frontendIdToMergedEntry.get(entry.frontendId).mergeInPlace(entry);
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
  },
};

/** @interface */
export class MergeExceptionsStoreCopiesBehaviorInterface {
  /** @return {!Array<!MultiStoreExceptionEntry>} */
  get passwordExceptions() {}
}
