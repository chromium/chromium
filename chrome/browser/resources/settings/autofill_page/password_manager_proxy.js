// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview PasswordManagerProxy is an abstraction over
 * chrome.passwordsPrivate which facilitates testing.
 */

import {addSingletonGetter} from 'chrome://resources/js/cr.m.js';

/**
 * Interface for all callbacks to the password API.
 * @interface
 */
export class PasswordManagerProxy {
  /**
   * Add an observer to the list of saved passwords.
   * @param {function(!Array<!PasswordManagerProxy.PasswordUiEntry>):void}
   *     listener
   */
  addSavedPasswordListChangedListener(listener) {}

  /**
   * Remove an observer from the list of saved passwords.
   * @param {function(!Array<!PasswordManagerProxy.PasswordUiEntry>):void}
   *     listener
   */
  removeSavedPasswordListChangedListener(listener) {}

  /**
   * Request the list of saved passwords.
   * TODO(https://crbug.com/919483): Return a promise instead of taking a
   * callback argument.
   * @param {function(!Array<!PasswordManagerProxy.PasswordUiEntry>):void}
   *     callback
   */
  getSavedPasswordList(callback) {}

  /**
   * Log that the Passwords page was accessed from the Chrome Settings WebUI.
   */
  recordPasswordsPageAccessInSettings() {}

  /**
   * Changes the saved password corresponding to |ids|.
   * @param {!Array<number>} ids The ids for the password entry being updated.
   * @param {string} newUsername
   * @param {string} newPassword
   * @return {!Promise<void>} A promise that resolves when the password is
   *     updated for all ids.
   */
  changeSavedPassword(ids, newUsername, newPassword) {}

  /**
   * Should remove the saved password and notify that the list has changed.
   * @param {number} id The id for the password entry being removed.
   *     No-op if |id| is not in the list.
   */
  removeSavedPassword(id) {}

  /**
   * Should remove the saved passwords and notify that the list has changed.
   * @param {!Array<number>} ids The ids for the password entries being removed.
   *     Any id not in the list is ignored.
   */
  removeSavedPasswords(ids) {}

  /**
   * Moves a list of passwords from the device to the account
   * @param {!Array<number>} ids The ids for the password entries being moved.
   */
  movePasswordsToAccount(ids) {}

  /**
   * Add an observer to the list of password exceptions.
   * @param {function(!Array<!PasswordManagerProxy.ExceptionEntry>):void}
   *     listener
   */
  addExceptionListChangedListener(listener) {}

  /**
   * Remove an observer from the list of password exceptions.
   * @param {function(!Array<!PasswordManagerProxy.ExceptionEntry>):void}
   *     listener
   */
  removeExceptionListChangedListener(listener) {}

  /**
   * Request the list of password exceptions.
   * TODO(https://crbug.com/919483): Return a promise instead of taking a
   * callback argument.
   * @param {function(!Array<!PasswordManagerProxy.ExceptionEntry>):void}
   *     callback
   */
  getExceptionList(callback) {}

  /**
   * Should remove the password exception and notify that the list has changed.
   * @param {number} id The id for the exception url entry being removed.
   *     No-op if |id| is not in the list.
   */
  removeException(id) {}

  /**
   * Should remove the password exceptions and notify that the list has changed.
   * @param {!Array<number>} ids The ids for the exception url entries being
   * removed. Any |id| not in the list is ignored.
   */
  removeExceptions(ids) {}

  /**
   * Should undo the last saved password or exception removal and notify that
   * the list has changed.
   */
  undoRemoveSavedPasswordOrException() {}

  /**
   * Gets the saved password for a given login pair.
   * @param {number} id The id for the password entry being being retrieved.
   * @param {!chrome.passwordsPrivate.PlaintextReason} reason The reason why the
   *     plaintext password is requested.
   * @return {!Promise<string>} A promise that resolves to the plaintext
   * password.
   */
  requestPlaintextPassword(id, reason) {}

