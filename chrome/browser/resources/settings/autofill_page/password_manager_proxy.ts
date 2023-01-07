// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview PasswordManagerProxy is an abstraction over
 * chrome.passwordsPrivate which facilitates testing.
 */

type InsecureCredentials = chrome.passwordsPrivate.PasswordUiEntry[];
export type SavedPasswordListChangedListener =
    (entries: chrome.passwordsPrivate.PasswordUiEntry[]) => void;
export type PasswordExceptionListChangedListener =
    (entries: chrome.passwordsPrivate.ExceptionEntry[]) => void;
export type PasswordsFileExportProgressListener =
    (progress: chrome.passwordsPrivate.PasswordExportProgress) => void;
export type AccountStorageOptInStateChangedListener = (optInState: boolean) =>
    void;
export type CredentialsChangedListener = (credentials: InsecureCredentials) =>
    void;
export type PasswordCheckStatusChangedListener =
    (status: chrome.passwordsPrivate.PasswordCheckStatus) => void;

export type PasswordManagerAuthTimeoutListener = () => void;

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
   * Request the list of saved passwords.
   * TODO(https://crbug.com/919483): Return a promise instead of taking a
   * callback argument.
   */
  getSavedPasswordList(callback: SavedPasswordListChangedListener): void;

  /**
   * Log that the Passwords page was accessed from the Chrome Settings WebUI.
   */
  recordPasswordsPageAccessInSettings(): void;

  /**
   * Requests whether the account store is a default location for saving
   * passwords. False means the device store is a default one. Must be called
   * when the current user has already opted-in for account storage.
   * @return A promise that resolves to whether the account store is default.
   */
  isAccountStoreDefault(): Promise<boolean>;

  /**
   * Requests whether the given |url| meets the requirements to save a password
   * for it (e.g. valid, has proper scheme etc.).
   * @return A promise that resolves to the corresponding URLCollection on
   *     success and to null otherwise.
   */
  getUrlCollection(url: string):
      Promise<chrome.passwordsPrivate.UrlCollection|null>;

  /**
   * Saves a new password entry described by the given |options|.
   * @param options Details about a new password and storage to be used.
   * @return A promise that resolves when the new entry is added.
   */
  addPassword(options: chrome.passwordsPrivate.AddPasswordOptions):
      Promise<void>;

  /**
   * Changes the saved password corresponding to |ids|.
   * @param ids The ids for the password entry being updated.
   * @return A promise that resolves with the new IDs when the password is
   *     updated for all ids.
   */
  changeSavedPassword(
      ids: number, params: chrome.passwordsPrivate.ChangeSavedPasswordParams):
      Promise<number>;

  /**
   * Should remove the saved password and notify that the list has changed.
   * @param id The id for the password entry being removed. No-op if |id| is not
   *     in the list.
   * @param fromStores The store from which credential should be removed.
   */
  removeSavedPassword(
      id: number, fromStores: chrome.passwordsPrivate.PasswordStoreSet): void;

  /**
   * Moves a list of passwords from the device to the account
   * @param ids The ids for the password entries being moved.
   */
  movePasswordsToAccount(ids: number[]): void;

  /**
   * Add an observer to the list of password exceptions.
   */
  addExceptionListChangedListener(
      listener: PasswordExceptionListChangedListener): void;

  /**
   * Remove an observer from the list of password exceptions.
   */
  removeExceptionListChangedListener(
      listener: PasswordExceptionListChangedListener): void;

  /**
   * Request the list of password exceptions.
   * TODO(https://crbug.com/919483): Return a promise instead of taking a
   * callback argument.
   */
  getExceptionList(callback: PasswordExceptionListChangedListener): void;

  /**
   * Should remove the password exception and notify that the list has changed.
   * @param id The id for the exception url entry being removed. No-op if |id|
   *     is not in the list.
   */
  removeException(id: number): void;

  /**
   * Should undo the last saved password or exception removal and notify that
   * the list has changed.
   */
  undoRemoveSavedPasswordOrException(): void;

  /**
   * Gets the full credential for a given id.
   * @param id The id for the password entry being being retrieved.
   * @return A promise that resolves to the full |PasswordUiEntry|.
   */
  requestCredentialDetails(id: number):
      Promise<chrome.passwordsPrivate.PasswordUiEntry>;

  /**
   * Gets the saved password for a given id and reason.
   * @param id The id for the password entry being being retrieved.
   * @param reason The reason why the plaintext password is requested.
   * @return A promise that resolves to the plaintext password.
   */
  requestPlaintextPassword(
      id: number,
      reason: chrome.passwordsPrivate.PlaintextReason): Promise<string>;

  /**
   * Triggers the dialog for importing passwords.
   * @return A promise that resolves to the import results.
   */
  importPasswords(toStore: chrome.passwordsPrivate.PasswordStoreSet):
      Promise<chrome.passwordsPrivate.ImportResults>;

  /**
   * Triggers the dialog for exporting passwords.
   */
  exportPasswords(): Promise<void>;

  /**
   * Queries the status of any ongoing export.
   */
  requestExportProgressStatus():
      Promise<chrome.passwordsPrivate.ExportProgressStatus>;

  /**
   * Add an observer to the export progress.
   */
  addPasswordsFileExportProgressListener(
      listener: PasswordsFileExportProgressListener): void;

  /**
   * Remove an observer from the export progress.
   */
  removePasswordsFileExportProgressListener(
      listener: PasswordsFileExportProgressListener): void;

  cancelExportPasswords(): void;

  /**
   * Add an observer to the account storage opt-in state.
   */
  addAccountStorageOptInStateListener(
      listener: AccountStorageOptInStateChangedListener): void;

  /**
   * Remove an observer to the account storage opt-in state.
   */
  removeAccountStorageOptInStateListener(
      listener: AccountStorageOptInStateChangedListener): void;

  /**
   * Requests the account-storage opt-in state of the current user.
   * @return A promise that resolves to the opt-in state.
   */
  isOptedInForAccountStorage(): Promise<boolean>;

  /**
   * Triggers the opt-in or opt-out flow for the account storage.
   * @param optIn Whether the user wants to opt in or opt out.
   */
  optInForAccountStorage(optIn: boolean): void;

  /**
   * Refreshes the cache for automatic password change scripts if the cache is
   * stale.
   * @return A promise that resolves when the cache is fresh.
   */
  refreshScriptsIfNecessary(): Promise<void>;

  /**
   * Requests the start of the bulk password check.
   */
  startBulkPasswordCheck(): Promise<void>;

  /**
   * Requests to interrupt an ongoing bulk password check.
   */
  stopBulkPasswordCheck(): void;

  /**
   * Requests the latest information about insecure credentials.
   */
  getInsecureCredentials(): Promise<InsecureCredentials>;

  /**
   * Requests the current status of the check.
   */
  getPasswordCheckStatus():
      Promise<chrome.passwordsPrivate.PasswordCheckStatus>;

  /**
   * Starts an automated password change flow.
   * @param credential The credential for which to start the flow.
   */
  startAutomatedPasswordChange(
      credential: chrome.passwordsPrivate.PasswordUiEntry): Promise<boolean>;

  /**
   * Dismisses / mutes the |insecureCredential| in the passwords store.
   */
  muteInsecureCredential(insecureCredential:
                             chrome.passwordsPrivate.PasswordUiEntry): void;

  /**
   * Restores / unmutes the |insecureCredential| in the passwords store.
   */
  unmuteInsecureCredential(insecureCredential:
                               chrome.passwordsPrivate.PasswordUiEntry): void;

  /**
   * Records the state of a change password flow for |insecureCredential|
   * and notes it is a manual flow via |isManualFlow|.
   */
  recordChangePasswordFlowStarted(
      insecureCredential: chrome.passwordsPrivate.PasswordUiEntry,
      isManualFlow: boolean): void;

  /**
   * Requests extension of authentication validity.
   */
  extendAuthValidity(): void;

  /**
   * Add an observer to the insecure passwords change.
   */
  addInsecureCredentialsListener(listener: CredentialsChangedListener): void;

  /**
   * Remove an observer to the insecure passwords change.
   */
  removeInsecureCredentialsListener(listener: CredentialsChangedListener): void;

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
   * Add an observer for authentication timeout.
   */
  addPasswordManagerAuthTimeoutListener(
      listener: PasswordManagerAuthTimeoutListener): void;

  /**
   * Remove the specified observer for authentication timeout.
   */
  removePasswordManagerAuthTimeoutListener(
      listener: PasswordManagerAuthTimeoutListener): void;

  /**
   * Records a given interaction on the Password Check page.
   */
  recordPasswordCheckInteraction(interaction: PasswordCheckInteraction): void;

  /**
   * Records the referrer of a given navigation to the Password Check page.
   */
  recordPasswordCheckReferrer(referrer: PasswordCheckReferrer): void;

  /**
   * Switches Biometric authentication before filling state after
   * successful authentication.
   */
  switchBiometricAuthBeforeFillingState(): void;
}

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
 * Represents different referrers when navigating to the Password Check page.
 *
 * These values are persisted to logs. Entries should not be renumbered and
 * numeric values should never be reused.
 *
 * Needs to stay in sync with PasswordCheckReferrer in enums.xml and
 * password_check_referrer.h.
 */
