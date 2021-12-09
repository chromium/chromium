// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview MultiStoreExceptionEntry is used for showing exceptions that
 * are duplicated across stores as a single item in the UI.
 */

import {MultiStoreIdHandler} from './multi_store_id_handler.js';

/**
 * A version of chrome.passwordsPrivate.ExceptionEntry used for deduplicating
 * exceptions from the device and the account.
 */
export class MultiStoreExceptionEntry extends MultiStoreIdHandler {
  private urls_: chrome.passwordsPrivate.UrlCollection;

  constructor(entry: chrome.passwordsPrivate.ExceptionEntry) {
    super();

    this.urls_ = entry.urls;

    this.setId(entry.id, entry.fromAccountStore);
  }

  /**
   * Incorporates the id of |otherEntry|, as long as |otherEntry| matches
   * |contents_| and the id corresponding to its store is not set. If these
   * preconditions are not satisfied, results in a no-op.
   * @return Whether the merge succeeded.
   */
  // TODO(crbug.com/1102294) Consider asserting frontendId as well.
  mergeInPlace(otherEntry: chrome.passwordsPrivate.ExceptionEntry): boolean {
    const alreadyHasCopyFromStore =
        (this.isPresentInAccount() && otherEntry.fromAccountStore) ||
        (this.isPresentOnDevice() && !otherEntry.fromAccountStore);
    if (alreadyHasCopyFromStore) {
      return false;
    }
    if (JSON.stringify(this.urls_) !== JSON.stringify(otherEntry.urls)) {
      return false;
    }
    this.setId(otherEntry.id, otherEntry.fromAccountStore);
    return true;
  }

  get urls() {
    return this.urls_;
  }
}
