// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview PasswordManagerProxy is an abstraction over
 * chrome.passwordsPrivate which facilitates testing.
 */

export type BlockedSite = chrome.passwordsPrivate.ExceptionEntry;

export type CredentialsChangedListener =
    (credentials: chrome.passwordsPrivate.PasswordUiEntry[]) => void;
export type PasswordCheckStatusChangedListener =
    (status: chrome.passwordsPrivate.PasswordCheckStatus) => void;
export type BlockedSitesListChangedListener = (entries: BlockedSite[]) => void;
export type PasswordsFileExportProgressListener =
    (progress: chrome.passwordsPrivate.PasswordExportProgress) => void;

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
  addSavedPasswordListChangedListener(listener: CredentialsChangedListener):
      void;

  /**
   * Remove an observer from the list of saved passwords.
   */
  removeSavedPasswordListChangedListener(listener: CredentialsChangedListener):
      void;

  /**
   * Add an observer to the list of blocked sites.
   */
  addBlockedSitesListChangedListener(listener: BlockedSitesListChangedListener):
      void;

  /**
   * Remove an observer from the list of blocked sites.
   */
  removeBlockedSitesListChangedListener(
      listener: BlockedSitesListChangedListener): void;

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
   * Add an observer to the insecure passwords change.
   */
  addInsecureCredentialsListener(listener: CredentialsChangedListener): void;

  /**
   * Remove an observer to the insecure passwords change.
   */
  removeInsecureCredentialsListener(listener: CredentialsChangedListener): void;

  /**
   * Request the list of saved passwords.
   */
  getSavedPasswordList(): Promise<chrome.passwordsPrivate.PasswordUiEntry[]>;

  /**
   * Request grouped credentials.
   */
  getCredentialGroups(): Promise<chrome.passwordsPrivate.CredentialGroup[]>;

  /**
   * Request the list of blocked sites.
   */
  getBlockedSitesList(): Promise<BlockedSite[]>;

  /**
   * Requests the current status of the check.
   */
  getPasswordCheckStatus():
      Promise<chrome.passwordsPrivate.PasswordCheckStatus>;

  /**
   * Requests the latest information about insecure credentials.
   */
  getInsecureCredentials(): Promise<chrome.passwordsPrivate.PasswordUiEntry[]>;

  /**
   * Requests the start of the bulk password check.
   */
  startBulkPasswordCheck(): Promise<void>;

  /**
   * Records a given interaction on the Password Check page.
   */
  recordPasswordCheckInteraction(interaction: PasswordCheckInteraction): void;

  /**
   * Triggers the shortcut creation dialog.
   */
  showAddShortcutDialog(): void;

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
   * Should remove the blocked site and notify that the list has changed.
   * @param id The id for the blocked url entry being removed. No-op if |id|
   *     is not in the list.
   */
  removeBlockedSite(id: number): void;

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
   * Queries the status of any ongoing export.
   */
  requestExportProgressStatus():
      Promise<chrome.passwordsPrivate.ExportProgressStatus>;

  /**
   * Triggers the dialog for exporting passwords.
   */
  exportPasswords(): Promise<void>;

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

  /**
   * Cancels the export in progress.
   */
  cancelExportPasswords(): void;
}

/**
 * Implementation that accesses the private API.
 */
export class PasswordManagerImpl implements PasswordManagerProxy {
  addSavedPasswordListChangedListener(listener: CredentialsChangedListener) {
    chrome.passwordsPrivate.onSavedPasswordsListChanged.addListener(listener);
  }

  removeSavedPasswordListChangedListener(listener: CredentialsChangedListener) {
    chrome.passwordsPrivate.onSavedPasswordsListChanged.removeListener(
        listener);
  }

  addBlockedSitesListChangedListener(listener:
                                         BlockedSitesListChangedListener) {
    chrome.passwordsPrivate.onPasswordExceptionsListChanged.addListener(
        listener);
  }

  removeBlockedSitesListChangedListener(listener:
                                            BlockedSitesListChangedListener) {
    chrome.passwordsPrivate.onPasswordExceptionsListChanged.removeListener(
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

  addInsecureCredentialsListener(listener: CredentialsChangedListener) {
    chrome.passwordsPrivate.onInsecureCredentialsChanged.addListener(listener);
  }

  removeInsecureCredentialsListener(listener: CredentialsChangedListener) {
    chrome.passwordsPrivate.onInsecureCredentialsChanged.removeListener(
        listener);
  }

  getSavedPasswordList() {
    return chrome.passwordsPrivate.getSavedPasswordList().catch(() => []);
  }

  getCredentialGroups() {
    return chrome.passwordsPrivate.getCredentialGroups();
  }

  getBlockedSitesList() {
    return chrome.passwordsPrivate.getPasswordExceptionList().catch(() => []);
  }

  getPasswordCheckStatus() {
    return chrome.passwordsPrivate.getPasswordCheckStatus();
  }

  getInsecureCredentials() {
    return chrome.passwordsPrivate.getInsecureCredentials();
  }

  startBulkPasswordCheck() {
    return chrome.passwordsPrivate.startPasswordCheck();
  }

  recordPasswordCheckInteraction(interaction: PasswordCheckInteraction) {
    chrome.metricsPrivate.recordEnumerationValue(
        'PasswordManager.BulkCheck.UserAction', interaction,
        PasswordCheckInteraction.COUNT);
  }

  showAddShortcutDialog() {
    chrome.passwordsPrivate.showAddShortcutDialog();
  }

  requestCredentialsDetails(ids: number[]) {
    return chrome.passwordsPrivate.requestCredentialsDetails(ids);
  }

  requestPlaintextPassword(
      id: number, reason: chrome.passwordsPrivate.PlaintextReason) {
    return chrome.passwordsPrivate.requestPlaintextPassword(id, reason);
  }

  removeBlockedSite(id: number) {
    chrome.passwordsPrivate.removePasswordException(id);
  }

  muteInsecureCredential(insecureCredential:
                             chrome.passwordsPrivate.PasswordUiEntry) {
    chrome.passwordsPrivate.muteInsecureCredential(insecureCredential);
  }

  unmuteInsecureCredential(insecureCredential:
                               chrome.passwordsPrivate.PasswordUiEntry) {
    chrome.passwordsPrivate.unmuteInsecureCredential(insecureCredential);
  }

  requestExportProgressStatus() {
    return chrome.passwordsPrivate.requestExportProgressStatus();
  }

  exportPasswords() {
    return chrome.passwordsPrivate.exportPasswords();
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

  static getInstance(): PasswordManagerProxy {
    return instance || (instance = new PasswordManagerImpl());
  }

  static setInstance(obj: PasswordManagerProxy) {
    instance = obj;
  }
}

let instance: PasswordManagerProxy|null = null;