export enum PasswordCheckReferrer {
  SAFETY_CHECK = 0,            // Web UI, recorded in JavaScript.
  PASSWORD_SETTINGS = 1,       // Web UI, recorded in JavaScript.
  PHISH_GUARD_DIALOG = 2,      // Native UI, recorded in C++.
  PASSWORD_BREACH_DIALOG = 3,  // Native UI, recorded in C++.
  // Must be last.
  COUNT = 4,
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

  getSavedPasswordList(callback: SavedPasswordListChangedListener) {
    chrome.passwordsPrivate.getSavedPasswordList(callback);
  }

  recordPasswordsPageAccessInSettings() {
    chrome.passwordsPrivate.recordPasswordsPageAccessInSettings();
  }

  isAccountStoreDefault() {
    return new Promise<boolean>(resolve => {
      chrome.passwordsPrivate.isAccountStoreDefault(resolve);
    });
  }

  getUrlCollection(url: string) {
    return new Promise<chrome.passwordsPrivate.UrlCollection|null>(resolve => {
      chrome.passwordsPrivate.getUrlCollection(url, urlCollection => {
        resolve(chrome.runtime.lastError ? null : urlCollection);
      });
    });
  }

  addPassword(options: chrome.passwordsPrivate.AddPasswordOptions) {
    return new Promise<void>(resolve => {
      chrome.passwordsPrivate.addPassword(options, resolve);
    });
  }

