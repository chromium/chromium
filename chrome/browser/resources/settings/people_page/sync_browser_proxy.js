// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview A helper object used from the the People section to get the
 * status of the sync backend and user preferences on what data to sync. Used
 * for both Chrome browser and ChromeOS.
 */
cr.exportPath('settings');

/**
 * @typedef {{fullName: (string|undefined),
 *            givenName: (string|undefined),
 *            email: string,
 *            avatarImage: (string|undefined)}}
 * @see chrome/browser/ui/webui/settings/people_handler.cc
 */
settings.StoredAccount;

/**
 * @typedef {{childUser: (boolean|undefined),
 *            disabled: (boolean|undefined),
 *            domain: (string|undefined),
 *            hasError: (boolean|undefined),
 *            hasUnrecoverableError: (boolean|undefined),
 *            managed: (boolean|undefined),
 *            firstSetupInProgress: (boolean|undefined),
 *            signedIn: (boolean|undefined),
 *            signedInUsername: (string|undefined),
 *            signinAllowed: (boolean|undefined),
 *            statusAction: (!settings.StatusAction),
 *            statusActionText: (string|undefined),
 *            statusText: (string|undefined),
 *            supervisedUser: (boolean|undefined),
 *            syncSystemEnabled: (boolean|undefined)}}
 * @see chrome/browser/ui/webui/settings/people_handler.cc
 */
settings.SyncStatus;


/**
 * Must be kept in sync with the return values of getSyncErrorAction in
 * chrome/browser/ui/webui/settings/people_handler.cc
 * @enum {string}
 */
settings.StatusAction = {
  NO_ACTION: 'noAction',             // No action to take.
  REAUTHENTICATE: 'reauthenticate',  // User needs to reauthenticate.
  SIGNOUT_AND_SIGNIN:
      'signOutAndSignIn',               // User needs to sign out and sign in.
  UPGRADE_CLIENT: 'upgradeClient',      // User needs to upgrade the client.
  ENTER_PASSPHRASE: 'enterPassphrase',  // User needs to enter passphrase.
  CONFIRM_SYNC_SETTINGS:
      'confirmSyncSettings',  // User needs to confirm sync settings.
};

/**
 * The state of sync. This is the data structure sent back and forth between
 * C++ and JS. Its naming and structure is not optimal, but changing it would
 * require changes to the C++ handler, which is already functional.
 * @typedef {{
 *   appsEnforced: boolean,
 *   appsRegistered: boolean,
 *   appsSynced: boolean,
 *   autofillEnforced: boolean,
 *   autofillRegistered: boolean,
 *   autofillSynced: boolean,
 *   bookmarksEnforced: boolean,
 *   bookmarksRegistered: boolean,
 *   bookmarksSynced: boolean,
 *   encryptAllData: boolean,
 *   encryptAllDataAllowed: boolean,
 *   enterPassphraseBody: (string|undefined),
 *   extensionsEnforced: boolean,
 *   extensionsRegistered: boolean,
 *   extensionsSynced: boolean,
 *   fullEncryptionBody: string,
 *   passphrase: (string|undefined),
 *   passphraseRequired: boolean,
 *   passwordsEnforced: boolean,
 *   passwordsRegistered: boolean,
 *   passwordsSynced: boolean,
 *   paymentsIntegrationEnabled: boolean,
 *   preferencesEnforced: boolean,
 *   preferencesRegistered: boolean,
 *   preferencesSynced: boolean,
 *   setNewPassphrase: (boolean|undefined),
 *   syncAllDataTypes: boolean,
 *   tabsEnforced: boolean,
 *   tabsRegistered: boolean,
 *   tabsSynced: boolean,
 *   themesEnforced: boolean,
 *   themesRegistered: boolean,
 *   themesSynced: boolean,
 *   typedUrlsEnforced: boolean,
 *   typedUrlsRegistered: boolean,
 *   typedUrlsSynced: boolean,
 *   wifiConfigurationsEnforced: boolean,
 *   wifiConfigurationsRegistered: boolean,
 *   wifiConfigurationsSynced: boolean,
 * }}
 */
settings.SyncPrefs;

/**
 * @enum {string}
 */
settings.PageStatus = {
  SPINNER: 'spinner',                     // Before the page has loaded.
  CONFIGURE: 'configure',                 // Preferences ready to be configured.
  TIMEOUT: 'timeout',                     // Preferences loading has timed out.
  DONE: 'done',                           // Sync subpage can be closed now.
  PASSPHRASE_FAILED: 'passphraseFailed',  // Error in the passphrase.
};

cr.define('settings', function() {
  /**
   * Key to be used with localStorage.
   * @type {string}
   */
  const PROMO_IMPRESSION_COUNT_KEY = 'signin-promo-count';

  /** @interface */
  class SyncBrowserProxy {
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

    /**
     * @return {number} the number of times the sync account promo was shown.
     */
    getPromoImpressionCount() {}

    /**
     * Increment the number of times the sync account promo was shown.
     */
    incrementPromoImpressionCount() {}

    // </if>

    // <if expr="chromeos">
    /**
     * Signs the user out.
     */
    attemptUserExit() {}

    // </if>

    /**
     * Gets the current sync status.
     * @return {!Promise<!settings.SyncStatus>}
     */
    getSyncStatus() {}

    /**
     * Gets a list of stored accounts.
     * @return {!Promise<!Array<!settings.StoredAccount>>}
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
     * @param {!settings.SyncPrefs} syncPrefs
     * @return {!Promise<!settings.PageStatus>}
     */
    setSyncDatatypes(syncPrefs) {}

    /**
     * Sets the sync encryption options.
     * @param {!settings.SyncPrefs} syncPrefs
     * @return {!Promise<!settings.PageStatus>}
     */
    setSyncEncryption(syncPrefs) {}

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
   * @implements {settings.SyncBrowserProxy}
   */
  class SyncBrowserProxyImpl {
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

    // </if>
    // <if expr="chromeos">
    /** @override */
    attemptUserExit() {
      return chrome.send('AttemptUserExit');
    }
    // </if>

    /** @override */
    getSyncStatus() {
      return cr.sendWithPromise('SyncSetupGetSyncStatus');
    }

    /** @override */
    getStoredAccounts() {
      return cr.sendWithPromise('SyncSetupGetStoredAccounts');
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
      return cr.sendWithPromise(
          'SyncSetupSetDatatypes', JSON.stringify(syncPrefs));
    }

    /** @override */
    setSyncEncryption(syncPrefs) {
      return cr.sendWithPromise(
          'SyncSetupSetEncryption', JSON.stringify(syncPrefs));
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

  cr.addSingletonGetter(SyncBrowserProxyImpl);

  return {
    SyncBrowserProxy: SyncBrowserProxy,
    SyncBrowserProxyImpl: SyncBrowserProxyImpl,
  };
});
