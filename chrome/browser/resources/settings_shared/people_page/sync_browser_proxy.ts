// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import {sendWithPromise} from 'chrome://resources/js/cr.js';
// clang-format on

/**
 * @see chrome/browser/ui/webui/settings/people_handler.cc
 */
export interface StoredAccount {
  fullName?: string;
  givenName?: string;
  email: string;
  isPrimaryAccount?: boolean;  // With sign in consent level, unrelated to sync.
  avatarImage?: string;
}

/**
 * Equivalent to C++ counterpart.
 * @see chrome/browser/signin/signin_ui_util.h
 * TODO(b/336510160): Look into integrating SYNC_PAUSED value.
 */
export enum SignedInState {
  SIGNED_OUT = 0,
  SIGNED_IN = 1,
  SYNCING = 2,
  SIGNED_IN_PAUSED = 3,
  WEB_ONLY_SIGNED_IN = 4,
}

/**
 * @see chrome/browser/ui/webui/settings/people_handler.cc
 */
export interface SyncStatus {
  statusAction: StatusAction;
  disabled?: boolean;
  domain?: string;
  hasError?: boolean;
  hasPasswordsOnlyError?: boolean;
  hasUnrecoverableError?: boolean;
  managed?: boolean;
  firstSetupInProgress?: boolean;
  signedInState?: SignedInState;
  signedInUsername?: string;
  statusActionText?: string;
  secondaryButtonActionText?: string;
  statusText?: string;
  supervisedUser?: boolean;
  syncCookiesSupported?: boolean;
  syncSystemEnabled?: boolean;
}

/**
 * Must be kept in sync with the return values of getSyncErrorAction in
 * chrome/browser/ui/webui/settings/people_handler.cc
 */
export enum StatusAction {
  NO_ACTION = 'noAction',                // No action to take.
  REAUTHENTICATE = 'reauthenticate',     // User needs to reauthenticate.
  UPGRADE_CLIENT = 'upgradeClient',      // User needs to upgrade the client.
  ENTER_PASSPHRASE = 'enterPassphrase',  // User needs to enter passphrase.
  // User needs to go through key retrieval.
  RETRIEVE_TRUSTED_VAULT_KEYS = 'retrieveTrustedVaultKeys',
  CONFIRM_SYNC_SETTINGS =
      'confirmSyncSettings',  // User needs to confirm sync settings.
  SHOW_BOOKMARKS_LIMIT_HELP_ARTICLE =
      'showBookmarksLimitHelpArticle',  // User needs to see bookmarks limit
                                        // help article.
}

/**
 * The state of sync. This is the data structure sent back and forth between
 * C++ and JS. Its naming and structure is not optimal, but changing it would
 * require changes to the C++ handler, which is already functional. See
 * PeopleHandler::PushSyncPrefs() for more details.
 */
export interface SyncPrefs {
  appsManaged: boolean;
  appsRegistered: boolean;
  appsSynced: boolean;
  autofillManaged: boolean;
  autofillRegistered: boolean;
  autofillSynced: boolean;
  bookmarksManaged: boolean;
  bookmarksRegistered: boolean;
  bookmarksSynced: boolean;
  cookiesManaged: boolean;
  cookiesRegistered: boolean;
  cookiesSynced: boolean;
  customPassphraseAllowed: boolean;
  encryptAllData: boolean;
  extensionsManaged: boolean;
  extensionsRegistered: boolean;
  extensionsSynced: boolean;
  localSyncEnabled: boolean;
  passphraseRequired: boolean;
  passwordsManaged: boolean;
  passwordsRegistered: boolean;
  passwordsSynced: boolean;
  paymentsManaged: boolean;
  paymentsRegistered: boolean;
  paymentsSynced: boolean;
  preferencesManaged: boolean;
  preferencesRegistered: boolean;
  preferencesSynced: boolean;
  productComparisonManaged: boolean;
  productComparisonRegistered: boolean;
  productComparisonSynced: boolean;
  readingListManaged: boolean;
  readingListRegistered: boolean;
  readingListSynced: boolean;
  savedTabGroupsManaged: boolean;
  savedTabGroupsRegistered: boolean;
  savedTabGroupsSynced: boolean;
  syncAllDataTypes: boolean;
  tabsManaged: boolean;
  tabsRegistered: boolean;
  tabsSynced: boolean;
  themesManaged: boolean;
  themesRegistered: boolean;
  themesSynced: boolean;
  trustedVaultKeysRequired: boolean;
  typedUrlsManaged: boolean;
  typedUrlsRegistered: boolean;
  typedUrlsSynced: boolean;
  wifiConfigurationsManaged: boolean;
  wifiConfigurationsRegistered: boolean;
  wifiConfigurationsSynced: boolean;
  explicitPassphraseTime?: string;
}

/**
 * Names of the individual data type properties to be cached from
 * SyncPrefs when the user checks 'Sync All'.
 */