  changeSavedPassword(
      id: number, params: chrome.passwordsPrivate.ChangeSavedPasswordParams) {
    return new Promise<number>(resolve => {
      chrome.passwordsPrivate.changeSavedPassword(
          id, params, (newId: number) => {
            resolve(newId);
          });
    });
  }

  removeSavedPassword(
      id: number, fromStores: chrome.passwordsPrivate.PasswordStoreSet) {
    chrome.passwordsPrivate.removeSavedPassword(id, fromStores);
  }

  movePasswordsToAccount(ids: number[]) {
    chrome.passwordsPrivate.movePasswordsToAccount(ids);
  }

  addExceptionListChangedListener(listener:
                                      PasswordExceptionListChangedListener) {
    chrome.passwordsPrivate.onPasswordExceptionsListChanged.addListener(
        listener);
  }

  removeExceptionListChangedListener(listener:
                                         PasswordExceptionListChangedListener) {
    chrome.passwordsPrivate.onPasswordExceptionsListChanged.removeListener(
        listener);
  }

  getExceptionList(callback: PasswordExceptionListChangedListener) {
    chrome.passwordsPrivate.getPasswordExceptionList(callback);
  }

  removeException(id: number) {
    chrome.passwordsPrivate.removePasswordException(id);
  }

  undoRemoveSavedPasswordOrException() {
    chrome.passwordsPrivate.undoRemoveSavedPasswordOrException();
  }

  requestCredentialDetails(id: number) {
    return new Promise<chrome.passwordsPrivate.PasswordUiEntry>(
        (resolve, reject) => {
          chrome.passwordsPrivate.requestCredentialDetails(
              id,
              (passwordUiEntry: chrome.passwordsPrivate.PasswordUiEntry) => {
                if (chrome.runtime.lastError) {
                  reject(chrome.runtime.lastError.message);
                  return;
                }

                resolve(passwordUiEntry);
              });
        });
  }