  /**
   * Triggers the dialogue for importing passwords.
   */
  importPasswords() {}

  /**
   * Triggers the dialogue for exporting passwords.
   * TODO(https://crbug.com/919483): Return a promise instead of taking a
   * callback argument.
   * @param {function():void} callback
   */
  exportPasswords(callback) {}

  /**
   * Queries the status of any ongoing export.
   * TODO(https://crbug.com/919483): Return a promise instead of taking a
   * callback argument.
   * @param {function(!PasswordManagerProxy.ExportProgressStatus):void}
   *     callback
   */
  requestExportProgressStatus(callback) {}

  /**
   * Add an observer to the export progress.
   * @param {function(!PasswordManagerProxy.PasswordExportProgress):void}
   *     listener
   */
  addPasswordsFileExportProgressListener(listener) {}

  /**
   * Remove an observer from the export progress.
   * @param {function(!PasswordManagerProxy.PasswordExportProgress):void}
   *     listener
   */
  removePasswordsFileExportProgressListener(listener) {}

  cancelExportPasswords() {}

  /**
   * Add an observer to the account storage opt-in state.
   * @param {function(boolean):void} listener
   */
  addAccountStorageOptInStateListener(listener) {}

  /**
   * Remove an observer to the account storage opt-in state.
   * @param {function(boolean):void} listener
   */
  removeAccountStorageOptInStateListener(listener) {}

  /**
   * Requests the account-storage opt-in state of the current user.
   * @return {!Promise<(boolean)>} A promise that resolves to the opt-in state.
   */
  isOptedInForAccountStorage() {}

  /**
   * Triggers the opt-in or opt-out flow for the account storage.
   * @param {boolean} optIn Whether the user wants to opt in or opt out.
   */
  optInForAccountStorage(optIn) {}

  /**
   * Requests the start of the bulk password check.
   * @return {!Promise<(void)>}
   */
  startBulkPasswordCheck() {}

  /**
   * Requests to interrupt an ongoing bulk password check.
   */
  stopBulkPasswordCheck() {}

  /**
   * Requests the latest information about compromised credentials.
   * @return {!Promise<(PasswordManagerProxy.InsecureCredentials)>}
   */
  getCompromisedCredentials() {}

  /**
   * Requests the latest information about weak credentials.
   * @return {!Promise<(PasswordManagerProxy.InsecureCredentials)>}
   */
  getWeakCredentials() {}

  /**
   * Returns the current status of the check via |callback|.
   * @return {!Promise<(PasswordManagerProxy.PasswordCheckStatus)>}
   */
  getPasswordCheckStatus() {}

  /**
   * Requests to remove |insecureCredential| from the password store.
   * @param {!PasswordManagerProxy.InsecureCredential} insecureCredential
   */
  removeInsecureCredential(insecureCredential) {}

  /**
   * Add an observer to the compromised passwords change.
   * @param {function(!PasswordManagerProxy.InsecureCredentials):void}
   *      listener
   */
  addCompromisedCredentialsListener(listener) {}

  /**
   * Remove an observer to the compromised passwords change.
   * @param {function(!PasswordManagerProxy.InsecureCredentials):void}
   *     listener
   */
  removeCompromisedCredentialsListener(listener) {}

  /**
   * Add an observer to the weak passwords change.
   * @param {function(!PasswordManagerProxy.InsecureCredentials):void}
   *      listener
   */
  addWeakCredentialsListener(listener) {}

  /**
   * Remove an observer to the weak passwords change.
   * @param {function(!PasswordManagerProxy.InsecureCredentials):void}
   *     listener
   */
  removeWeakCredentialsListener(listener) {}

  /**
   * Add an observer to the passwords check status change.
   * @param {function(!PasswordManagerProxy.PasswordCheckStatus):void} listener
   */
  addPasswordCheckStatusListener(listener) {}

