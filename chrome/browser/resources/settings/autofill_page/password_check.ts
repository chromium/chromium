// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_action_menu/cr_action_menu.js';
import 'chrome://resources/cr_elements/cr_button/cr_button.m.js';
import '../settings_shared_css.js';
import 'chrome://resources/cr_elements/icons.m.js';
import 'chrome://resources/cr_elements/shared_style_css.m.js';
import 'chrome://resources/polymer/v3_0/iron-flex-layout/iron-flex-layout-classes.js';
import 'chrome://resources/polymer/v3_0/iron-icon/iron-icon.js';
import 'chrome://resources/polymer/v3_0/iron-list/iron-list.js';
import 'chrome://resources/polymer/v3_0/paper-spinner/paper-spinner-lite.js';
import '../i18n_setup.js';
import '../route.js';
import '../prefs/prefs.js';
import './password_check_edit_dialog.js';
import './password_check_edit_disclaimer_dialog.js';
import './password_check_list_item.js';
import './password_remove_confirmation_dialog.js';
// <if expr="chromeos_ash or chromeos_lacros">
import '../controls/password_prompt_dialog.js';

// </if>

import {CrActionMenuElement} from 'chrome://resources/cr_elements/cr_action_menu/cr_action_menu.js';
import {CrButtonElement} from 'chrome://resources/cr_elements/cr_button/cr_button.m.js';
import {assert, assertNotReached} from 'chrome://resources/js/assert.m.js';
import {focusWithoutInk} from 'chrome://resources/js/cr/ui/focus_without_ink.m.js';
import {I18nMixin, I18nMixinInterface} from 'chrome://resources/js/i18n_mixin.js';
// <if expr="chromeos_ash or chromeos_lacros">
import {getDeepActiveElement} from 'chrome://resources/js/util.m.js';
// </if>
import {WebUIListenerMixin, WebUIListenerMixinInterface} from 'chrome://resources/js/web_ui_listener_mixin.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

// <if expr="chromeos_ash or chromeos_lacros">
import {loadTimeData} from '../i18n_setup.js';
// </if>

import {StoredAccount, SyncBrowserProxyImpl, SyncPrefs, SyncStatus} from '../people_page/sync_browser_proxy.js';
import {PrefsMixin, PrefsMixinInterface} from '../prefs/prefs_mixin.js';
import {routes} from '../route.js';
import {Route, RouteObserverMixin, RouteObserverMixinInterface, Router} from '../router.js';

// <if expr="chromeos_ash or chromeos_lacros">
import {BlockingRequestManager} from './blocking_request_manager.js';
// </if>

import {getTemplate} from './password_check.html.js';
import {PasswordCheckListItemElement} from './password_check_list_item.js';
import {PasswordCheckMixin, PasswordCheckMixinInterface} from './password_check_mixin.js';
import {PasswordCheckInteraction, SavedPasswordListChangedListener} from './password_manager_proxy.js';


const CheckState = chrome.passwordsPrivate.PasswordCheckState;

export interface SettingsPasswordCheckElement {
  $: {
    compromisedCredentialsBody: HTMLElement,
    compromisedPasswordsDescription: HTMLElement,
    controlPasswordCheckButton: CrButtonElement,
    leakedPasswordList: HTMLElement,
    menuEditPassword: HTMLButtonElement,
    menuShowPassword: HTMLButtonElement,
    moreActionsMenu: CrActionMenuElement,
    mutedPasswordList: HTMLElement,
    noCompromisedCredentials: HTMLElement,
    signedOutUserLabel: HTMLElement,
    subtitle: HTMLElement,
    title: HTMLElement,
    titleRow: HTMLElement,
    weakCredentialsBody: HTMLElement,
    weakPasswordsDescription: HTMLElement,
    weakPasswordList: HTMLElement,
  };
}

const SettingsPasswordCheckElementBase =
    RouteObserverMixin(WebUIListenerMixin(
        I18nMixin(PrefsMixin(PasswordCheckMixin((PolymerElement)))))) as {
      new (): PolymerElement & I18nMixinInterface &
      WebUIListenerMixinInterface & PrefsMixinInterface &
      PasswordCheckMixinInterface & RouteObserverMixinInterface
    };

