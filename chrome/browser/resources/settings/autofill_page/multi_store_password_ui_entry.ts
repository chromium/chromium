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

    switch (entry.storedIn) {
      case chrome.passwordsPrivate.PasswordStoreSet.ACCOUNT:
        this.setId(entry.id, /* fromAccountStore= */ true);
        break;
      case chrome.passwordsPrivate.PasswordStoreSet.DEVICE:
        this.setId(entry.id, /* fromAccountStore= */ false);
        break;
      case chrome.passwordsPrivate.PasswordStoreSet.DEVICE_AND_ACCOUNT:
        this.setId(entry.id, /* fromAccountStore= */ false);
        this.setId(entry.id, /* fromAccountStore= */ true);
        break;
    }
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
  private static getContents_(entry: chrome.passwordsPrivate.PasswordUiEntry):
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