  /**
   * Remove an observer to the passwords check status change.
   * @param {function(!PasswordManagerProxy.PasswordCheckStatus):void} listener
   */
  removePasswordCheckStatusListener(listener) {}

  /**
   * Requests the plaintext password for |credential|. |callback| gets invoked
   * with the same |credential|, whose |password| field will be set.
   * @param {!PasswordManagerProxy.InsecureCredential} credential
   * @param {!chrome.passwordsPrivate.PlaintextReason} reason
   * @return {!Promise<!PasswordManagerProxy.InsecureCredential>} A promise
   *     that resolves to the InsecureCredential with the password field
   *     populated.
   */
  getPlaintextInsecurePassword(credential, reason) {}

  /**
   * Requests to change the password of |credential| to |new_password|.
   * @param {!PasswordManagerProxy.InsecureCredential} credential
   * @param {string} newPassword
   * @return {!Promise<void>} A promise that resolves when the password is
   *     updated.
   */
  changeInsecureCredential(credential, newPassword) {}

  /**
   * Records a given interaction on the Password Check page.
   * @param {!PasswordManagerProxy.PasswordCheckInteraction} interaction
   */
  recordPasswordCheckInteraction(interaction) {}

  /**
   * Records the referrer of a given navigation to the Password Check page.
   * @param {!PasswordManagerProxy.PasswordCheckReferrer} referrer
   */
  recordPasswordCheckReferrer(referrer) {}
}

// TODO(https://crbug.com/1047726): Instead of exposing these classes on
// PasswordManagerProxy, they should be living in their own "settings.passwords"
// namespace and be exported by this file.

/** @typedef {chrome.passwordsPrivate.PasswordUiEntry} */
PasswordManagerProxy.PasswordUiEntry;

/** @typedef {chrome.passwordsPrivate.UrlCollection} */
PasswordManagerProxy.UrlCollection;

/** @typedef {chrome.passwordsPrivate.ExceptionEntry} */
PasswordManagerProxy.ExceptionEntry;

/** @typedef {chrome.passwordsPrivate.PasswordExportProgress} */
PasswordManagerProxy.PasswordExportProgress;

/** @typedef {chrome.passwordsPrivate.ExportProgressStatus} */
PasswordManagerProxy.ExportProgressStatus;

/** @typedef {chrome.passwordsPrivate.InsecureCredential} */
PasswordManagerProxy.InsecureCredential;

/** @typedef {Array<!chrome.passwordsPrivate.InsecureCredential>} */
PasswordManagerProxy.InsecureCredentials;

/** @typedef {chrome.passwordsPrivate.PasswordCheckStatus} */
PasswordManagerProxy.PasswordCheckStatus;

/**
 * Represents different interactions the user can perform on the Password Check
 * page.
 *
 * These values are persisted to logs. Entries should not be renumbered and
 * numeric values should never be reused.
 *
 * Needs to stay in sync with PasswordCheckInteraction in enums.xml and
 * password_manager_metrics_util.h.
 *
 * @enum {number}
 */
PasswordManagerProxy.PasswordCheckInteraction = {
  START_CHECK_AUTOMATICALLY: 0,
  START_CHECK_MANUALLY: 1,
  STOP_CHECK: 2,
  CHANGE_PASSWORD: 3,
  EDIT_PASSWORD: 4,
  REMOVE_PASSWORD: 5,
  SHOW_PASSWORD: 6,
  // Must be last.
  COUNT: 7,
};

/**
 * Represents different referrers when navigating to the Password Check page.
 *
 * These values are persisted to logs. Entries should not be renumbered and
 * numeric values should never be reused.
 *
 * Needs to stay in sync with PasswordCheckReferrer in enums.xml and
 * password_check_referrer.h.
 *
 * @enum {number}
 */