export class SettingsPasswordCheckElement extends
    SettingsPasswordCheckElementBase {
  static get is() {
    return 'settings-password-check';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      storedAccounts_: Array,

      title_: {
        type: String,
        computed: 'computeTitle_(status, canUsePasswordCheckup_)',
      },

      mutedLeakedCredentialsTitle_: {
        type: String,
        computed: 'computeMutedLeakedCredentialsTitle_(mutedPasswords)',
      },

      isSignedOut_: {
        type: Boolean,
        computed: 'computeIsSignedOut_(syncStatus_, storedAccounts_)',
      },

      isSyncingPasswords_: {
        type: Boolean,
        computed: 'computeIsSyncingPasswords_(syncPrefs_, syncStatus_)',
      },

      canUsePasswordCheckup_: {
        type: Boolean,
        computed: 'computeCanUsePasswordCheckup_(syncPrefs_, syncStatus_)',
      },

      isButtonHidden_: {
        type: Boolean,
        computed:
            'computeIsButtonHidden_(status, isSignedOut_, isInitialStatus)',
      },

      isMutePasswordButtonEnabled_: {
        type: Boolean,
        computed: 'computeIsMutePasswordButtonEnabled_(activePassword_)',
      },

      isUnmutePasswordButtonEnabled_: {
        type: Boolean,
        computed: 'computeIsUnmutePasswordButtonEnabled_(activePassword_)',
      },

      syncPrefs_: Object,
      syncStatus_: Object,
      showPasswordEditDialog_: Boolean,
      showPasswordRemoveDialog_: Boolean,
      showPasswordEditDisclaimer_: Boolean,

      /**
       * The password that the user is interacting with now.
       */
      activePassword_: Object,

      showCompromisedCredentialsBody_: {
        type: Boolean,
        computed: 'computeShowCompromisedCredentialsBody_(' +
            'isSignedOut_, leakedPasswords, mutedPasswords)',
      },

      showMutedPasswordsSection_: {
        type: Boolean,
        computed: 'computeShowMutedLeakedCredentials_(mutedPasswords)'
      },

      showNoCompromisedPasswordsLabel_: {
        type: Boolean,
        computed: 'computeShowNoCompromisedPasswordsLabel_(' +
            'syncStatus_, prefs.*, status, leakedPasswords)',
      },

      showHideMenuTitle_: {
        type: String,
        computed: 'computeShowHideMenuTitle(activePassword_)',
      },

      iconHaloClass_: {
        type: String,
        computed: 'computeIconHaloClass_(' +
            'status, isSignedOut_, leakedPasswords, weakPasswords)',
      },

      /**
       * The ids of insecure credentials for which user clicked "Change
       * Password" button
       */
      clickedChangePasswordIds_: {
        type: Object,
        value: new Set(),
      },

      // <if expr="chromeos_ash or chromeos_lacros">
      showPasswordPromptDialog_: Boolean,
      tokenRequestManager_: Object,
      // </if>
    };
  }

  private storedAccounts_: Array<StoredAccount>;
  private title_: string;
  private mutedPasswordsTitle_: string;
  private isSignedOut_: boolean;
  private isSyncingPasswords_: boolean;
  private canUsePasswordCheckup_: boolean;
  private isButtonHidden_: boolean;
  private isMutePasswordButtonEnabled_: boolean;
  private isUnmutePasswordButtonEnabled_: boolean;
  private syncPrefs_: SyncPrefs;
  private syncStatus_: SyncStatus;
  private showPasswordEditDialog_: boolean;
  private showPasswordRemoveDialog_: boolean;
  private showPasswordEditDisclaimer_: boolean;
  private activePassword_: chrome.passwordsPrivate.InsecureCredential|null;
  private showCompromisedCredentialsBody_: boolean;
  private showMutedPasswordsSection_: boolean;
  private showNoCompromisedPasswordsLabel_: boolean;
  private showHideMenuTitle_: string;
  private iconHaloClass_: string;
  private clickedChangePasswordIds_: Set<number>;

  // <if expr="chromeos_ash or chromeos_lacros">
  private showPasswordPromptDialog_: boolean;
  private tokenRequestManager_: BlockingRequestManager;
  // </if>

  private activeDialogAnchorStack_: Array<HTMLElement>|null;
  private activeListItem_: PasswordCheckListItemElement|null;
  startCheckAutomaticallySucceeded: boolean = false;
  private setSavedPasswordsListener_: SavedPasswordListChangedListener|null;

  constructor() {
    super();

    /**
     * A stack of the elements that triggered dialog to open and should
     * therefore receive focus when that dialog is closed. The bottom of the
     * stack is the element that triggered the earliest open dialog and top of
     * the stack is the element that triggered the most recent (i.e. active)
     * dialog. If no dialog is open, the stack is empty.
     */
    this.activeDialogAnchorStack_ = null;

    /**
     * The password_check_list_item that the user is interacting with now.
     */
    this.activeListItem_ = null;

    /**
     * Observer for saved passwords to update startCheckAutomaticallySucceeded
     * once they are changed. It's needed to run password check on navigation
     * again once passwords changed.
     */
    this.setSavedPasswordsListener_ = null;
  }

  override connectedCallback() {
    super.connectedCallback();

    // <if expr="chromeos_ash or chromeos_lacros">
    // If the user's account supports the password check, an auth token will be
    // required in order for them to view or export passwords. Otherwise there
    // is no additional security so |tokenRequestManager_| will immediately
    // resolve requests.
    this.tokenRequestManager_ =
        loadTimeData.getBoolean('userCannotManuallyEnterPassword') ?
        new BlockingRequestManager() :
        new BlockingRequestManager(() => this.openPasswordPromptDialog_());

    // </if>
    this.activeDialogAnchorStack_ = [];

    const setSavedPasswordsListener: SavedPasswordListChangedListener =
        _list => {
          this.startCheckAutomaticallySucceeded = false;
        };
    this.setSavedPasswordsListener_ = setSavedPasswordsListener;
    this.passwordManager!.addSavedPasswordListChangedListener(
        setSavedPasswordsListener);

    // Set the manager. These can be overridden by tests.
    const syncBrowserProxy = SyncBrowserProxyImpl.getInstance();

    const syncStatusChanged = (syncStatus: SyncStatus) => this.syncStatus_ =
        syncStatus;
    const syncPrefsChanged = (syncPrefs: SyncPrefs) => this.syncPrefs_ =
        syncPrefs;

    // Listen for changes.
    this.addWebUIListener('sync-status-changed', syncStatusChanged);
    this.addWebUIListener('sync-prefs-changed', syncPrefsChanged);

    // Request initial data.
    syncBrowserProxy.getSyncStatus().then(syncStatusChanged);
    syncBrowserProxy.sendSyncPrefsChanged();

    // For non-ChromeOS, also check whether accounts are available.
    // <if expr="not (chromeos_ash or chromeos_lacros)">
    const storedAccountsChanged = (accounts: Array<StoredAccount>) =>
        this.storedAccounts_ = accounts;
    syncBrowserProxy.getStoredAccounts().then(storedAccountsChanged);
    this.addWebUIListener('stored-accounts-updated', storedAccountsChanged);
    // </if>
  }

  override disconnectedCallback() {
    super.disconnectedCallback();

    this.passwordManager!.removeSavedPasswordListChangedListener(
        assert(this.setSavedPasswordsListener_!));
    this.setSavedPasswordsListener_ = null;
  }

  /**
   * Tries to start bulk password check on page open if instructed to do so and
   * didn't start successfully before
   */
  override currentRouteChanged(currentRoute: Route) {
    const router = Router.getInstance();

    if (currentRoute.path === routes.CHECK_PASSWORDS.path &&
        !this.startCheckAutomaticallySucceeded &&
        router.getQueryParameters().get('start') === 'true') {
      this.passwordManager!.recordPasswordCheckInteraction(
          PasswordCheckInteraction.START_CHECK_AUTOMATICALLY);
      this.passwordManager!.startBulkPasswordCheck().then(
          () => {
            this.startCheckAutomaticallySucceeded = true;
          },
          _error => {
              // Catching error
          });
    }
    // Requesting status on navigation to update elapsedTimeSinceLastCheck
    this.passwordManager!.getPasswordCheckStatus().then(
        status => this.status = status);
  }

  /**
   * Start/Stop bulk password check.
   */
  private onPasswordCheckButtonClick_() {
    switch (this.status.state) {
      case CheckState.RUNNING:
        this.passwordManager!.recordPasswordCheckInteraction(
            PasswordCheckInteraction.STOP_CHECK);
        this.passwordManager!.stopBulkPasswordCheck();
        return;
      case CheckState.IDLE:
      case CheckState.CANCELED:
      case CheckState.OFFLINE:
      case CheckState.OTHER_ERROR:
        this.passwordManager!.recordPasswordCheckInteraction(
            PasswordCheckInteraction.START_CHECK_MANUALLY);
        this.passwordManager!.startBulkPasswordCheck();
        return;
      case CheckState.SIGNED_OUT:
        // Runs the startBulkPasswordCheck to check passwords for weakness that
        // works for both sign in and sign out users.
        this.passwordManager!.recordPasswordCheckInteraction(
            PasswordCheckInteraction.START_CHECK_MANUALLY);
        this.passwordManager!.startBulkPasswordCheck().then(
            () => {},
            _error => {
                // Catching error
            });
        return;
      case CheckState.NO_PASSWORDS:
      case CheckState.QUOTA_LIMIT:
    }
    assertNotReached(
        'Can\'t trigger an action for state: ' + this.status.state);
  }

  /**
   * @return true if there are any compromised credentials.
   */
  private hasLeakedCredentials_(): boolean {
    return !!this.leakedPasswords.length;
  }

  /**
   * @return true if there are any compromised credentials that are dismissed.
   */
  private computeShowMutedLeakedCredentials_(): boolean {
    return this.isMutedPasswordsEnabled && !!this.mutedPasswords.length;
  }

  /**
   * @return true if there are any weak credentials.
   */
  private hasWeakCredentials_(): boolean {
    return !!this.weakPasswords.length;
  }

  /**
   * @return true if there are any insecure credentials.
   */
  private hasInsecureCredentials_(): boolean {
    return !!this.leakedPasswords.length || this.hasWeakCredentials_();
  }

  /**
   * @return A relevant help text for weak passwords. Contains a link that
   * depends on whether the user is syncing passwords or not.
   */
  private getWeakPasswordsHelpText_(): string {
    return this.i18nAdvanced(
        this.isSyncingPasswords_ ? 'weakPasswordsDescriptionGeneration' :
                                   'weakPasswordsDescription');
  }

  private onMoreActionsClick_(
      event: CustomEvent<{moreActionsButton: HTMLElement}>) {
    const target = event.detail.moreActionsButton;
    this.$.moreActionsMenu.showAt(target);
    this.activeDialogAnchorStack_!.push(target);
    this.activeListItem_ = event.target as PasswordCheckListItemElement;
    this.activePassword_ = this.activeListItem_!.item;
  }

  private onMenuShowPasswordClick_() {
    this.activePassword_!.password ? this.activeListItem_!.hidePassword() :
                                     this.activeListItem_!.showPassword();
    this.$.moreActionsMenu.close();
    this.activePassword_ = null;
    this.activeDialogAnchorStack_!.pop();
  }

  private onEditPasswordClick_() {
    this.passwordManager!
        .getPlaintextInsecurePassword(
            assert(this.activePassword_!),
            chrome.passwordsPrivate.PlaintextReason.EDIT)
        .then(
            insecureCredential => {
              this.activePassword_ = insecureCredential;
              this.showPasswordEditDialog_ = true;
            },
            _error => {
              // <if expr="chromeos_ash or chromeos_lacros">
              // If no password was found, refresh auth token and retry.
              this.tokenRequestManager_.request(
                  () => this.onEditPasswordClick_());
              // </if>
              // <if expr="not (chromeos_ash or chromeos_lacros)">
              this.activePassword_ = null;
              this.onPasswordEditDialogClosed_();
              // </if>
            });
    this.$.moreActionsMenu.close();
  }

  private onMenuRemovePasswordClick_() {
    this.$.moreActionsMenu.close();
    this.showPasswordRemoveDialog_ = true;
  }

  private onMenuMuteCompromisedPasswordClick_() {
    this.passwordManager!.recordPasswordCheckInteraction(
        PasswordCheckInteraction.MUTE_PASSWORD);
    this.$.moreActionsMenu.close();
    this.passwordManager!.muteInsecureCredential(
        assert(this.activeListItem_!.item));
  }

  private onMenuUnmuteMutedCompromisedPasswordClick_() {
    this.passwordManager!.recordPasswordCheckInteraction(
        PasswordCheckInteraction.UNMUTE_PASSWORD);
    this.$.moreActionsMenu.close();
    this.passwordManager!.unmuteInsecureCredential(
        assert(this.activeListItem_!.item));
  }

  private onPasswordRemoveDialogClosed_() {
    this.showPasswordRemoveDialog_ = false;
    focusWithoutInk(assert(this.activeDialogAnchorStack_!.pop()!));
  }

  private onPasswordEditDialogClosed_() {
    this.showPasswordEditDialog_ = false;
    focusWithoutInk(assert(this.activeDialogAnchorStack_!.pop()!));
  }

  private onAlreadyChangedClick_(event: CustomEvent<HTMLElement>) {
    const target = event.detail;
    // Setting required properties for Password Check Edit dialog
    this.activeDialogAnchorStack_!.push(target);
    this.activeListItem_ = event.target as PasswordCheckListItemElement;
    this.activePassword_ = this.activeListItem_.item;

    this.showPasswordEditDisclaimer_ = true;
  }

  private onEditDisclaimerClosed_() {
    this.showPasswordEditDisclaimer_ = false;
    focusWithoutInk(assert(this.activeDialogAnchorStack_!.pop()!));
  }

  private computeShowHideMenuTitle(): string {
    return this.i18n(
        this.activeListItem_!.isPasswordVisible ? 'hideCompromisedPassword' :
                                                  'showCompromisedPassword');
  }

  private computeIconHaloClass_(): string {
    return !this.isCheckInProgress_() && this.hasLeakedCredentials_() ?
        'warning-halo' :
        '';
  }

  /**
   * @return the icon (warning, info or error) indicating the check status.
   */
  private getStatusIcon_(): string {
    if (!this.hasInsecureCredentialsOrErrors_()) {
      return 'settings:check-circle';
    }
    if (this.hasLeakedCredentials_()) {
      return 'cr:warning';
    }
    return 'cr:info';
  }

  /**
   * @return the CSS class used to style the icon (warning, info or error).
   */
  private getStatusIconClass_(): string {
    if (!this.hasInsecureCredentialsOrErrors_()) {
      return this.waitsForFirstCheck_() ? 'hidden' : 'no-security-issues';
    }
    if (this.hasLeakedCredentials_()) {
      return 'has-security-issues';
    }
    return '';
  }

  /**
   * @return the title message indicating the state of the last/ongoing check.
   */
  private computeTitle_(): string {
    switch (this.status.state) {
      case CheckState.IDLE:
        return this.waitsForFirstCheck_() ? '' : this.i18n('checkedPasswords');
      case CheckState.CANCELED:
        return this.i18n('checkPasswordsCanceled');
      case CheckState.RUNNING:
        // Returns the progress of a running check. Ensures that both numbers
        // are at least 1.
        const alreadyProcessed = this.status.alreadyProcessed || 0;
        return this.i18n(
            'checkPasswordsProgress', alreadyProcessed + 1,
            Math.max(this.status.remainingInQueue! + alreadyProcessed, 1));
      case CheckState.OFFLINE:
        return this.i18n('checkPasswordsErrorOffline');
      case CheckState.SIGNED_OUT:
        // When user is signed out we run the password weakness check. Since it
        // works very fast, we always shows "Checked passwords" in this case.
        return this.i18n('checkedPasswords');
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
  }

  /**
   * @return the muted / dismissed passwords section title which includes the
   * number of muted passwords.
   */
  private computeMutedLeakedCredentialsTitle_(): string {
    return this.i18n('mutedPasswords', this.mutedPasswords.length);
  }

  /**
   * @return true iff a check is running right according to the given |status|.
   */
  private isCheckInProgress_(): boolean {
    return this.status.state === CheckState.RUNNING;
  }

  /**
   * @return true if a password is compromised. A weak password may not be
   * compromised.
   */
  private isPasswordCompromised_(): boolean {
    return !!this.activePassword_ && !!this.activePassword_!.compromisedInfo;
  }

  /**
   * @return true if the pref value for
   *     profile.password_dismiss_compromised_alert exists and equals to false.
   */
  private isMutingDisabledByPrefs_(): boolean {
    return !!this.prefs &&
        this.getPref('profile.password_dismiss_compromised_alert').value ===
        false;
  }

  /**
   * @return true if muting is enabled
   * and the password is compromised and is dismissable/mutable.
   */
  private computeIsMutePasswordButtonEnabled_(): boolean {
    return this.isMutedPasswordsEnabled && this.isPasswordCompromised_() &&
        !this.activePassword_!.compromisedInfo!.isMuted;
  }

  /**
   * @return true if unmuting is enabled
   * and the password is compromised and is dismissed/muted.
   */
  private computeIsUnmutePasswordButtonEnabled_(): boolean {
    return this.isMutedPasswordsEnabled && this.isPasswordCompromised_() &&
        !!this.activePassword_!.compromisedInfo!.isMuted;
  }

  /**
   * @return true to show the timestamp when a check was completed successfully.
   */
  private showsTimestamp_(): boolean {
    return !!this.status.elapsedTimeSinceLastCheck &&
        (this.status.state === CheckState.IDLE ||
         this.status.state === CheckState.SIGNED_OUT);
  }

  /**
   * @return the button caption indicating it's current functionality.
   */
  private getButtonText_(): string {
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
        // We should allow signed out users to click the "Check again" button to
        // run the passwords weakness check.
        return this.i18n('checkPasswordsAgain');
      case CheckState.QUOTA_LIMIT:
        return '';  // Undefined behavior. Don't show any misleading text.
    }
    assertNotReached(
        'Can\'t find a button text for state: ' + this.status.state);
  }

  /**
   * @return 'action-button' only for the very first check.
   */
  private getButtonTypeClass_(): string {
    return this.waitsForFirstCheck_() ? 'action-button' : ' ';
  }

  /**
   * @return true iff the check/stop button should be visible for a given state.
   */
  private computeIsButtonHidden_(): boolean {
    switch (this.status.state) {
      case CheckState.IDLE:
        return this.isInitialStatus;  // Only a native IDLE state allows checks.
      case CheckState.CANCELED:
      case CheckState.RUNNING:
      case CheckState.OFFLINE:
      case CheckState.OTHER_ERROR:
      case CheckState.SIGNED_OUT:
        return false;
      case CheckState.NO_PASSWORDS:
      case CheckState.QUOTA_LIMIT:
        return true;
    }
    assertNotReached(
        'Can\'t determine button visibility for state: ' + this.status.state);
    return true;
  }

  /**
   * @return The chrome:// address where the banner image is located.
   */
  private bannerImageSrc_(isDarkMode: boolean): string {
    const type =
        (this.status.state === CheckState.IDLE && !this.waitsForFirstCheck_()) ?
        'positive' :
        'neutral';
    const suffix = isDarkMode ? '_dark' : '';
    return `chrome://settings/images/password_check_${type}${suffix}.svg`;
  }

  /**
   * @return true iff the banner should be shown.
   */
  private shouldShowBanner_(): boolean {
    if (this.hasInsecureCredentials_()) {
      return false;
    }
    return this.status.state === CheckState.CANCELED ||
        !this.hasInsecureCredentialsOrErrors_();
  }

  /**
   * @return true if there are insecure credentials or the status is unexpected
   * for a regular password check.
   */
  private hasInsecureCredentialsOrErrors_(): boolean {
    if (this.hasInsecureCredentials_()) {
      return true;
    }
    switch (this.status.state) {
      case CheckState.IDLE:
      case CheckState.RUNNING:
      case CheckState.SIGNED_OUT:
        return false;
      case CheckState.CANCELED:
      case CheckState.OFFLINE:
      case CheckState.NO_PASSWORDS:
      case CheckState.QUOTA_LIMIT:
      case CheckState.OTHER_ERROR:
        return true;
    }
    assertNotReached(
        'Not specified whether to state is an error: ' + this.status.state);
  }

  /**
   * @return true if there are insecure credentials or the status is unexpected
   * for a regular password check.
   */
  private showsPasswordsCount_(): boolean {
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
        return true;
    }
    assertNotReached(
        'Not specified whether to show passwords for state: ' +
        this.status.state);
  }

  /**
   * @return a localized and pluralized string of the passwords count, depending
   * on whether the user is signed in and whether other compromised passwords
   * exist.
   */
  private getPasswordsCount_(): string {
    return this.isSignedOut_ && this.leakedPasswords.length === 0 ?
        this.weakPasswordsCount :
        this.insecurePasswordsCount;
  }

  /**
   * @return the label that should be shown in the compromised password section
   * if a user is signed out. This label depends on whether the user already had
   * compromised credentials that were found in the past.
   */
  private getSignedOutUserLabel_(): string {
    // This label contains the link, thus we need to use i18nAdvanced() here as
    // well as the `inner-h-t-m-l` attribute in the DOM.
    return this.i18nAdvanced(
        this.hasLeakedCredentials_() ?
            'signedOutUserHasCompromisedCredentialsLabel' :
            'signedOutUserLabel');
  }

  /**
   * @return true iff the leak or weak check was performed at least once before.
   */
  private waitsForFirstCheck_(): boolean {
    return !this.status.elapsedTimeSinceLastCheck;
  }

  /**
   * @return true iff the user is signed out.
   */
  private computeIsSignedOut_(): boolean {
    if (!this.syncStatus_ || !this.syncStatus_.signedIn) {
      return !this.storedAccounts_ || this.storedAccounts_.length === 0;
    }
    return !!this.syncStatus_.hasError;
  }

  /**
   * @return true iff the user is syncing passwords.
   */
  private computeIsSyncingPasswords_(): boolean {
    return !!this.syncStatus_ && !!this.syncStatus_.signedIn &&
        !this.syncStatus_.hasError && !!this.syncPrefs_ &&
        this.syncPrefs_.passwordsSynced;
  }

  /**
   * @return whether the user can use the online Password Checkup.
   */
  private computeCanUsePasswordCheckup_(): boolean {
    return !!this.syncStatus_ && !!this.syncStatus_.signedIn &&
        (!this.syncPrefs_ || !this.syncPrefs_.encryptAllData);
  }

  private computeShowCompromisedCredentialsBody_(): boolean {
    // Always shows compromised credentials section if user is signed out.
    return this.isSignedOut_ || this.hasLeakedCredentials_() ||
        this.computeShowMutedLeakedCredentials_();
  }

  private computeShowNoCompromisedPasswordsLabel_(): boolean {
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
  }

  private onChangePasswordClick_(event: CustomEvent<{id: number}>) {
    this.clickedChangePasswordIds_.add(event.detail.id);
    this.notifyPath('clickedChangePasswordIds_.size');
  }

  private clickedChangePassword_(
      item: chrome.passwordsPrivate.InsecureCredential): boolean {
    return this.clickedChangePasswordIds_.has(item.id);
  }

  // <if expr="chromeos_ash or chromeos_lacros">
  /**
   * Copied from passwords_section.js.
   * TODO(crbug.com/1074228): Extract to a separate behavior
   *
   * @param e Contains newly created auth token
   *     chrome.quickUnlockPrivate.TokenInfo. Note that its precise value is
   *     not relevant here, only the facts that it's created.
   */
  private onTokenObtained_(e: CustomEvent<any>) {
    assert(e.detail);
    this.tokenRequestManager_.resolve();
  }

  private onPasswordPromptClosed_() {
    this.showPasswordPromptDialog_ = false;
    focusWithoutInk(assert(this.activeDialogAnchorStack_!.pop()!));
  }

  private openPasswordPromptDialog_() {
    this.activeDialogAnchorStack_!.push(getDeepActiveElement() as HTMLElement);
    this.showPasswordPromptDialog_ = true;
  }
  // </if>
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-password-check': SettingsPasswordCheckElement,
  }
}

customElements.define(
    SettingsPasswordCheckElement.is, SettingsPasswordCheckElement);