export const syncPrefsIndividualDataTypes: string[] = [
  'appsSynced',
  'autofillSynced',
  'bookmarksSynced',
  'cookiesSynced',
  'extensionsSynced',
  'readingListSynced',
  'passwordsSynced',
  'paymentsSynced',
  'preferencesSynced',
  'productComparisonSynced',
  'savedTabGroupsSynced',
  'tabsSynced',
  'themesSynced',
  'typedUrlsSynced',
  'wifiConfigurationsSynced',
];

// Always keep in sync with `UserSelectableType` (C++).
// LINT.IfChange(UserSelectableType)
export enum UserSelectableType {
  BOOKMARKS = 0,
  PREFERENCES = 1,
  PASSWORDS = 2,
  AUTOFILL = 3,
  THEMES = 4,
  HISTORY = 5,
  EXTENSIONS = 6,
  APPS = 7,
  READING_LIST = 8,
  TABS = 9,
  SAVED_TAB_GROUPS = 10,
  PAYMENTS = 11,
  PRODUCT_COMPARISON = 12,
  COOKIES = 13
}
// LINT.ThenChange(/components/sync/base/user_selectable_type.h:UserSelectableType)

export enum PageStatus {
  SPINNER = 'spinner',      // Before the page has loaded.
  CONFIGURE = 'configure',  // Preferences ready to be configured.
  DONE = 'done',            // Sync subpage can be closed now.
  PASSPHRASE_FAILED = 'passphraseFailed',  // Error in the passphrase.
}

// WARNING: Keep synced with chrome/browser/ui/webui/settings/people_handler.cc.
export enum TrustedVaultBannerState {
  NOT_SHOWN = 0,
  OFFER_OPT_IN = 1,
  OPTED_IN = 2,
}

// Always keep in sync with `ChromeSigninUserChoice` (C++).
export enum ChromeSigninUserChoice {
  NO_CHOICE = 0,
  ALWAYS_ASK = 1,
  SIGNIN = 2,
  DO_NOT_SIGNIN = 3,
}

export interface ChromeSigninUserChoiceInfo {
  shouldShowSettings: boolean;
  choice: ChromeSigninUserChoice;
  signedInEmail: string;
}

// LINT.IfChange(ChromeSigninAccessPoint)
export enum ChromeSigninAccessPoint {
  SETTINGS = 0,
  SETTINGS_YOUR_SAVED_INFO = 1,
}
// LINT.ThenChange(/chrome/browser/ui/webui/settings/people_handler.cc:ChromeSigninAccessPoint)

export interface SyncBrowserProxy {
  // <if expr="not is_chromeos">
  /**
   * Starts the signin process for the user. Does nothing if the user is
   * already signed in.
   */
  startSignIn(accessPoint: ChromeSigninAccessPoint): void;

  /**
   * Signs out the signed-in user.
   */
  signOut(deleteProfile: boolean): void;

  /**
   * Invalidates the Sync token without signing the user out.
   */
  pauseSync(): void;

  /**
   * Function to invoke when the account settings page with the account storage
   * per type settings is shown.
   */
  didNavigateToAccountSettingsPage(): void;

  /**
   * Sets a single type of data to sync.
   */
  setSyncDatatype(pref: UserSelectableType, value: boolean):
      Promise<PageStatus>;

  recordSigninPendingOffered(): void;
  // </if>

  // <if expr="is_chromeos">
  /**
   * Signs the user out.
   */
  attemptUserExit(): void;

  /**
   * Turns on sync for the currently logged in user. Chrome OS users are
   * always signed in to Chrome.
   */
  turnOnSync(): void;

  /**
   * Turns off sync. Does not sign out of Chrome.
   */
  turnOffSync(): void;
  // </if>

  /**
   * Starts the key retrieval process.
   */
  startKeyRetrieval(): void;

  /**
   * Displays the sync passphrase dialog for users to enter passphrase to enable
   * sync.
   */
  showSyncPassphraseDialog(): void;

  /**
   * Gets the current sync status.
   */
  getSyncStatus(): Promise<SyncStatus>;

  /**
   * Gets a list of stored accounts.
   */
  getStoredAccounts(): Promise<StoredAccount[]>;

  /**
   * Gets the current profile avatar.
   */
  getProfileAvatar(): Promise<string>;

  /**
   * Function to invoke when the sync page has been navigated to. This
   * registers the UI as the "active" sync UI so that if the user tries to
   * open another sync UI, this one will be shown instead.
   */
  didNavigateToSyncPage(): void;

  /**
   * Function to invoke when leaving the sync page so that the C++ layer can
   * be notified that the sync UI is no longer open.
   */
  didNavigateAwayFromSyncPage(didAbort: boolean): void;

  /**
   * Sets which types of data to sync.
   */
  setSyncDatatypes(syncPrefs: SyncPrefs): Promise<PageStatus>;

  /**
   * Attempts to set up a new passphrase to encrypt Sync data.
   * @return Whether the passphrase was successfully set. The call can fail, for
   *     example, if encrypting the data is disallowed.
   */
  setEncryptionPassphrase(passphrase: string): Promise<boolean>;

