// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview MultiStorePasswordUiEntry is used for showing entries that
 * are duplicated across stores as a single item in the UI.
 */

/**
 * A version of chrome.passwordsPrivate.PasswordUiEntry which contains a
 * password and some helper methods.
 */
export class MultiStorePasswordUiEntry {
  private contents_: chrome.passwordsPrivate.PasswordUiEntry;
  private password_: string = '';

  constructor(entry: chrome.passwordsPrivate.PasswordUiEntry) {
    this.contents_ = entry;
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
    return this.contents_.passwordNote;
  }

  get id(): number {
    return this.contents_.id;
  }

  get storedIn(): chrome.passwordsPrivate.PasswordStoreSet {
    return this.contents_.storedIn;
  }

  get isAndroidCredential(): boolean {
    return this.contents_.isAndroidCredential;
  }
}
