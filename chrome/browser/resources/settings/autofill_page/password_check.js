// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_action_menu/cr_action_menu.m.js';
import 'chrome://resources/cr_elements/cr_button/cr_button.m.js';
import '../settings_shared_css.m.js';
import 'chrome://resources/cr_elements/icons.m.js';
import 'chrome://resources/cr_elements/shared_style_css.m.js';
import 'chrome://resources/polymer/v3_0/iron-flex-layout/iron-flex-layout-classes.js';
import 'chrome://resources/polymer/v3_0/iron-icon/iron-icon.js';
import 'chrome://resources/polymer/v3_0/iron-list/iron-list.js';
import 'chrome://resources/polymer/v3_0/paper-spinner/paper-spinner-lite.js';
import '../route.js';
import '../prefs/prefs.m.js';
import './password_check_edit_dialog.js';
import './password_check_edit_disclaimer_dialog.js';
import './password_check_list_item.js';
import './password_remove_confirmation_dialog.js';

import {assert, assertNotReached} from 'chrome://resources/js/assert.m.js';
import {focusWithoutInk} from 'chrome://resources/js/cr/ui/focus_without_ink.m.js';
import {I18nBehavior} from 'chrome://resources/js/i18n_behavior.m.js';
import {WebUIListenerBehavior} from 'chrome://resources/js/web_ui_listener_behavior.m.js';
import {html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {loadTimeData} from '../i18n_setup.js';
import {SyncBrowserProxyImpl, SyncPrefs, SyncStatus} from '../people_page/sync_browser_proxy.m.js';
import {PrefsBehavior} from '../prefs/prefs_behavior.m.js';
import {Route, Router, RouteObserverBehavior} from '../router.m.js';
import {routes} from '../route.js';

import {PasswordCheckBehavior} from './password_check_behavior.js';
import {PasswordManagerImpl, PasswordManagerProxy} from './password_manager_proxy.js';
// <if expr="chromeos">
import '../controls/password_prompt_dialog.m.js';
import {getDeepActiveElement} from 'chrome://resources/js/util.m.js';
import {BlockingRequestManager} from './blocking_request_manager.js';
// </if>

const CheckState = chrome.passwordsPrivate.PasswordCheckState;

Polymer({
  is: 'settings-password-check',

  _template: html`{__html_template__}`,

  behaviors: [
    I18nBehavior,
    PasswordCheckBehavior,
    PrefsBehavior,
    RouteObserverBehavior,
    WebUIListenerBehavior,
  ],

  properties: {
    // <if expr="not chromeos">
    /** @private */
    storedAccounts_: Array,
    // </if>

    /** @private */
    title_: {
      type: String,
      computed: 'computeTitle_(status, canUsePasswordCheckup_)',
    },

    /** @private */
    isSignedOut_: {
      type: Boolean,
      computed: 'computeIsSignedOut_(syncStatus_, storedAccounts_)',
    },

    /** @private */
    isSyncingPasswords_: {
      type: Boolean,
      computed: 'computeIsSyncingPasswords_(syncPrefs_, syncStatus_)',
    },

    canUsePasswordCheckup_: {
      type: Boolean,
      computed: 'computeCanUsePasswordCheckup_(syncPrefs_, syncStatus_)',
    },

    /** @private */
    isButtonHidden_: {
      type: Boolean,
      computed: 'computeIsButtonHidden_(status, isSignedOut_, isInitialStatus)',
    },

    /** @private {SyncPrefs} */
    syncPrefs_: Object,

    /** @private {SyncStatus} */
    syncStatus_: Object,

    /** @private */
    showPasswordEditDialog_: Boolean,

    /** @private */
    showPasswordRemoveDialog_: Boolean,

    /** @private */
    showPasswordEditDisclaimer_: Boolean,

    /**
     * The password that the user is interacting with now.
     * @private {?PasswordManagerProxy.InsecureCredential}
     */
    activePassword_: Object,

    /** @private */
    showCompromisedCredentialsBody_: {
      type: Boolean,
      computed: 'computeShowCompromisedCredentialsBody_(isSignedOut_, ' +
          'leakedPasswords, passwordsWeaknessCheckEnabled)',
    },

    /** @private */
    showNoCompromisedPasswordsLabel_: {
      type: Boolean,
      computed: 'computeShowNoCompromisedPasswordsLabel_(syncStatus_, ' +
          'prefs.*, status, leakedPasswords, passwordsWeaknessCheckEnabled)',
    },

    /** @private */
    showHideMenuTitle_: {
      type: String,
      computed: 'computeShowHideMenuTitle(activePassword_)',
    },

    /**
     * The ids of insecure credentials for which user clicked "Change Password"
     * button
     * @private
     */
    clickedChangePasswordIds_: {
      type: Object,
      value: new Set(),
    },

    // <if expr="chromeos">
    /** @private */
    showPasswordPromptDialog_: Boolean,

    /** @private {BlockingRequestManager} */
    tokenRequestManager_: Object,
    // </if>
  },

  /**
   * A stack of the elements that triggered dialog to open and should therefore
   * receive focus when that dialog is closed. The bottom of the stack is the
   * element that triggered the earliest open dialog and top of the stack is the
   * element that triggered the most recent (i.e. active) dialog. If no dialog
   * is open, the stack is empty.
   * @private {?Array<!HTMLElement>}
   */
  activeDialogAnchorStack_: null,

  /**
   * The password_check_list_item that the user is interacting with now.
   * @private {?EventTarget}
   */
  activeListItem_: null,

  /** @private {boolean} */
  startCheckAutomaticallySucceeded: false,

  /**
   * Observer for saved passwords to update startCheckAutomaticallySucceeded
   * once they are changed. It's needed to run password check on navigation
   * again once passwords changed.
   * @private {?function(!Array<PasswordManagerProxy.PasswordUiEntry>):void}
   */
  setSavedPasswordsListener_: null,

  /** @override */
  attached() {
    // <if expr="chromeos">
    // If the user's account supports the password check, an auth token will be
    // required in order for them to view or export passwords. Otherwise there
    // is no additional security so |tokenRequestManager_| will immediately
    // resolve requests.
    this.tokenRequestManager_ =
        loadTimeData.getBoolean('userCannotManuallyEnterPassword') ?
        new BlockingRequestManager() :
        new BlockingRequestManager(this.openPasswordPromptDialog_.bind(this));

    // </if>
    this.activeDialogAnchorStack_ = [];

    const setSavedPasswordsListener = list => {
      this.startCheckAutomaticallySucceeded = false;
    };
    this.setSavedPasswordsListener_ = setSavedPasswordsListener;
    this.passwordManager.addSavedPasswordListChangedListener(
        setSavedPasswordsListener);

    // Set the manager. These can be overridden by tests.
    const syncBrowserProxy = SyncBrowserProxyImpl.getInstance();

    const syncStatusChanged = syncStatus => this.syncStatus_ = syncStatus;
    const syncPrefsChanged = syncPrefs => this.syncPrefs_ = syncPrefs;

    // Listen for changes.
    this.addWebUIListener('sync-status-changed', syncStatusChanged);
    this.addWebUIListener('sync-prefs-changed', syncPrefsChanged);

    // Request initial data.
    syncBrowserProxy.getSyncStatus().then(syncStatusChanged);
    syncBrowserProxy.sendSyncPrefsChanged();

    // For non-ChromeOS, also check whether accounts are available.
    // <if expr="not chromeos">
    const storedAccountsChanged = accounts => this.storedAccounts_ = accounts;
    syncBrowserProxy.getStoredAccounts().then(storedAccountsChanged);
    this.addWebUIListener('stored-accounts-updated', storedAccountsChanged);
    // </if>
  },

  /** @override */
  detached() {
    this.passwordManager.removeSavedPasswordListChangedListener(
        assert(this.setSavedPasswordsListener_));
  },

  /**
   * Tries to start bulk password check on page open if instructed to do so and
   * didn't start successfully before
   * @private
   */
  currentRouteChanged(currentRoute) {
    const router = Router.getInstance();

    if (currentRoute.path === routes.CHECK_PASSWORDS.path &&
        !this.startCheckAutomaticallySucceeded &&
        router.getQueryParameters().get('start') === 'true') {
      this.passwordManager.recordPasswordCheckInteraction(
          PasswordManagerProxy.PasswordCheckInteraction
              .START_CHECK_AUTOMATICALLY);
      this.passwordManager.startBulkPasswordCheck().then(
          () => {
            this.startCheckAutomaticallySucceeded = true;
          },
          error => {
            // Catching error
          });
    }
    // Requesting status on navigation to update elapsedTimeSinceLastCheck
    this.passwordManager.getPasswordCheckStatus().then(
        status => this.status = status);
  },

  /**
   * Start/Stop bulk password check.
   * @private
   */
  onPasswordCheckButtonClick_() {
    switch (this.status.state) {
      case CheckState.RUNNING:
        this.passwordManager.recordPasswordCheckInteraction(
            PasswordManagerProxy.PasswordCheckInteraction.STOP_CHECK);
        this.passwordManager.stopBulkPasswordCheck();
        return;
      case CheckState.IDLE:
      case CheckState.CANCELED:
      case CheckState.OFFLINE:
      case CheckState.OTHER_ERROR:
        this.passwordManager.recordPasswordCheckInteraction(
            PasswordManagerProxy.PasswordCheckInteraction.START_CHECK_MANUALLY);
        this.passwordManager.startBulkPasswordCheck();
        return;
      case CheckState.SIGNED_OUT:
        // Runs the startBulkPasswordCheck to check passwords for weakness that
        // works for both sign in and sign out users.
        this.passwordManager.recordPasswordCheckInteraction(
            PasswordManagerProxy.PasswordCheckInteraction.START_CHECK_MANUALLY);
        this.passwordManager.startBulkPasswordCheck().then(
            () => {},
            error => {
                // Catching error
            });
        return;
      case CheckState.NO_PASSWORDS:
      case CheckState.QUOTA_LIMIT:
    }
    assertNotReached(
        'Can\'t trigger an action for state: ' + this.status.state);
  },

  /**
   * Returns true if there are any compromised credentials.
   * @return {boolean}
   * @private
   */
  hasLeakedCredentials_() {
    return !!this.leakedPasswords.length;
  },

  /**
   * Returns true if there are any weak credentials.
   * @return {boolean}
   * @private
   */
  hasWeakCredentials_() {
    return this.passwordsWeaknessCheckEnabled && !!this.weakPasswords.length;
  },

  /**
   * Returns true if there are any insecure credentials.
   * @return {boolean}
   * @private
   */
  hasInsecureCredentials_() {
    return !!this.leakedPasswords.length || this.hasWeakCredentials_();
  },

  /**
   * Returns a relevant help text for weak passwords. Contains a link that
   * depends on whether the user is syncing passwords or not.
   * @return {string}
   * @private
   */
  getWeakPasswordsHelpText_() {
    return this.i18nAdvanced(
        this.isSyncingPasswords_ ? 'weakPasswordsDescriptionGeneration' :
                                   'weakPasswordsDescription');
  },

  /**
   * @param {!CustomEvent<{moreActionsButton: !HTMLElement}>} event
   * @private
   */
  onMoreActionsClick_(event) {
    const target = event.detail.moreActionsButton;
    this.$.moreActionsMenu.showAt(target);
    this.activeDialogAnchorStack_.push(target);
    this.activeListItem_ = event.target;
    this.activePassword_ = this.activeListItem_.item;
  },

  /** @private */
  onMenuShowPasswordClick_() {
    this.activePassword_.password ? this.activeListItem_.hidePassword() :
                                    this.activeListItem_.showPassword();
    this.$.moreActionsMenu.close();
    this.activePassword_ = null;
    this.activeDialogAnchorStack_.pop();
  },

  /** @private */
  onEditPasswordClick_() {
    this.passwordManager
        .getPlaintextInsecurePassword(
            assert(this.activePassword_),
            chrome.passwordsPrivate.PlaintextReason.EDIT)
        .then(
            insecureCredential => {
              this.activePassword_ = insecureCredential;
              this.showPasswordEditDialog_ = true;
            },
            error => {
              // <if expr="chromeos">
              // If no password was found, refresh auth token and retry.
              this.tokenRequestManager_.request(
                  this.onEditPasswordClick_.bind(this));
              // </if>
              // <if expr="not chromeos">
              this.activePassword_ = null;
              this.onPasswordEditDialogClosed_();
              // </if>
            });
    this.$.moreActionsMenu.close();
  },

  /** @private */
  onMenuRemovePasswordClick_() {
    this.$.moreActionsMenu.close();
    this.showPasswordRemoveDialog_ = true;
  },

  /** @private */
  onPasswordRemoveDialogClosed_() {
    this.showPasswordRemoveDialog_ = false;
    focusWithoutInk(assert(this.activeDialogAnchorStack_.pop()));
  },

  /** @private */
  onPasswordEditDialogClosed_() {
    this.showPasswordEditDialog_ = false;
    focusWithoutInk(assert(this.activeDialogAnchorStack_.pop()));
  },

  /**
   * @param {!CustomEvent<!HTMLElement>} event
   * @private
   */
  onAlreadyChangedClick_(event) {
    const target = event.detail;
    // Setting required properties for Password Check Edit dialog
    this.activeDialogAnchorStack_.push(target);
    this.activeListItem_ = event.target;
    this.activePassword_ = event.target.item;

    this.showPasswordEditDisclaimer_ = true;
  },

  /** @private */
  onEditDisclaimerClosed_() {
    this.showPasswordEditDisclaimer_ = false;
    focusWithoutInk(assert(this.activeDialogAnchorStack_.pop()));
  },

  /**
   * @return {string}
   * @private
   */
  computeShowHideMenuTitle() {
    return this.i18n(
        this.activeListItem_.isPasswordVisible_ ? 'hideCompromisedPassword' :
                                                  'showCompromisedPassword');
  },

  /**
   * Returns the icon (warning, info or error) indicating the check status.
   * @return {string}
   * @private
   */
  getStatusIcon_() {
    if (!this.hasInsecureCredentialsOrErrors_()) {
      return 'settings:check-circle';
    }
    if (this.hasInsecureCredentials_()) {
      return 'cr:warning';
    }
    return 'cr:info';
  },

  /**
   * Returns the CSS class used to style the icon (warning, info or error).
   * @return {string}
   * @private
   */
  getStatusIconClass_() {
    if (!this.hasInsecureCredentialsOrErrors_()) {
      return this.waitsForFirstCheck_() ? 'hidden' : 'no-security-issues';
    }
    if (this.hasInsecureCredentials_()) {
      return 'has-security-issues';
    }
    return '';
  },

  /**
   * Returns the title message indicating the state of the last/ongoing check.
   * @return {string}
   * @private
   */
  computeTitle_() {
    switch (this.status.state) {
      case CheckState.IDLE:
        return this.waitsForFirstCheck_() ?
            this.i18n('checkPasswordsDescription') :
            this.i18n('checkedPasswords');
      case CheckState.CANCELED:
        return this.i18n('checkPasswordsCanceled');
      case CheckState.RUNNING:
        // Returns the progress of a running check. Ensures that both numbers
        // are at least 1.
        return this.i18n(
            'checkPasswordsProgress', (this.status.alreadyProcessed || 0) + 1,
            Math.max(
                this.status.remainingInQueue + this.status.alreadyProcessed,
                1));
      case CheckState.OFFLINE:
        return this.i18n('checkPasswordsErrorOffline');
      case CheckState.SIGNED_OUT:
        // When user is signed out and |passwordsWeaknessCheckEnabled| is
        // true, we run the password weakness check. Since it works very fast,
        // we always shows "Checked passwords" in this case.
        return this.i18n(
            this.passwordsWeaknessCheckEnabled ?
                'checkedPasswords' :
                'checkPasswordsErrorSignedOut');
      case CheckState.NO_PASSWORDS:
        return this.i18n('checkPasswordsErrorNoPasswords');
      case CheckState.QUOTA_LIMIT:
        // Note: For the checkup case we embed the link as HTML, thus we need to
        // use i18nAdvanced() here as well as the `inner-h-t-m-l` attribute in
        // the DOM.
        return this.canUsePasswordCheckup_ ?
            this.i18nAdvanced('checkPasswordsErrorQuotaGoogleAccount') :
            this.i18n('checkPasswordsErrorQuota');
      case CheckState.OTHER_ERROR:
        return this.i18n('checkPasswordsErrorGeneric');
    }
    assertNotReached('Can\'t find a title for state: ' + this.status.state);
  },

  /**
   * Returns true iff a check is running right according to the given |status|.
   * @return {boolean}
   * @private
   */
  isCheckInProgress_() {
    return this.status.state === CheckState.RUNNING;
  },

  /**
   * Returns true to show the timestamp when a check was completed successfully.
   * @return {boolean}
   * @private
   */
  showsTimestamp_() {
    return this.status.state === CheckState.IDLE &&
        !!this.status.elapsedTimeSinceLastCheck;
  },

  /**
   * Returns the button caption indicating it's current functionality.
   * @return {string}
   * @private
   */
  getButtonText_() {
    switch (this.status.state) {
      case CheckState.IDLE:
        return this.waitsForFirstCheck_() ? this.i18n('checkPasswords') :
                                            this.i18n('checkPasswordsAgain');
      case CheckState.CANCELED:
        return this.i18n('checkPasswordsAgain');
      case CheckState.RUNNING:
        return this.i18n('checkPasswordsStop');
      case CheckState.OFFLINE:
      case CheckState.NO_PASSWORDS:
      case CheckState.OTHER_ERROR:
        return this.i18n('checkPasswordsAgainAfterError');
      case CheckState.SIGNED_OUT:
        // When |passwordsWeaknessCheckEnabled| is true, we should allow signed
        // out users to click the "Check again" button to run the passwords
        // weakness check.
        return this.i18n(
            this.passwordsWeaknessCheckEnabled ?
                'checkPasswordsAgain' :
                'checkPasswordsAgainAfterError');
      case CheckState.QUOTA_LIMIT:
        return '';  // Undefined behavior. Don't show any misleading text.
    }
    assertNotReached(
        'Can\'t find a button text for state: ' + this.status.state);
  },

  /**
   * Returns 'action-button' only for the very first check.
   * @return {string}
   * @private
   */
  getButtonTypeClass_() {
    return this.waitsForFirstCheck_() ? 'action-button' : ' ';
  },

  /**
   * Returns true iff the check/stop button should be visible for a given state.
   * @return {!boolean}
   * @private
   */
  computeIsButtonHidden_() {
    switch (this.status.state) {
      case CheckState.IDLE:
        return this.isInitialStatus;  // Only a native IDLE state allows checks.
      case CheckState.CANCELED:
      case CheckState.RUNNING:
      case CheckState.OFFLINE:
      case CheckState.OTHER_ERROR:
        return false;
      case CheckState.SIGNED_OUT:
        // When |passwordsWeaknessCheckEnabled| is true, we should allow signed
        // out users to run the passwords weakness check.
        return !this.passwordsWeaknessCheckEnabled && this.isSignedOut_;
      case CheckState.NO_PASSWORDS:
      case CheckState.QUOTA_LIMIT:
        return true;
    }
    assertNotReached(
        'Can\'t determine button visibility for state: ' + this.status.state);
  },

  /**
   * Returns the chrome:// address where the banner image is located.
   * @param {boolean} isDarkMode
   * @return {string}
   * @private
   */
  bannerImageSrc_(isDarkMode) {
    const type =
        (this.status.state === CheckState.IDLE && !this.waitsForFirstCheck_()) ?
        'positive' :
        'neutral';
    const suffix = isDarkMode ? '_dark' : '';
    return `chrome://settings/images/password_check_${type}${suffix}.svg`;
  },

  /**
   * Returns true iff the banner should be shown.
   * @return {boolean}
   * @private
   */
  shouldShowBanner_() {
    if (this.hasInsecureCredentials_()) {
      return false;
    }
    return this.status.state === CheckState.CANCELED ||
        !this.hasInsecureCredentialsOrErrors_();
  },

  /**
   * Returns true if there are insecure credentials or the status is unexpected
   * for a regular password check.
   * @return {boolean}
   * @private
   */
  hasInsecureCredentialsOrErrors_() {
    if (this.hasInsecureCredentials_()) {
      return true;
    }
    switch (this.status.state) {
      case CheckState.IDLE:
      case CheckState.RUNNING:
        return false;
      case CheckState.CANCELED:
      case CheckState.OFFLINE:
      case CheckState.NO_PASSWORDS:
      case CheckState.QUOTA_LIMIT:
      case CheckState.OTHER_ERROR:
        return true;
      case CheckState.SIGNED_OUT:
        // If |passwordsWeaknessCheckEnabled| is true and user is signed out,
        // this is not an error and we can run the password weakness check.
        return !this.passwordsWeaknessCheckEnabled;
    }
    assertNotReached(
        'Not specified whether to state is an error: ' + this.status.state);
  },

  /**
   * Returns true if there are insecure credentials or the status is unexpected
   * for a regular password check.
   * @return {boolean}
   * @private
   */
  showsPasswordsCount_() {
    if (this.hasInsecureCredentials_()) {
      return true;
    }
    switch (this.status.state) {
      case CheckState.IDLE:
        return !this.waitsForFirstCheck_();
      case CheckState.CANCELED:
      case CheckState.RUNNING:
      case CheckState.OFFLINE:
      case CheckState.NO_PASSWORDS:
      case CheckState.QUOTA_LIMIT:
      case CheckState.OTHER_ERROR:
        return false;
      case CheckState.SIGNED_OUT:
        // Shows "No security issues found" if user is signed out and doesn't
        // have insecure credentials.
        return this.passwordsWeaknessCheckEnabled;
    }
    assertNotReached(
        'Not specified whether to show passwords for state: ' +
        this.status.state);
  },

  /**
   * Returns a localized and pluralized string of the passwords count, depending
   * on whether the weak check feature is enabled, whether the user is signed in
   * and whether other compromised passwords exist.
   * @return {string}
   * @private
   */
  getPasswordsCount_() {
    if (!this.passwordsWeaknessCheckEnabled) {
      return this.compromisedPasswordsCount;
    }

    return this.isSignedOut_ && this.leakedPasswords.length === 0 ?
        this.weakPasswordsCount :
        this.insecurePasswordsCount;
  },

  /**
   * Returns the label that should be shown in the compromised password section
   * if a user is signed out. This label depends on whether the user already had
   * compromised credentials that were found in the past.
   * @return {string}
   * @private
   */
  getSignedOutUserLabel_() {
    // This label contains the link, thus we need to use i18nAdvanced() here as
    // well as the `inner-h-t-m-l` attribute in the DOM.
    return this.i18nAdvanced(
        this.hasLeakedCredentials_() ?
            'signedOutUserHasCompromisedCredentialsLabel' :
            'signedOutUserLabel');
  },

  /**
   * Returns true iff the leak check was performed at least once before.
   * @return {boolean}
   * @private
   */
  waitsForFirstCheck_() {
    // We don't run the compromise check if user is signed out and don't need to
    // wait for the first check.
    if (this.passwordsWeaknessCheckEnabled && this.isSignedOut_) {
      return false;
    }
    return !this.status.elapsedTimeSinceLastCheck;
  },

  /**
   * Returns true iff the user is signed out.
   * @return {boolean}
   * @private
   */
  computeIsSignedOut_() {
    if (!this.syncStatus_ || !this.syncStatus_.signedIn) {
      return !this.storedAccounts_ || this.storedAccounts_.length === 0;
    }
    return !!this.syncStatus_.hasError;
  },

  /**
   * Returns true iff the user is syncing passwords.
   * @return {boolean}
   * @private
   */
  computeIsSyncingPasswords_() {
    return !!this.syncStatus_ && !!this.syncStatus_.signedIn &&
        !this.syncStatus_.hasError && !!this.syncPrefs_ &&
        this.syncPrefs_.passwordsSynced;
  },

  /**
   * Returns whether the user can use the online Password Checkup.
   * @return {boolean}
   * @private
   */
  computeCanUsePasswordCheckup_() {
    return !!this.syncStatus_ && !!this.syncStatus_.signedIn &&
        (!this.syncPrefs_ || !this.syncPrefs_.encryptAllData);
  },

  /**
   * @return {boolean}
   * @private
   */
  computeShowCompromisedCredentialsBody_() {
    // Always shows compromised credetnials section if
    // |passwordsWeaknessCheckEnabled| is true and user is signed out.
    if (this.passwordsWeaknessCheckEnabled && this.isSignedOut_) {
      return true;
    }
    return this.hasLeakedCredentials_();
  },

  /**
   * @return {boolean}
   * @private
   */
  computeShowNoCompromisedPasswordsLabel_() {
    // Check if user isn't signed in.
    if (!this.syncStatus_ || !this.syncStatus_.signedIn) {
      return false;
    }

    // Check if breach detection is turned off in settings.
    if (!this.prefs ||
        !this.getPref('profile.password_manager_leak_detection').value) {
      return false;
    }

    // Return true if there was a successful check and no compromised passwords
    // were found.
    return !this.hasLeakedCredentials_() && this.showsTimestamp_();
  },

  /**
   * @param {!CustomEvent<{id: number}>} event
   * @private
   */
  onChangePasswordClick_(event) {
    this.clickedChangePasswordIds_.add(event.detail.id);
    this.notifyPath('clickedChangePasswordIds_.size');
  },

  /**
   * @param {!PasswordManagerProxy.InsecureCredential} item
   * @return {boolean}
   * @private
   */
  clickedChangePassword_(item) {
    return this.clickedChangePasswordIds_.has(item.id);
  },

  // <if expr="chromeos">
  /**
   * Copied from passwords_section.js.
   * TODO(crbug.com/1074228): Extract to a separate behavior
   *
   * @param {!CustomEvent<!chrome.quickUnlockPrivate.TokenInfo>} e - Contains
   *     newly created auth token. Note that its precise value is not relevant
   *     here, only the facts that it's created.
   * @private
   */
  onTokenObtained_(e) {
    assert(e.detail);
    this.tokenRequestManager_.resolve();
  },

  /** @private */
  onPasswordPromptClosed_() {
    this.showPasswordPromptDialog_ = false;
    focusWithoutInk(assert(this.activeDialogAnchorStack_.pop()));
  },

  /** @private */
  openPasswordPromptDialog_() {
    this.activeDialogAnchorStack_.push(/** @type {!HTMLElement} */ (getDeepActiveElement()));
    this.showPasswordPromptDialog_ = true;
  },
  // </if>
});