PasswordManagerProxy.PasswordCheckReferrer = {
  SAFETY_CHECK: 0,            // Web UI, recorded in JavaScript.
  PASSWORD_SETTINGS: 1,       // Web UI, recorded in JavaScript.
  PHISH_GUARD_DIALOG: 2,      // Native UI, recorded in C++.
  PASSWORD_BREACH_DIALOG: 3,  // Native UI, recorded in C++.
  // Must be last.
  COUNT: 4,
};

/**
 * Implementation that accesses the private API.
 * @implements {PasswordManagerProxy}
 */
export class PasswordManagerImpl {
  /** @override */
  addSavedPasswordListChangedListener(listener) {
    chrome.passwordsPrivate.onSavedPasswordsListChanged.addListener(listener);
  }

  /** @override */
  removeSavedPasswordListChangedListener(listener) {
    chrome.passwordsPrivate.onSavedPasswordsListChanged.removeListener(
        listener);
  }

  /** @override */
  getSavedPasswordList(callback) {
    chrome.passwordsPrivate.getSavedPasswordList(callback);
  }

  /** @override */
  recordPasswordsPageAccessInSettings() {
    chrome.passwordsPrivate.recordPasswordsPageAccessInSettings();
  }

  /** @override */
  changeSavedPassword(ids, newUsername, newPassword) {
    return new Promise(resolve => {
      chrome.passwordsPrivate.changeSavedPassword(
          ids, newUsername, newPassword, resolve);
    });
  }

  /** @override */
  removeSavedPassword(id) {
    chrome.passwordsPrivate.removeSavedPassword(id);
  }

  /** @override */
  removeSavedPasswords(ids) {
    chrome.passwordsPrivate.removeSavedPasswords(ids);
  }

  /** @override */
  movePasswordsToAccount(ids) {
    chrome.passwordsPrivate.movePasswordsToAccount(ids);
  }

  /** @override */
  addExceptionListChangedListener(listener) {
    chrome.passwordsPrivate.onPasswordExceptionsListChanged.addListener(
        listener);
  }

  /** @override */
  removeExceptionListChangedListener(listener) {
    chrome.passwordsPrivate.onPasswordExceptionsListChanged.removeListener(
        listener);
  }

  /** @override */
  getExceptionList(callback) {
    chrome.passwordsPrivate.getPasswordExceptionList(callback);
  }

  /** @override */
  removeException(id) {
    chrome.passwordsPrivate.removePasswordException(id);
  }

  /** @override */
  removeExceptions(ids) {
    chrome.passwordsPrivate.removePasswordExceptions(ids);
  }

  /** @override */
  undoRemoveSavedPasswordOrException() {
    chrome.passwordsPrivate.undoRemoveSavedPasswordOrException();
  }

  /** @override */
  requestPlaintextPassword(id, reason) {
    return new Promise((resolve, reject) => {
      chrome.passwordsPrivate.requestPlaintextPassword(
          id, reason, (password) => {
            if (chrome.runtime.lastError) {
              reject(chrome.runtime.lastError.message);
              return;
            }

            resolve(password);
          });
    });
  }

  /** @override */
  importPasswords() {
    chrome.passwordsPrivate.importPasswords();
  }

  /** @override */
  exportPasswords(callback) {
    chrome.passwordsPrivate.exportPasswords(callback);
  }

  /** @override */
  requestExportProgressStatus(callback) {
    chrome.passwordsPrivate.requestExportProgressStatus(callback);
  }

  /** @override */
  addPasswordsFileExportProgressListener(listener) {
    chrome.passwordsPrivate.onPasswordsFileExportProgress.addListener(listener);
  }

  /** @override */
  removePasswordsFileExportProgressListener(listener) {
    chrome.passwordsPrivate.onPasswordsFileExportProgress.removeListener(
        listener);
  }

  /** @override */
  cancelExportPasswords() {
    chrome.passwordsPrivate.cancelExportPasswords();
  }

  /** @override */
  addAccountStorageOptInStateListener(listener) {
    chrome.passwordsPrivate.onAccountStorageOptInStateChanged.addListener(
        listener);
  }

