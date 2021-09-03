// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview MultiStorePasswordUiEntry is used for showing entries that
 * are duplicated across stores as a single item in the UI.
 */

import {assert} from 'chrome://resources/js/assert.m.js';

import {MultiStoreIdHandler} from './multi_store_id_handler.js';
import {PasswordManagerProxy} from './password_manager_proxy.js';

/**
 * A version of chrome.passwordsPrivate.PasswordUiEntry used for deduplicating
 * entries from the device and the account.
 */
export class MultiStorePasswordUiEntry extends MultiStoreIdHandler {
  /**
   * @param {!chrome.passwordsPrivate.PasswordUiEntry} entry
   */
  constructor(entry) {
    super();

    /** @type {!MultiStorePasswordUiEntry.Contents} */
    this.contents_ = MultiStorePasswordUiEntry.getContents_(entry);

    /** @type {string} */
    this.password_ = '';

    this.setId(entry.id, entry.fromAccountStore);
  }

  /**
   * Incorporates the id of |otherEntry|, as long as |otherEntry| matches
   * |contents_| and the id corresponding to its store is not set. If these
   * preconditions are not satisfied, results in a no-op.
   * @param {!chrome.passwordsPrivate.PasswordUiEntry} otherEntry
   * @return {boolean} Returns whether the merge succeeded.
   */
  // TODO(crbug.com/1102294) Consider asserting frontendId as well.
  mergeInPlace(otherEntry) {
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

  /** @return {!chrome.passwordsPrivate.UrlCollection} */
  get urls() {
    return this.contents_.urls;
  }
  /** @return {string} */
  get username() {
    return this.contents_.username;
  }
  /** @return {string} */
  get password() {
    return this.password_;
  }
  /** @param {string} password */
  set password(password) {
    this.password_ = password;
  }
  /** @return {(string|undefined)} */
  get federationText() {
    return this.contents_.federationText;
  }

  /**
   * Extract all the information except for the id and fromPasswordStore.
   * @param {!chrome.passwordsPrivate.PasswordUiEntry} entry
   * @return {!MultiStorePasswordUiEntry.Contents}
   */
  static getContents_(entry) {
    return {
      urls: entry.urls,
      username: entry.username,
      federationText: entry.federationText
    };
  }
}

/**
 * @typedef {{
 *   urls: !chrome.passwordsPrivate.UrlCollection,
 *   username: string,
 *   federationText: (string|undefined)
 * }}
 */
MultiStorePasswordUiEntry.Contents;
