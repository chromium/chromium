// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview PasswordManagerProxy is an abstraction over
 * chrome.passwordsPrivate which facilitates testing.
 */

export type BlockedSite = chrome.passwordsPrivate.ExceptionEntry;

export type AccountStorageEnabledStateChangedListener =
    (enabledState: boolean) => void;
export type CredentialsChangedListener =
    (credentials: chrome.passwordsPrivate.PasswordUiEntry[]) => void;
export type PasswordCheckStatusChangedListener =
    (status: chrome.passwordsPrivate.PasswordCheckStatus) => void;
export type BlockedSitesListChangedListener = (entries: BlockedSite[]) => void;
export type PasswordsFileExportProgressListener =
    (progress: chrome.passwordsPrivate.PasswordExportProgress) => void;
export type PasswordManagerAuthTimeoutListener = () => void;

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
 * Should be kept in sync with
 * |password_manager::metrics_util::PasswordViewPageInteractions|.
 * These values are persisted to logs. Entries should not be renumbered and
 * numeric values should never be reused.
 */
export enum PasswordViewPageInteractions {
  CREDENTIAL_ROW_CLICKED = 0,
  CREDENTIAL_FOUND = 1,
  CREDENTIAL_NOT_FOUND = 2,
  USERNAME_COPY_BUTTON_CLICKED = 3,
  PASSWORD_COPY_BUTTON_CLICKED = 4,
  PASSWORD_SHOW_BUTTON_CLICKED = 5,
  PASSWORD_EDIT_BUTTON_CLICKED = 6,
  PASSWORD_DELETE_BUTTON_CLICKED = 7,
  CREDENTIAL_EDITED = 8,
  TIMED_OUT_IN_EDIT_DIALOG = 9,
  TIMED_OUT_IN_VIEW_PAGE = 10,
  CREDENTIAL_REQUESTED_BY_URL = 11,
  PASSKEY_DISPLAY_NAME_COPY_BUTTON_CLICKED = 12,
  PASSKEY_DELETE_BUTTON_CLICKED = 13,
  PASSKEY_EDIT_BUTTON_CLICKED = 14,
  // Must be last.
  COUNT = 15,
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
   * Requests the latest information about insecure credentials.
   */
  getCredentialsWithReusedPassword():
      Promise<chrome.passwordsPrivate.PasswordUiEntryList[]>;

  /**
   * Requests the start of the bulk password check.
   */
  startBulkPasswordCheck(): Promise<void>;

  /**
   * Records a given interaction on the Password Check page.
   */
  recordPasswordCheckInteraction(interaction: PasswordCheckInteraction): void;

  /**
   * Records a given interaction on the Password details page.
   */
  recordPasswordViewInteraction(interaction: PasswordViewPageInteractions):
      void;

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
   * Saves a new password entry described by the given |options|.
   * @param options Details about a new password and storage to be used.
   * @return A promise that resolves when the new entry is added.
   */
  addPassword(options: chrome.passwordsPrivate.AddPasswordOptions):
      Promise<void>;

  /**
   * Fetches family members (password share recipients).
   * @return A promise that resolves the FamilyFetchResults.
   */
  fetchFamilyMembers(): Promise<chrome.passwordsPrivate.FamilyFetchResults>;

  /**
   * Sends sharing invitations to the recipients.
   * @param id The id of the password entry to be shared.
   * @param recipients The list of selected recipients.
   */
  sharePassword(
      id: number, recipients: chrome.passwordsPrivate.RecipientInfo[]): void;

  /**
   * Updates the given credential. Not all parameters can be updated.
   * @param credential the credential to update.
   * @return A promise that resolves if the credential was found and updated,
   *     rejects otherwise.
   */
  changeCredential(credential: chrome.passwordsPrivate.PasswordUiEntry):
      Promise<void>;

  /**
   * Should remove the credential and notify that the list has changed.
   * @param id The id for the credential being removed. No-op if |id| is not in
   *     the list.
   * @param fromStores The store from which credential should be removed.
   */
  removeCredential(
      id: number, fromStores: chrome.passwordsPrivate.PasswordStoreSet): void;

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
   * Should undo the last saved password or exception removal and notify that
   * the list has changed.
   */
  undoRemoveSavedPasswordOrException(): void;

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
   * Switches Biometric authentication before filling state after
   * successful authentication.
   * @return A promise that resolves with authentication result.
   */
  switchBiometricAuthBeforeFillingState(): Promise<boolean>;

  /**
   * Shows the file with the exported passwords in the OS shell.
   */
  showExportedFileInShell(filePath: string): void;

  /**
   * Requests whether the given |url| meets the requirements to save a password
   * for it (e.g. valid, has proper scheme etc.).
   * @return A promise that resolves to the corresponding URLCollection on
   *     success and to null otherwise.
   */
  getUrlCollection(url: string):
      Promise<chrome.passwordsPrivate.UrlCollection|null>;

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
   * Requests extension of authentication validity.
   */
  extendAuthValidity(): void;

  /**
   * Add an observer to the account storage enabled state.
   */
  addAccountStorageEnabledStateListener(
      listener: AccountStorageEnabledStateChangedListener): void;

  /**
   * Remove an observer to the account storage enabled state.
   */
  removeAccountStorageEnabledStateListener(
      listener: AccountStorageEnabledStateChangedListener): void;

  /**
   * Requests the account-storage enabled state of the current user.
   * @return A promise that resolves to the enabled state.
   */
  isAccountStorageEnabled(): Promise<boolean>;

