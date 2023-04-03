// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_components/settings_prefs/prefs.js';
import 'chrome://resources/cr_elements/cr_action_menu/cr_action_menu.js';
import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import '../settings_shared.css.js';
import 'chrome://resources/cr_elements/icons.html.js';
import 'chrome://resources/cr_elements/cr_shared_style.css.js';
import 'chrome://resources/polymer/v3_0/iron-flex-layout/iron-flex-layout-classes.js';
import 'chrome://resources/polymer/v3_0/iron-icon/iron-icon.js';
import 'chrome://resources/polymer/v3_0/iron-list/iron-list.js';
import 'chrome://resources/polymer/v3_0/paper-spinner/paper-spinner-lite.js';
import '../i18n_setup.js';
import '../route.js';
import './password_check_edit_disclaimer_dialog.js';
import './password_check_list_item.js';
import './password_remove_confirmation_dialog.js';
// <if expr="is_chromeos">
import '../controls/password_prompt_dialog.js';

// </if>

import {PrefsMixin} from 'chrome://resources/cr_components/settings_prefs/prefs_mixin.js';
import {CrActionMenuElement} from 'chrome://resources/cr_elements/cr_action_menu/cr_action_menu.js';
import {CrButtonElement} from 'chrome://resources/cr_elements/cr_button/cr_button.js';
import {I18nMixin} from 'chrome://resources/cr_elements/i18n_mixin.js';
import {WebUiListenerMixin} from 'chrome://resources/cr_elements/web_ui_listener_mixin.js';
import {assert, assertNotReached} from 'chrome://resources/js/assert_ts.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {routes} from '../route.js';
import {Route, RouteObserverMixin, Router} from '../router.js';

