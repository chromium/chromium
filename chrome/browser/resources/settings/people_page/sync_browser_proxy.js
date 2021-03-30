// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import {addSingletonGetter, sendWithPromise} from 'chrome://resources/js/cr.m.js';
// clang-format on

  /**
   * @typedef {{fullName: (string|undefined),
   *            givenName: (string|undefined),
   *            email: string,
   *            avatarImage: (string|undefined)}}
   * @see chrome/browser/ui/webui/settings/people_handler.cc
   */
  export let StoredAccount;

  /**
   * @typedef {{childUser: (boolean|undefined),
   *            disabled: (boolean|undefined),
   *            domain: (string|undefined),
   *            hasError: (boolean|undefined),
   *            hasPasswordsOnlyError: (boolean|undefined),
   *            hasUnrecoverableError: (boolean|undefined),
   *            managed: (boolean|undefined),
   *            firstSetupInProgress: (boolean|undefined),
   *            signedIn: (boolean|undefined),
   *            signedInUsername: (string|undefined),
   *            statusAction: (!StatusAction),
   *            statusActionText: (string|undefined),
   *            statusText: (string|undefined),
   *            supervisedUser: (boolean|undefined),
   *            syncSystemEnabled: (boolean|undefined)}}
   * @see chrome/browser/ui/webui/settings/people_handler.cc
   */
  export let SyncStatus;

  /**
   * Must be kept in sync with the return values of getSyncErrorAction in
   * chrome/browser/ui/webui/settings/people_handler.cc
   * @enum {string}
   */
  export const StatusAction = {
    NO_ACTION: 'noAction',             // No action to take.
    REAUTHENTICATE: 'reauthenticate',  // User needs to reauthenticate.
    SIGNOUT_AND_SIGNIN:
        'signOutAndSignIn',               // User needs to sign out and sign in.
    UPGRADE_CLIENT: 'upgradeClient',      // User needs to upgrade the client.
    ENTER_PASSPHRASE: 'enterPassphrase',  // User needs to enter passphrase.
    // User needs to go through key retrieval.
    RETRIEVE_TRUSTED_VAULT_KEYS: 'retrieveTrustedVaultKeys',
    CONFIRM_SYNC_SETTINGS:
        'confirmSyncSettings',  // User needs to confirm sync settings.
  };

  /**
   * The state of sync. This is the data structure sent back and forth between
   * C++ and JS. Its naming and structure is not optimal, but changing it would
   * require changes to the C++ handler, which is already functional. See
   * PeopleHandler::PushSyncPrefs() for more details.
   * @typedef {{
   *   appsRegistered: boolean,
   *   appsSynced: boolean,
   *   autofillRegistered: boolean,
   *   autofillSynced: boolean,
   *   bookmarksRegistered: boolean,
   *   bookmarksSynced: boolean,
   *   encryptAllData: boolean,
   *   encryptAllDataAllowed: boolean,
   *   enterPassphraseBody: (string|undefined),
   *   extensionsRegistered: boolean,
   *   extensionsSynced: boolean,
   *   fullEncryptionBody: string,
   *   passphraseRequired: boolean,
   *   passwordsRegistered: boolean,
   *   passwordsSynced: boolean,
   *   paymentsIntegrationEnabled: boolean,
   *   preferencesRegistered: boolean,
   *   preferencesSynced: boolean,
   *   readingListRegistered: boolean,
   *   readingListSynced: boolean,
   *   syncAllDataTypes: boolean,
   *   tabsRegistered: boolean,
   *   tabsSynced: boolean,
   *   themesRegistered: boolean,
   *   themesSynced: boolean,
   *   trustedVaultKeysRequired: boolean,
   *   typedUrlsRegistered: boolean,
   *   typedUrlsSynced: boolean,
   *   wifiConfigurationsRegistered: boolean,
   *   wifiConfigurationsSynced: boolean,
   * }}
   */
  export let SyncPrefs;

  /** @enum {string} */
  export const PageStatus = {
    SPINNER: 'spinner',      // Before the page has loaded.
    CONFIGURE: 'configure',  // Preferences ready to be configured.
    DONE: 'done',            // Sync subpage can be closed now.
    PASSPHRASE_FAILED: 'passphraseFailed',  // Error in the passphrase.
  };

  /**
   * Key to be used with localStorage.
   * @type {string}
   */
  const PROMO_IMPRESSION_COUNT_KEY = 'signin-promo-count';

  /** @interface */
  export class SyncBrowserProxy {
    // <if expr="not chromeos">
    /**
     * Starts the signin process for the user. Does nothing if the user is
     * already signed in.
     */
    startSignIn() {}

    /**
     * Signs out the signed-in user.
     * @param {boolean} deleteProfile
     */
    signOut(deleteProfile) {}

    /**
     * Invalidates the Sync token without signing the user out.
     */
    pauseSync() {}
    // </if>

    /**
     * @return {number} the number of times the sync account promo was shown.
     */
    getPromoImpressionCount() {}

    /**
     * Increment the number of times the sync account promo was shown.
     */
    incrementPromoImpressionCount() {}

    // <if expr="chromeos">
    /**
     * Signs the user out.
     */
    attemptUserExit() {}

    /**
     * Turns on sync for the currently logged in user. Chrome OS users are
     * always signed in to Chrome.
     */
    turnOnSync() {}

    /**
     * Turns off sync. Does not sign out of Chrome.
     */
    turnOffSync() {}
    // </if>

    /**
     * Starts the key retrieval process.
     */
    startKeyRetrieval() {}

    /**
     * Gets the current sync status.
     * @return {!Promise<!SyncStatus>}
     */
    getSyncStatus() {}

    /**
     * Gets a list of stored accounts.
     * @return {!Promise<!Array<!StoredAccount>>}
     */
    getStoredAccounts() {}

    /**
     * Function to invoke when the sync page has been navigated to. This
     * registers the UI as the "active" sync UI so that if the user tries to
     * open another sync UI, this one will be shown instead.
     */
    didNavigateToSyncPage() {}

    /**
     * Function to invoke when leaving the sync page so that the C++ layer can
     * be notified that the sync UI is no longer open.
     * @param {boolean} didAbort
     */
    didNavigateAwayFromSyncPage(didAbort) {}

    /**
     * Sets which types of data to sync.
     * @param {!SyncPrefs} syncPrefs
     * @return {!Promise<!PageStatus>}
     */
    setSyncDatatypes(syncPrefs) {}

    /**
     * Attempts to set up a new passphrase to encrypt Sync data.
     * @param {string} passphrase
     * @return {!Promise<boolean>} Whether the passphrase was successfully set.
     * The call can fail, for example, if encrypting the data is disallowed.
     */
    setEncryptionPassphrase(passphrase) {}

    /**
     * Attempts to set the passphrase to decrypt Sync data.
     * @param {string} passphrase
     * @return {!Promise<boolean>} Whether the passphrase was successfully set.
     * The call can fail, for example, if the passphrase is incorrect.
     */
    setDecryptionPassphrase(passphrase) {}

    /**
     * Start syncing with an account, specified by its email.
     * |isDefaultPromoAccount| is true if |email| is the email of the default
     * account displayed in the promo.
     * @param {string} email
     * @param {boolean} isDefaultPromoAccount
     */
    startSyncingWithEmail(email, isDefaultPromoAccount) {}

    /**
     * Opens the Google Activity Controls url in a new tab.
     */
    openActivityControlsUrl() {}

    /**
     * Function to dispatch event sync-prefs-changed even without a change.
     * This is used to decide whether we should show the link to password
     * manager in passwords section on page load.
     */
    sendSyncPrefsChanged() {}
  }

  /**
   * @implements {SyncBrowserProxy}
   */
  export class SyncBrowserProxyImpl {
    // <if expr="not chromeos">
    /** @override */
    startSignIn() {
      chrome.send('SyncSetupStartSignIn');
    }

    /** @override */
    signOut(deleteProfile) {
      chrome.send('SyncSetupSignout', [deleteProfile]);
    }

    /** @override */
    pauseSync() {
      chrome.send('SyncSetupPauseSync');
    }
    // </if>

    /** @override */
    getPromoImpressionCount() {
      return parseInt(
                 window.localStorage.getItem(PROMO_IMPRESSION_COUNT_KEY), 10) ||
          0;
    }

    /** @override */
    incrementPromoImpressionCount() {
      window.localStorage.setItem(
          PROMO_IMPRESSION_COUNT_KEY,
          (this.getPromoImpressionCount() + 1).toString());
    }

    // <if expr="chromeos">
    /** @override */
    attemptUserExit() {
      return chrome.send('AttemptUserExit');
    }

    /** @override */
    turnOnSync() {
      return chrome.send('TurnOnSync');
    }

    /** @override */
    turnOffSync() {
      return chrome.send('TurnOffSync');
    }
    // </if>

    /** @override */
    startKeyRetrieval() {
      chrome.send('SyncStartKeyRetrieval');
    }

    /** @override */
    getSyncStatus() {
      return sendWithPromise('SyncSetupGetSyncStatus');
    }

    /** @override */
    getStoredAccounts() {
      return sendWithPromise('SyncSetupGetStoredAccounts');
    }

    /** @override */
    didNavigateToSyncPage() {
      chrome.send('SyncSetupShowSetupUI');
    }

    /** @override */
    didNavigateAwayFromSyncPage(didAbort) {
      chrome.send('SyncSetupDidClosePage', [didAbort]);
    }

    /** @override */
    setSyncDatatypes(syncPrefs) {
      return sendWithPromise(
          'SyncSetupSetDatatypes', JSON.stringify(syncPrefs));
    }

    /** @override */
    setEncryptionPassphrase(passphrase) {
      return sendWithPromise('SyncSetupSetEncryptionPassphrase', passphrase);
    }

    /** @override */
    setDecryptionPassphrase(passphrase) {
      return sendWithPromise('SyncSetupSetDecryptionPassphrase', passphrase);
    }

    /** @override */
    startSyncingWithEmail(email, isDefaultPromoAccount) {
      chrome.send(
          'SyncSetupStartSyncingWithEmail', [email, isDefaultPromoAccount]);
    }

    /** @override */
    openActivityControlsUrl() {
      chrome.metricsPrivate.recordUserAction(
          'Signin_AccountSettings_GoogleActivityControlsClicked');
    }

    /** @override */
    sendSyncPrefsChanged() {
      chrome.send('SyncPrefsDispatch');
    }
  }

  addSingletonGetter(SyncBrowserProxyImpl);