  /**
   * Triggers the enabling/disabling flow for the account storage.
   * @param enabled Whether the user wants to enable or disable.
   */
  setAccountStorageEnabled(enabled: boolean): void;

  /**
   * Requests whether the account store is a default location for saving
   * passwords. False means the device store is a default one. Must be called
   * when account storage is enabled.
   * @return A promise that resolves to whether the account store is default.
   */
  isAccountStoreDefault(): Promise<boolean>;

  /**
   * Moves a list of passwords from the device to the account
   * @param ids The ids for the password entries being moved.
   */
  movePasswordsToAccount(ids: number[]): void;

  /** Dismiss the menu notifications for the Safety Hub password module. */
  dismissSafetyHubPasswordMenuNotification(): void;

  /** Starts the flow for changing Password Manager PIN. */
  changePasswordManagerPin(): Promise<boolean>;

  /** Checks whether changing the Password Manager PIN is possible. */
  isPasswordManagerPinAvailable(): Promise<boolean>;

  /**
   * Starts the flow for disconnecting the Cloud Authenticator
   * (Passkeys Enclave).
   */
  disconnectCloudAuthenticator(): Promise<boolean>;

  /**
   * Checks whether the Chrome client is connected to the Cloud Authenticator
   * (Passkeys Enclave).
   */
  isConnectedToCloudAuthenticator(): Promise<boolean>;

  /**
   * Deletes all password manager data (passwords, passkeys, etc.)
   */
  deleteAllPasswordManagerData(): Promise<boolean>;
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

  getCredentialsWithReusedPassword() {
    return chrome.passwordsPrivate.getCredentialsWithReusedPassword();
  }

  startBulkPasswordCheck() {
    return chrome.passwordsPrivate.startPasswordCheck();
  }

  recordPasswordCheckInteraction(interaction: PasswordCheckInteraction) {
    chrome.metricsPrivate.recordEnumerationValue(
        'PasswordManager.BulkCheck.UserAction', interaction,
        PasswordCheckInteraction.COUNT);
  }

  recordPasswordViewInteraction(interaction: PasswordViewPageInteractions) {
    chrome.metricsPrivate.recordEnumerationValue(
        'PasswordManager.PasswordViewPage.UserActions', interaction,
        PasswordViewPageInteractions.COUNT);
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

  addPassword(options: chrome.passwordsPrivate.AddPasswordOptions) {
    return chrome.passwordsPrivate.addPassword(options);
  }

  changeCredential(credential: chrome.passwordsPrivate.PasswordUiEntry) {
    return chrome.passwordsPrivate.changeCredential(credential);
  }

  removeCredential(
      id: number, fromStores: chrome.passwordsPrivate.PasswordStoreSet) {
    chrome.passwordsPrivate.removeCredential(id, fromStores);
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

  undoRemoveSavedPasswordOrException() {
    chrome.passwordsPrivate.undoRemoveSavedPasswordOrException();
  }

  fetchFamilyMembers() {
    return chrome.passwordsPrivate.fetchFamilyMembers();
  }

  sharePassword(
      id: number, recipients: chrome.passwordsPrivate.RecipientInfo[]) {
    chrome.passwordsPrivate.sharePassword(id, recipients);
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

  switchBiometricAuthBeforeFillingState() {
    return chrome.passwordsPrivate.switchBiometricAuthBeforeFillingState();
  }

  showExportedFileInShell(filePath: string) {
    chrome.passwordsPrivate.showExportedFileInShell(filePath);
  }

  getUrlCollection(url: string) {
    return chrome.passwordsPrivate.getUrlCollection(url);
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

  extendAuthValidity() {
    chrome.passwordsPrivate.extendAuthValidity();
  }

  addAccountStorageEnabledStateListener(
      listener: AccountStorageEnabledStateChangedListener) {
    chrome.passwordsPrivate.onAccountStorageEnabledStateChanged.addListener(
        listener);
  }

  removeAccountStorageEnabledStateListener(
      listener: AccountStorageEnabledStateChangedListener) {
    chrome.passwordsPrivate.onAccountStorageEnabledStateChanged.removeListener(
        listener);
  }

  isAccountStorageEnabled() {
    return chrome.passwordsPrivate.isAccountStorageEnabled();
  }

  setAccountStorageEnabled(enabled: boolean) {
    chrome.passwordsPrivate.setAccountStorageEnabled(enabled);
  }

  isAccountStoreDefault() {
    return chrome.passwordsPrivate.isAccountStoreDefault();
  }

  movePasswordsToAccount(ids: number[]) {
    chrome.passwordsPrivate.movePasswordsToAccount(ids);
  }

  dismissSafetyHubPasswordMenuNotification() {
    chrome.send('dismissSafetyHubPasswordMenuNotification');
  }

  changePasswordManagerPin() {
    return chrome.passwordsPrivate.changePasswordManagerPin();
  }

  isPasswordManagerPinAvailable() {
    return chrome.passwordsPrivate.isPasswordManagerPinAvailable();
  }

  disconnectCloudAuthenticator() {
    return chrome.passwordsPrivate.disconnectCloudAuthenticator();
  }

  isConnectedToCloudAuthenticator() {
    return chrome.passwordsPrivate.isConnectedToCloudAuthenticator();
  }

  deleteAllPasswordManagerData() {
    return chrome.passwordsPrivate.deleteAllPasswordManagerData();
  }

  static getInstance(): PasswordManagerProxy {
    return instance || (instance = new PasswordManagerImpl());
  }

  static setInstance(obj: PasswordManagerProxy) {
    instance = obj;
  }
}

let instance: PasswordManagerProxy|null = null;
