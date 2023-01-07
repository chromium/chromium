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
 * Represents different interactions the user can perform on the Password Check
 * page.
 *
 * These values are persisted to logs. Entries should not be renumbered and
 * numeric values should never be reused.
 *
 * Needs to stay in sync with PasswordCheckInteraction in enums.xml and
 * password_manager_metrics_util.h.
 */
export enum PasswordCheckInteraction {
  START_CHECK_AUTOMATICALLY = 0,
  START_CHECK_MANUALLY = 1,
  STOP_CHECK = 2,
  CHANGE_PASSWORD = 3,
  EDIT_PASSWORD = 4,
  REMOVE_PASSWORD = 5,
  SHOW_PASSWORD = 6,
  MUTE_PASSWORD = 7,
  UNMUTE_PASSWORD = 8,
  CHANGE_PASSWORD_AUTOMATICALLY = 9,
  // Must be last.
  COUNT = 10,
}

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

  /**
   * Requests the start of the bulk password check.
   */
  startBulkPasswordCheck(): Promise<void>;

  /**
   * Records a given interaction on the Password Check page.
   */
  recordPasswordCheckInteraction(interaction: PasswordCheckInteraction): void;
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

  startBulkPasswordCheck() {
    return new Promise<void>((resolve, reject) => {
      chrome.passwordsPrivate.startPasswordCheck(() => {
        if (chrome.runtime.lastError) {
          reject(chrome.runtime.lastError.message);
          return;
        }
        resolve();
      });
    });
  }

  recordPasswordCheckInteraction(interaction: PasswordCheckInteraction) {
    chrome.metricsPrivate.recordEnumerationValue(
        'PasswordManager.BulkCheck.UserAction', interaction,
        PasswordCheckInteraction.COUNT);
  }

  static getInstance(): PasswordManagerProxy {
    return instance || (instance = new PasswordManagerImpl());
  }

  static setInstance(obj: PasswordManagerProxy) {
    instance = obj;
  }
}

let instance: PasswordManagerProxy|null = null;
