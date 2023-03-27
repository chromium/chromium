// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {HatsBrowserProxyImpl, TrustSafetyInteraction} from '../hats_browser_proxy.js';

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

// WARNING: Keep synced with
// chrome/browser/ui/webui/settings/password_manager_handler.cc.
export enum PasswordManagerPage {
  PASSWORDS = 0,
  CHECKUP = 1,
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
   * Request the list of saved passwords.
   * @return A promise that resolves with the list of saved passwords.
   */
  getSavedPasswordList(): Promise<chrome.passwordsPrivate.PasswordUiEntry[]>;

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
   * @return A promise that resolves with the list of password exceptions.
   */
  getExceptionList(): Promise<chrome.passwordsPrivate.ExceptionEntry[]>;

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
   * Gets the list of full (with note and password) credentials for given ids.
   * @param ids The id for the password entries being retrieved.
   * @return A promise that resolves to |PasswordUiEntry[]|.
   */
  requestCredentialsDetails(ids: number[]):
      Promise<chrome.passwordsPrivate.PasswordUiEntry[]>;

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
   * Resumes the password import process when user has selected which passwords
   * to replace.
   * @return A promise that resolves to the |ImportResults|.
   */
  continueImport(selectedIds: number[]):
      Promise<chrome.passwordsPrivate.ImportResults>;

  /**
   * Resets the PasswordImporter if it is in the CONFLICTS/FINISHED state and
   * the user closes the dialog. Only when the PasswordImporter is in FINISHED
   * state, |deleteFile| option is taken into account.
   * @param deleteFile Whether to trigger deletion of the last imported file.
   */
  resetImporter(deleteFile: boolean): Promise<void>;

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
   * Records the state of a change password flow for |insecureCredential|.
   */
  recordChangePasswordFlowStarted(
      insecureCredential: chrome.passwordsPrivate.PasswordUiEntry): void;

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

  /**
   * Shows new Password Manager UI (chrome://password-manager).
   */
  showPasswordManager(page: PasswordManagerPage): void;
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

  getSavedPasswordList() {
    return chrome.passwordsPrivate.getSavedPasswordList();
  }

  recordPasswordsPageAccessInSettings() {
    chrome.passwordsPrivate.recordPasswordsPageAccessInSettings();
  }

  isAccountStoreDefault() {
    return chrome.passwordsPrivate.isAccountStoreDefault();
  }

  getUrlCollection(url: string) {
    return chrome.passwordsPrivate.getUrlCollection(url);
  }

  addPassword(options: chrome.passwordsPrivate.AddPasswordOptions) {
    return chrome.passwordsPrivate.addPassword(options);
  }

  changeSavedPassword(
      id: number, params: chrome.passwordsPrivate.ChangeSavedPasswordParams) {
    return chrome.passwordsPrivate.changeSavedPassword(id, params);
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

  getExceptionList() {
    return chrome.passwordsPrivate.getPasswordExceptionList();
  }

  removeException(id: number) {
    chrome.passwordsPrivate.removePasswordException(id);
  }

  undoRemoveSavedPasswordOrException() {
    chrome.passwordsPrivate.undoRemoveSavedPasswordOrException();
  }

  requestCredentialsDetails(ids: number[]) {
    return chrome.passwordsPrivate.requestCredentialsDetails(ids);
  }

  requestPlaintextPassword(
      id: number, reason: chrome.passwordsPrivate.PlaintextReason) {
    return chrome.passwordsPrivate.requestPlaintextPassword(id, reason);
  }

  importPasswords(toStore: chrome.passwordsPrivate.PasswordStoreSet) {
    return chrome.passwordsPrivate.importPasswords(toStore);
  }

  continueImport(selectedIds: number[]) {
    return chrome.passwordsPrivate.continueImport(selectedIds);
  }

  resetImporter(deleteFile: boolean) {
    return chrome.passwordsPrivate.resetImporter(deleteFile);
  }

  exportPasswords() {
    return chrome.passwordsPrivate.exportPasswords();
  }

  requestExportProgressStatus() {
    return chrome.passwordsPrivate.requestExportProgressStatus();
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
    return chrome.passwordsPrivate.isOptedInForAccountStorage();
  }

  getPasswordCheckStatus() {
    return chrome.passwordsPrivate.getPasswordCheckStatus();
  }

  optInForAccountStorage(optIn: boolean) {
    chrome.passwordsPrivate.optInForAccountStorage(optIn);
  }

  startBulkPasswordCheck() {
    // Note: PasswordCheck can be run automatically, such as when the row or
    // button is clicked from the passwords_section page. In this case, we also
    // want to count it as if the user ran password check, because it is still
    // an explicit action.
    HatsBrowserProxyImpl.getInstance().trustSafetyInteractionOccurred(
        TrustSafetyInteraction.RAN_PASSWORD_CHECK);
    return chrome.passwordsPrivate.startPasswordCheck();
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

  recordChangePasswordFlowStarted(insecureCredential:
                                      chrome.passwordsPrivate.PasswordUiEntry) {
    chrome.passwordsPrivate.recordChangePasswordFlowStarted(insecureCredential);
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

  showPasswordManager(page: PasswordManagerPage) {
    chrome.send('showPasswordManager', [page]);
  }

  static getInstance(): PasswordManagerProxy {
    return instance || (instance = new PasswordManagerImpl());
  }

  static setInstance(obj: PasswordManagerProxy) {
    instance = obj;
  }
}

let instance: PasswordManagerProxy|null = null;