  /** @override */
  removeAccountStorageOptInStateListener(listener) {
    chrome.passwordsPrivate.onAccountStorageOptInStateChanged.removeListener(
        listener);
  }

  /** @override */
  isOptedInForAccountStorage() {
    return new Promise(resolve => {
      chrome.passwordsPrivate.isOptedInForAccountStorage(resolve);
    });
  }

  /** @override */
  getPasswordCheckStatus() {
    return new Promise(resolve => {
      chrome.passwordsPrivate.getPasswordCheckStatus(resolve);
    });
  }

  /** @override */
  optInForAccountStorage(optIn) {
    chrome.passwordsPrivate.optInForAccountStorage(optIn);
  }

  /** @override */
  startBulkPasswordCheck() {
    return new Promise((resolve, reject) => {
      chrome.passwordsPrivate.startPasswordCheck(() => {
        if (chrome.runtime.lastError) {
          reject(chrome.runtime.lastError.message);
          return;
        }
        resolve();
      });
    });
  }

  /** @override */
  stopBulkPasswordCheck() {
    chrome.passwordsPrivate.stopPasswordCheck();
  }

  /** @override */
  getCompromisedCredentials() {
    return new Promise(resolve => {
      chrome.passwordsPrivate.getCompromisedCredentials(resolve);
    });
  }

  /** @override */
  getWeakCredentials() {
    return new Promise(resolve => {
      chrome.passwordsPrivate.getWeakCredentials(resolve);
    });
  }

  /** @override */
  removeInsecureCredential(insecureCredential) {
    chrome.passwordsPrivate.removeInsecureCredential(insecureCredential);
  }

  /** @override */
  addCompromisedCredentialsListener(listener) {
    chrome.passwordsPrivate.onCompromisedCredentialsChanged.addListener(
        listener);
  }

  /** @override */
  removeCompromisedCredentialsListener(listener) {
    chrome.passwordsPrivate.onCompromisedCredentialsChanged.removeListener(
        listener);
  }

  /** @override */
  addWeakCredentialsListener(listener) {
    chrome.passwordsPrivate.onWeakCredentialsChanged.addListener(listener);
  }

  /** @override */
  removeWeakCredentialsListener(listener) {
    chrome.passwordsPrivate.onWeakCredentialsChanged.removeListener(listener);
  }

  /** @override */
  addPasswordCheckStatusListener(listener) {
    chrome.passwordsPrivate.onPasswordCheckStatusChanged.addListener(listener);
  }

  /** @override */
  removePasswordCheckStatusListener(listener) {
    chrome.passwordsPrivate.onPasswordCheckStatusChanged.removeListener(
        listener);
  }

  /** @override */
  getPlaintextInsecurePassword(credential, reason) {
    return new Promise((resolve, reject) => {
      chrome.passwordsPrivate.getPlaintextInsecurePassword(
          credential, reason, credentialWithPassword => {
            if (chrome.runtime.lastError) {
              reject(chrome.runtime.lastError.message);
              return;
            }

            resolve(credentialWithPassword);
          });
    });
  }

  /** @override */
  changeInsecureCredential(credential, newPassword) {
    return new Promise(resolve => {
      chrome.passwordsPrivate.changeInsecureCredential(
          credential, newPassword, resolve);
    });
  }

  /** override */
  recordPasswordCheckInteraction(interaction) {
    chrome.metricsPrivate.recordEnumerationValue(
        'PasswordManager.BulkCheck.UserAction', interaction,
        PasswordManagerProxy.PasswordCheckInteraction.COUNT);
  }

  /** override */
  recordPasswordCheckReferrer(referrer) {
    chrome.metricsPrivate.recordEnumerationValue(
        'PasswordManager.BulkCheck.PasswordCheckReferrer', referrer,
        PasswordManagerProxy.PasswordCheckReferrer.COUNT);
  }
}

addSingletonGetter(PasswordManagerImpl);