  requestPlaintextPassword(
      id: number, reason: chrome.passwordsPrivate.PlaintextReason) {
    return new Promise<string>((resolve, reject) => {
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

  importPasswords(toStore: chrome.passwordsPrivate.PasswordStoreSet) {
    return new Promise<chrome.passwordsPrivate.ImportResults>(resolve => {
      chrome.passwordsPrivate.importPasswords(toStore, resolve);
    });
  }

  exportPasswords() {
    return new Promise<void>((resolve, reject) => {
      chrome.passwordsPrivate.exportPasswords(() => {
        if (chrome.runtime.lastError) {
          reject(chrome.runtime.lastError.message);
          return;
        }
        resolve();
      });
    });
  }

  requestExportProgressStatus() {
    return new Promise<chrome.passwordsPrivate.ExportProgressStatus>(
        resolve => {
          chrome.passwordsPrivate.requestExportProgressStatus(resolve);
        });
  }

  addPasswordsFileExportProgressListener(
      listener: PasswordsFileExportProgressListener) {
    chrome.passwordsPrivate.onPasswordsFileExportProgress.addListener(listener);
  }

  removePasswordsFileExportProgressListener(
      listener: PasswordsFileExportProgressListener) {
    chrome.passwordsPrivate.onPasswordsFileExportProgress.removeListener(
        listener);
  }

  cancelExportPasswords() {
    chrome.passwordsPrivate.cancelExportPasswords();
  }

  addAccountStorageOptInStateListener(
      listener: AccountStorageOptInStateChangedListener) {
    chrome.passwordsPrivate.onAccountStorageOptInStateChanged.addListener(
        listener);
  }

  removeAccountStorageOptInStateListener(
      listener: AccountStorageOptInStateChangedListener) {
    chrome.passwordsPrivate.onAccountStorageOptInStateChanged.removeListener(
        listener);
  }

  isOptedInForAccountStorage() {
    return new Promise<boolean>(resolve => {
      chrome.passwordsPrivate.isOptedInForAccountStorage(resolve);
    });
  }

  getPasswordCheckStatus() {
    return new Promise<chrome.passwordsPrivate.PasswordCheckStatus>(resolve => {
      chrome.passwordsPrivate.getPasswordCheckStatus(resolve);
    });
  }

  startAutomatedPasswordChange(credential:
                                   chrome.passwordsPrivate.PasswordUiEntry) {
    return new Promise<boolean>(resolve => {
      chrome.passwordsPrivate.startAutomatedPasswordChange(credential, resolve);
    });
  }

  optInForAccountStorage(optIn: boolean) {
    chrome.passwordsPrivate.optInForAccountStorage(optIn);
  }

  refreshScriptsIfNecessary() {
    return new Promise<void>(resolve => {
      chrome.passwordsPrivate.refreshScriptsIfNecessary(resolve);
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

  stopBulkPasswordCheck() {
    chrome.passwordsPrivate.stopPasswordCheck();
  }

  getInsecureCredentials() {
    return chrome.passwordsPrivate.getInsecureCredentials();
  }

  muteInsecureCredential(insecureCredential:
                             chrome.passwordsPrivate.PasswordUiEntry) {
    chrome.passwordsPrivate.muteInsecureCredential(insecureCredential);
  }

  unmuteInsecureCredential(insecureCredential:
                               chrome.passwordsPrivate.PasswordUiEntry) {
    chrome.passwordsPrivate.unmuteInsecureCredential(insecureCredential);
  }

  recordChangePasswordFlowStarted(
      insecureCredential: chrome.passwordsPrivate.PasswordUiEntry,
      isManualFlow: boolean) {
    chrome.passwordsPrivate.recordChangePasswordFlowStarted(
        insecureCredential, isManualFlow);
  }

  extendAuthValidity() {
    chrome.passwordsPrivate.extendAuthValidity();
  }

  addInsecureCredentialsListener(listener: CredentialsChangedListener) {
    chrome.passwordsPrivate.onInsecureCredentialsChanged.addListener(listener);
  }

  removeInsecureCredentialsListener(listener: CredentialsChangedListener) {
    chrome.passwordsPrivate.onInsecureCredentialsChanged.removeListener(
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

  addPasswordManagerAuthTimeoutListener(
      listener: PasswordManagerAuthTimeoutListener) {
    chrome.passwordsPrivate.onPasswordManagerAuthTimeout.addListener(listener);
  }

  removePasswordManagerAuthTimeoutListener(
      listener: PasswordManagerAuthTimeoutListener) {
    chrome.passwordsPrivate.onPasswordManagerAuthTimeout.removeListener(
        listener);
  }

  /** override */
  recordPasswordCheckInteraction(interaction: PasswordCheckInteraction) {
    chrome.metricsPrivate.recordEnumerationValue(
        'PasswordManager.BulkCheck.UserAction', interaction,
        PasswordCheckInteraction.COUNT);
  }

  switchBiometricAuthBeforeFillingState() {
    chrome.passwordsPrivate.switchBiometricAuthBeforeFillingState();
  }

  /** override */
  recordPasswordCheckReferrer(referrer: PasswordCheckReferrer) {
    chrome.metricsPrivate.recordEnumerationValue(
        'PasswordManager.BulkCheck.PasswordCheckReferrer', referrer,
        PasswordCheckReferrer.COUNT);
  }

  static getInstance(): PasswordManagerProxy {
    return instance || (instance = new PasswordManagerImpl());
  }

  static setInstance(obj: PasswordManagerProxy) {
    instance = obj;
  }
}

let instance: PasswordManagerProxy|null = null;