import {MergePasswordsStoreCopiesMixin} from './merge_passwords_store_copies_mixin.js';
import {getTemplate} from './password_check.html.js';
import {PasswordCheckListItemElement} from './password_check_list_item.js';
import {PasswordCheckMixin} from './password_check_mixin.js';
import {PasswordCheckInteraction, SavedPasswordListChangedListener} from './password_manager_proxy.js';
import {PasswordRequestorMixin} from './password_requestor_mixin.js';
import {UserUtilMixin} from './user_util_mixin.js';

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
    UserUtilMixin(MergePasswordsStoreCopiesMixin(
        RouteObserverMixin(WebUiListenerMixin(I18nMixin(PrefsMixin(
            PasswordRequestorMixin(PasswordCheckMixin((PolymerElement)))))))));

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
      title_: {
        type: String,
        computed: 'computeTitle_(status, canUsePasswordCheckup_)',
      },

      mutedLeakedCredentialsTitle_: {
        type: String,
        computed: 'computeMutedLeakedCredentialsTitle_(mutedPasswords)',
      },

      canUsePasswordCheckup_: {
        type: Boolean,
        computed: 'computeCanUsePasswordCheckup_(syncPrefs, ' +
            'isSyncingPasswords)',
      },

      isButtonHidden_: {
        type: Boolean,
        computed: 'computeIsButtonHidden_(status, signedIn, isInitialStatus)',
      },

      isMutePasswordButtonEnabled_: {
        type: Boolean,
        computed: 'computeIsMutePasswordButtonEnabled_(activePassword_)',
      },

      isUnmutePasswordButtonEnabled_: {
        type: Boolean,
        computed: 'computeIsUnmutePasswordButtonEnabled_(activePassword_)',
      },

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
            'signedIn, leakedPasswords, mutedPasswords)',
      },

      showMutedPasswordsSection_: {
        type: Boolean,
        computed: 'computeShowMutedLeakedCredentials_(mutedPasswords)',
      },

      showNoCompromisedPasswordsLabel_: {
        type: Boolean,
        computed: 'computeShowNoCompromisedPasswordsLabel_(' +
            'signedIn, prefs.*, status, leakedPasswords)',
      },

      showHideMenuTitle_: {
        type: String,
        computed: 'computeShowHideMenuTitle(activePassword_)',
      },

      iconHaloClass_: {
        type: String,
        computed: 'computeIconHaloClass_(' +
            'status, signedIn, leakedPasswords, weakPasswords)',
      },

      /**
       * The ids of insecure credentials for which user clicked "Change
       * Password" button
       */
      clickedChangePasswordIds_: {
        type: Object,
        value: new Set(),
      },
    };
  }

  private title_: TrustedHTML;
  private mutedPasswordsTitle_: string;
  private canUsePasswordCheckup_: boolean;
  private isButtonHidden_: boolean;
  private isMutePasswordButtonEnabled_: boolean;
  private isUnmutePasswordButtonEnabled_: boolean;
  private showPasswordEditDialog_: boolean;
  private showPasswordRemoveDialog_: boolean;
  private showPasswordEditDisclaimer_: boolean;
  private activePassword_: chrome.passwordsPrivate.PasswordUiEntry|null;
  private showCompromisedCredentialsBody_: boolean;
  private showMutedPasswordsSection_: boolean;
  private showNoCompromisedPasswordsLabel_: boolean;
  private showHideMenuTitle_: string;
  private iconHaloClass_: string;
  private clickedChangePasswordIds_: Set<number>;

  private activeListItem_: PasswordCheckListItemElement|null;
  startCheckAutomaticallySucceeded: boolean = false;
  private setSavedPasswordsListener_: SavedPasswordListChangedListener|null;

  constructor() {
    super();

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

    const setSavedPasswordsListener: SavedPasswordListChangedListener =
        _list => {
          this.startCheckAutomaticallySucceeded = false;
        };
    this.setSavedPasswordsListener_ = setSavedPasswordsListener;
    this.passwordManager!.addSavedPasswordListChangedListener(
        setSavedPasswordsListener);
  }

  override disconnectedCallback() {
    super.disconnectedCallback();

    assert(this.setSavedPasswordsListener_);
    this.passwordManager!.removeSavedPasswordListChangedListener(
        this.setSavedPasswordsListener_);
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
    return !!this.mutedPasswords.length;
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
  private getWeakPasswordsHelpText_(): TrustedHTML {
    return this.i18nAdvanced(
        this.isSyncingPasswords ? 'weakPasswordsDescriptionGeneration' :
                                  'weakPasswordsDescription');
  }

  private onMoreActionsClick_(
      event: CustomEvent<{moreActionsButton: HTMLElement}>) {
    const target = event.detail.moreActionsButton;
    this.$.moreActionsMenu.showAt(target);
    this.activeListItem_ = event.target as PasswordCheckListItemElement;
    this.activePassword_ = this.activeListItem_!.item;
  }

  private onMenuShowPasswordClick_() {
    this.activePassword_!.password ? this.activeListItem_!.hidePassword() :
                                     this.activeListItem_!.showPassword();
    this.$.moreActionsMenu.close();
    this.activePassword_ = null;
  }

  private onEditPasswordClick_() {
    assert(this.activePassword_);
    this.requestCredentialDetails(this.activePassword_.id)
        .then(
            credential => {
              this.activePassword_! = credential;
              this.showPasswordEditDialog_ = true;
              this.passwordManager!.recordPasswordCheckInteraction(
                  PasswordCheckInteraction.EDIT_PASSWORD);
            },
            _error => {
              // <if expr="not is_chromeos">
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
    this.passwordManager!.muteInsecureCredential(this.activeListItem_!.item);
  }

  private onMenuUnmuteMutedCompromisedPasswordClick_() {
    this.passwordManager!.recordPasswordCheckInteraction(
        PasswordCheckInteraction.UNMUTE_PASSWORD);
    this.$.moreActionsMenu.close();
    this.passwordManager!.unmuteInsecureCredential(this.activeListItem_!.item);
  }

  private onPasswordRemoveDialogClosed_() {
    this.showPasswordRemoveDialog_ = false;
  }

  private onPasswordEditDialogClosed_() {
    this.showPasswordEditDialog_ = false;
  }

  private onAlreadyChangedClick_(event: CustomEvent<HTMLElement>) {
    // Setting required properties for Password Check Edit dialog
    this.activeListItem_ = event.target as PasswordCheckListItemElement;
    this.activePassword_ = this.activeListItem_.item;

    this.showPasswordEditDisclaimer_ = true;
  }

  private onEditDisclaimerClosed_() {
    this.showPasswordEditDisclaimer_ = false;
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
  private computeTitle_(): TrustedHTML {
    switch (this.status.state) {
      case CheckState.IDLE:
        return this.waitsForFirstCheck_() ?
            window.trustedTypes!.emptyHTML :
            this.i18nAdvanced('checkedPasswords');
      case CheckState.CANCELED:
        return this.i18nAdvanced('checkPasswordsCanceled');
      case CheckState.RUNNING:
        // Returns the progress of a running check. Ensures that both numbers
        // are at least 1.
        const alreadyProcessed = this.status.alreadyProcessed || 0;
        return this.i18nAdvanced('checkPasswordsProgress', {
          substitutions: [
            String(alreadyProcessed + 1),
            String(
                Math.max(this.status.remainingInQueue! + alreadyProcessed, 1)),
          ],
        });
      case CheckState.OFFLINE:
        return this.i18nAdvanced('checkPasswordsErrorOffline');
      case CheckState.SIGNED_OUT:
        // When user is signed out we run the password weakness check. Since it
        // works very fast, we always shows "Checked passwords" in this case.
        return this.i18nAdvanced('checkedPasswords');
      case CheckState.NO_PASSWORDS:
        return this.i18nAdvanced('checkPasswordsErrorNoPasswords');
      case CheckState.QUOTA_LIMIT:
        // Note: For the checkup case we embed the link as HTML, thus we need to
        // use i18nAdvanced() here as well as the `inner-h-t-m-l` attribute in
        // the DOM.
        return this.canUsePasswordCheckup_ ?
            this.i18nAdvanced('checkPasswordsErrorQuotaGoogleAccount') :
            this.i18nAdvanced('checkPasswordsErrorQuota');
      case CheckState.OTHER_ERROR:
        return this.i18nAdvanced('checkPasswordsErrorGeneric');
      default:
        assertNotReached('Can\'t find a title for state: ' + this.status.state);
    }
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
   * @return true if the password is compromised and is dismissable/mutable.
   */
  private computeIsMutePasswordButtonEnabled_(): boolean {
    return this.isPasswordCompromised_() &&
        !this.activePassword_!.compromisedInfo!.isMuted;
  }

  /**
   * @return true if the password is compromised and is dismissed/muted.
   */
  private computeIsUnmutePasswordButtonEnabled_(): boolean {
    return this.isPasswordCompromised_() &&
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
      default:
        assertNotReached(
            'Can\'t find a button text for state: ' + this.status.state);
    }
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
      default:
        assertNotReached(
            'Can\'t determine button visibility for state: ' +
            this.status.state);
    }
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
      default:
        assertNotReached(
            'Not specified whether to state is an error: ' + this.status.state);
    }
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
      default:
        assertNotReached(
            'Not specified whether to show passwords for state: ' +
            this.status.state);
    }
  }

  /**
   * @return a localized and pluralized string of the passwords count, depending
   * on whether the user is signed in and whether other compromised passwords
   * exist.
   */
  private getPasswordsCount_(): string {
    return !this.signedIn && this.leakedPasswords.length === 0 ?
        this.weakPasswordsCount :
        this.insecurePasswordsCount;
  }

  /**
   * @return the label that should be shown in the compromised password section
   * if a user is signed out. This label depends on whether the user already had
   * compromised credentials that were found in the past.
   */
  private getSignedOutUserLabel_(): TrustedHTML {
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
   * @return whether the user can use the online Password Checkup.
   */
  private computeCanUsePasswordCheckup_(): boolean {
    return this.isSyncingPasswords &&
        (!this.syncPrefs || !this.syncPrefs.encryptAllData);
  }

  private computeShowCompromisedCredentialsBody_(): boolean {
    // Always shows compromised credentials section if user is signed out.
    return !this.signedIn || this.hasLeakedCredentials_() ||
        this.computeShowMutedLeakedCredentials_();
  }

  private computeShowNoCompromisedPasswordsLabel_(): boolean {
    // Check if user isn't signed in.
    if (!this.signedIn) {
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

  private clickedChangePassword_(item: chrome.passwordsPrivate.PasswordUiEntry):
      boolean {
    return this.clickedChangePasswordIds_.has(item.id);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-password-check': SettingsPasswordCheckElement;
  }
}

customElements.define(
    SettingsPasswordCheckElement.is, SettingsPasswordCheckElement);
