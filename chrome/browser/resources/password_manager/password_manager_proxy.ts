// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview PasswordManagerProxy is an abstraction over
 * chrome.passwordsPrivate which facilitates testing.
 */

export type SavedPasswordListChangedListener =
    (entries: chrome.passwordsPrivate.PasswordUiEntry[]) => void;
export type PasswordCheckStatusChangedListener =
    (status: chrome.passwordsPrivate.PasswordCheckStatus) => void;

/**
 * Interface for all callbacks to the password API.
 */
export interface PasswordManagerProxy {
  /**
   * Add an observer to the list of saved passwords.
   */
  addSavedPasswordListChangedListener(
      listener: SavedPasswordListChangedListener): void;

  /**
   * Remove an observer from the list of saved passwords.
   */
  removeSavedPasswordListChangedListener(
      listener: SavedPasswordListChangedListener): void;

  /**
   * Add an observer to the passwords check status change.
   */
  addPasswordCheckStatusListener(listener: PasswordCheckStatusChangedListener):
      void;

  /**
   * Remove an observer to the passwords check status change.
   */
  removePasswordCheckStatusListener(
      listener: PasswordCheckStatusChangedListener): void;

  /**
   * Request the list of saved passwords.
   */
  getSavedPasswordList(): Promise<chrome.passwordsPrivate.PasswordUiEntry[]>;

  /**
   * Requests the current status of the check.
   */
  getPasswordCheckStatus():
      Promise<chrome.passwordsPrivate.PasswordCheckStatus>;
}

/**
 * Implementation that accesses the private API.
 */
export class PasswordManagerImpl implements PasswordManagerProxy {
  addSavedPasswordListChangedListener(listener:
                                          SavedPasswordListChangedListener) {
    chrome.passwordsPrivate.onSavedPasswordsListChanged.addListener(listener);
  }

  removeSavedPasswordListChangedListener(listener:
                                             SavedPasswordListChangedListener) {
    chrome.passwordsPrivate.onSavedPasswordsListChanged.removeListener(
        listener);
  }

  addPasswordCheckStatusListener(listener: PasswordCheckStatusChangedListener) {
    chrome.passwordsPrivate.onPasswordCheckStatusChanged.addListener(listener);
  }

  removePasswordCheckStatusListener(listener:
                                        PasswordCheckStatusChangedListener) {
    chrome.passwordsPrivate.onPasswordCheckStatusChanged.removeListener(
        listener);
  }

  getSavedPasswordList() {
    return new Promise<chrome.passwordsPrivate.PasswordUiEntry[]>(resolve => {
      chrome.passwordsPrivate.getSavedPasswordList(passwords => {
        resolve(chrome.runtime.lastError ? [] : passwords);
      });
    });
  }

  getPasswordCheckStatus() {
    return new Promise<chrome.passwordsPrivate.PasswordCheckStatus>(resolve => {
      chrome.passwordsPrivate.getPasswordCheckStatus(resolve);
    });
  }

  static getInstance(): PasswordManagerProxy {
    return instance || (instance = new PasswordManagerImpl());
  }

  static setInstance(obj: PasswordManagerProxy) {
    instance = obj;
  }
}

let instance: PasswordManagerProxy|null = null;