  /**
   * Attempts to set the passphrase to decrypt Sync data.
   * @return Whether the passphrase was successfully set. The call can fail, for
   *     example, if the passphrase is incorrect.
   */
  setDecryptionPassphrase(passphrase: string): Promise<boolean>;

  /**
   * Start syncing with an account, specified by its email.
   * |isDefaultPromoAccount| is true if |email| is the email of the default
   * account displayed in the promo.
   */
  startSyncingWithEmail(email: string, isDefaultPromoAccount: boolean): void;

  /**
   * Opens the Google Activity Controls url in a new tab.
   */
  openActivityControlsUrl(): void;

  /**
   * Function to dispatch event sync-prefs-changed even without a change.
   * This is used to decide whether we should show the link to password
   * manager in passwords section on page load.
   */
  sendSyncPrefsChanged(): void;

  /**
   * Forces a trusted-vault-banner-state-changed event to be fired.
   */
  sendTrustedVaultBannerStateChanged(): void;

  /**
   * Sets the ChromeSigninUserChoice from the signed in email after a user
   * choice on the UI.
   */
  setChromeSigninUserChoice(
      choice: ChromeSigninUserChoice, signedInEmail: string): void;

  /**
   * Gets the information related to the Chrome Signin user choice settings.
   */
  getChromeSigninUserChoiceInfo(): Promise<ChromeSigninUserChoiceInfo>;
}

export class SyncBrowserProxyImpl implements SyncBrowserProxy {
  // <if expr="not is_chromeos">
  startSignIn(accessPoint: ChromeSigninAccessPoint) {
    chrome.send('SyncSetupStartSignIn', [accessPoint]);
  }

  signOut(deleteProfile: boolean) {
    chrome.send('SyncSetupSignout', [deleteProfile]);
  }

  pauseSync() {
    chrome.send('SyncSetupPauseSync');
  }

  didNavigateToAccountSettingsPage() {
    chrome.send('ShowAccountSettingsUI');
  }

  setSyncDatatype(pref: UserSelectableType, value: boolean) {
    return sendWithPromise('SetDatatype', pref, value);
  }

  recordSigninPendingOffered() {
    chrome.send('RecordSigninPendingOffered');
  }
  // </if>

  // <if expr="is_chromeos">
  attemptUserExit() {
    chrome.send('AttemptUserExit');
  }

  turnOnSync() {
    chrome.send('TurnOnSync');
  }

  turnOffSync() {
    chrome.send('TurnOffSync');
  }
  // </if>

  startKeyRetrieval() {
    chrome.send('SyncStartKeyRetrieval');
  }

  showSyncPassphraseDialog() {
    chrome.send('SyncShowSyncPassphraseDialog');
  }

  getSyncStatus() {
    return sendWithPromise('SyncSetupGetSyncStatus');
  }

  getStoredAccounts() {
    return sendWithPromise('SyncSetupGetStoredAccounts');
  }

  getProfileAvatar() {
    return sendWithPromise('SyncSetupGetProfileAvatar');
  }

  didNavigateToSyncPage() {
    chrome.send('SyncSetupShowSetupUI');
  }

  didNavigateAwayFromSyncPage(didAbort: boolean) {
    chrome.send('SyncSetupDidClosePage', [didAbort]);
  }

  setSyncDatatypes(syncPrefs: SyncPrefs) {
    return sendWithPromise('SyncSetupSetDatatypes', JSON.stringify(syncPrefs));
  }

  setEncryptionPassphrase(passphrase: string) {
    return sendWithPromise('SyncSetupSetEncryptionPassphrase', passphrase);
  }

  setDecryptionPassphrase(passphrase: string) {
    return sendWithPromise('SyncSetupSetDecryptionPassphrase', passphrase);
  }

  startSyncingWithEmail(email: string, isDefaultPromoAccount: boolean) {
    chrome.send(
        'SyncSetupStartSyncingWithEmail', [email, isDefaultPromoAccount]);
  }

  openActivityControlsUrl() {
    chrome.metricsPrivate.recordUserAction(
        'Signin_AccountSettings_GoogleActivityControlsClicked');
  }

  sendSyncPrefsChanged() {
    chrome.send('SyncPrefsDispatch');
  }

  sendTrustedVaultBannerStateChanged() {
    chrome.send('SyncTrustedVaultBannerStateDispatch');
  }

  setChromeSigninUserChoice(
      choice: ChromeSigninUserChoice, signedInEmail: string): void {
    chrome.send('SetChromeSigninUserChoice', [choice, signedInEmail]);
  }

  getChromeSigninUserChoiceInfo(): Promise<ChromeSigninUserChoiceInfo> {
    return sendWithPromise('GetChromeSigninUserChoiceInfo');
  }

  static getInstance(): SyncBrowserProxy {
    return instance || (instance = new SyncBrowserProxyImpl());
  }

  static setInstance(obj: SyncBrowserProxy) {
    instance = obj;
  }
}

let instance: SyncBrowserProxy|null = null;
