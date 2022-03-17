// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview MultiStorePasswordUiEntry is used for showing entries that
 * are duplicated across stores as a single item in the UI.
 */

import {MultiStoreIdHandler} from './multi_store_id_handler.js';

/**
 * A version of chrome.passwordsPrivate.PasswordUiEntry used for deduplicating
 * entries from the device and the account.
 */
export class MultiStorePasswordUiEntry extends MultiStoreIdHandler {
  private contents_: MultiStorePasswordUiEntryContents;
  private password_: string = '';

  constructor(entry: chrome.passwordsPrivate.PasswordUiEntry) {
    super();

    this.contents_ = MultiStorePasswordUiEntry.getContents_(entry);

    this.setId(entry.id, entry.fromAccountStore);
  }

  /**
   * Incorporates the id of |otherEntry|, as long as |otherEntry| matches
   * |contents_| and the id corresponding to its store is not set. If these
   * preconditions are not satisfied, results in a no-op.
   * @return Whether the merge succeeded.
   */
  // TODO(crbug.com/1102294) Consider asserting frontendId as well.
  mergeInPlace(otherEntry: chrome.passwordsPrivate.PasswordUiEntry): boolean {
    const alreadyHasCopyFromStore =
        (this.isPresentInAccount() && otherEntry.fromAccountStore) ||
        (this.isPresentOnDevice() && !otherEntry.fromAccountStore);
    if (alreadyHasCopyFromStore) {
      return false;
    }
    if (JSON.stringify(this.contents_) !==
        JSON.stringify(MultiStorePasswordUiEntry.getContents_(otherEntry))) {
      return false;
    }
    this.setId(otherEntry.id, otherEntry.fromAccountStore);
    return true;
  }

  get urls(): chrome.passwordsPrivate.UrlCollection {
    return this.contents_.urls;
  }

  get username(): string {
    return this.contents_.username;
  }

  get password(): string {
    return this.password_;
  }

  set password(password: string) {
    this.password_ = password;
  }

  get federationText(): (string|undefined) {
    return this.contents_.federationText;
  }

  get note(): string {
    return this.contents_.note;
  }

  /**
   * Extract all the information except for the id and fromPasswordStore.
   */
  static getContents_(entry: chrome.passwordsPrivate.PasswordUiEntry):
      MultiStorePasswordUiEntryContents {
    return {
      urls: entry.urls,
      username: entry.username,
      federationText: entry.federationText,
      note: entry.passwordNote,
    };
  }
}

type MultiStorePasswordUiEntryContents = {
  urls: chrome.passwordsPrivate.UrlCollection,
  username: string,
  note: string,
  federationText?: string,
};
