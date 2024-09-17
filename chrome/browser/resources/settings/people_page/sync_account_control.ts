// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
/**
 * @fileoverview
 * 'settings-sync-account-section' is the settings page containing sign-in
 * settings.
 */
import '//resources/cr_elements/cr_action_menu/cr_action_menu.js';
import '//resources/cr_elements/cr_button/cr_button.js';
import '//resources/cr_elements/cr_icon_button/cr_icon_button.js';
import '//resources/cr_elements/icons_lit.html.js';
import '//resources/cr_elements/cr_shared_style.css.js';
import '//resources/cr_elements/cr_shared_vars.css.js';
import '//resources/cr_elements/cr_icon/cr_icon.js';
import '/shared/settings/people_page/profile_info_browser_proxy.js';
import '../icons.html.js';
import '/shared/settings/prefs/prefs.js';
import '../settings_shared.css.js';

import type {CrButtonElement} from '//resources/cr_elements/cr_button/cr_button.js';
import {WebUiListenerMixin} from '//resources/cr_elements/web_ui_listener_mixin.js';
import {assert} from '//resources/js/assert.js';
import type {DomRepeatEvent} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import type {StoredAccount, SyncBrowserProxy, SyncStatus} from '/shared/settings/people_page/sync_browser_proxy.js';
import {SignedInState, StatusAction, SyncBrowserProxyImpl} from '/shared/settings/people_page/sync_browser_proxy.js';
import {PrefsMixin} from '/shared/settings/prefs/prefs_mixin.js';

import {loadTimeData} from '../i18n_setup.js';
import {Router} from '../router.js';

import {getTemplate} from './sync_account_control.html.js';

export const MAX_SIGNIN_PROMO_IMPRESSION: number = 10;

export interface SettingsSyncAccountControlElement {
  $: {
    signIn: CrButtonElement,
  };
}

const SettingsSyncAccountControlElementBase =
    WebUiListenerMixin(PrefsMixin(PolymerElement));

export class SettingsSyncAccountControlElement extends
    SettingsSyncAccountControlElementBase {
  static get is() {
    return 'settings-sync-account-control';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      /**
       * Preferences state.
       */
      prefs: {
        type: Object,
        notify: true,
      },

      /**
       * The current sync status, supplied by parent element.
       */
      syncStatus: Object,

      // String to be used as a title when the promo has an account.
      promoLabelWithAccount: String,

      // String to be used as title of the promo has no account.
      promoLabelWithNoAccount: String,

      // String to be used as a subtitle when the promo has an account.
      promoSecondaryLabelWithAccount: String,

      // String to be used as subtitle of the promo has no account.
      promoSecondaryLabelWithNoAccount: String,

      /**
       * Proxy variable for syncStatus.signedInState to shield observer from
       * being triggered multiple times whenever syncStatus changes.
       */
      syncing_: {
        type: Boolean,
        computed: 'isSyncing_(syncStatus.signedInState)',
        observer: 'onSyncChanged_',
      },

      storedAccounts_: Object,

      shownAccount_: Object,

      showingPromo: {
        type: Boolean,
        value: false,
        reflectToAttribute: true,
      },

      // This property should be set by the parent only and should not change
      // after the element is created.
      embeddedInSubpage: {
        type: Boolean,
        reflectToAttribute: true,
      },

      // This property should be set by the parent only and should not change
      // after the element is created.
      hideButtons: {
        type: Boolean,
        value: false,
        reflectToAttribute: true,
      },

      // This property should be set by the parent only and should not change
      // after the element is created.
      hideBanner: {
        type: Boolean,
        value: false,
        reflectToAttribute: true,
      },

      shouldShowAvatarRow_: {
        type: Boolean,
        value: false,
        computed: 'computeShouldShowAvatarRow_(storedAccounts_, syncStatus,' +
            'storedAccounts_.length, syncStatus.signedInState)',
        observer: 'onShouldShowAvatarRowChange_',
      },

      subLabel_: {
        type: String,
        computed: 'computeSubLabel_(promoSecondaryLabelWithAccount,' +
            'promoSecondaryLabelWithNoAccount, shownAccount_)',
      },

      showSetupButtons_: {
        type: Boolean,
        computed: 'computeShowSetupButtons_(' +
            'hideButtons, syncStatus.firstSetupInProgress)',
      },
    };
  }

  static get observers() {
    return [
      'onShownAccountShouldChange_(storedAccounts_, syncStatus)',
    ];
  }

  syncStatus: SyncStatus;
  promoLabelWithAccount: string;
  promoLabelWithNoAccount: string;
  promoSecondaryLabelWithAccount: string;
  promoSecondaryLabelWithNoAccount: string;
  private syncing_: boolean;
  private storedAccounts_: StoredAccount[];
  private shownAccount_: StoredAccount|null;
  showingPromo: boolean;
  embeddedInSubpage: boolean;
  hideButtons: boolean;
  hideBanner: boolean;
  private shouldShowAvatarRow_: boolean;
  private subLabel_: string;
  private showSetupButtons_: boolean;
  private syncBrowserProxy_: SyncBrowserProxy =
      SyncBrowserProxyImpl.getInstance();

  override connectedCallback() {
    super.connectedCallback();

    this.syncBrowserProxy_.getStoredAccounts().then(
        this.handleStoredAccounts_.bind(this));
    this.addWebUiListener(
        'stored-accounts-updated', this.handleStoredAccounts_.bind(this));
  }

  /**
   * Records Signin_Impression_FromSettings user action.
   */
  private recordImpressionUserActions_() {
    assert(!this.isSyncing_());

    chrome.metricsPrivate.recordUserAction('Signin_Impression_FromSettings');
  }

  private onSyncChanged_() {
    if (this.embeddedInSubpage) {
      this.showingPromo = true;
      return;
    }

    if (!this.showingPromo && !this.isSyncing_() &&
        this.syncBrowserProxy_.getPromoImpressionCount() <
            MAX_SIGNIN_PROMO_IMPRESSION) {
      this.showingPromo = true;
      this.syncBrowserProxy_.incrementPromoImpressionCount();
    } else {
      // Turn off the promo if the user is signed in.
      this.showingPromo = false;
    }
    if (!this.isSyncing_() && this.shownAccount_ !== undefined) {
      this.recordImpressionUserActions_();
    }
  }

  private getLabel_(labelWithAccount: string, labelWithNoAccount: string):
      string {
    return this.shownAccount_ ? labelWithAccount : labelWithNoAccount;
  }

  private computeSubLabel_(): string {
    return this.getLabel_(
        this.promoSecondaryLabelWithAccount,
        this.promoSecondaryLabelWithNoAccount);
  }

  private getSubstituteLabel_(label: string, name: string): string {
    return loadTimeData.substituteString(label, name);
  }

  private getAccountLabel_(
      signedInLabel: string, syncingLabel: string, email: string): string {
    // When in sign in paused, only show the email address.
    if (this.syncStatus.signedInState === SignedInState.SIGNED_IN_PAUSED) {
      return email;
    }

    if (this.syncStatus.firstSetupInProgress) {
      return this.syncStatus.statusText || email;
    }

    if (this.isSyncing_() && !this.syncStatus.hasError &&
        !this.syncStatus.disabled) {
      return loadTimeData.substituteString(syncingLabel, email);
    }

    return (this.shownAccount_! && this.shownAccount_!!.isPrimaryAccount) ?
        loadTimeData.substituteString(signedInLabel, email) :
        email;
  }

  private getAccountImageSrc_(image: string|null): string {
    // image can be undefined if the account has not set an avatar photo.
    return image || 'chrome://theme/IDR_PROFILE_AVATAR_PLACEHOLDER_LARGE';
  }

  /**
   * @return The CSS class of the sync icon.
   */
  private getSyncIconStyle_(): string {
    if (this.syncStatus.disabled) {
      return 'sync-disabled';
    }
    if (!this.syncStatus.hasError) {
      return 'sync';
    }
    // Specific error cases below.
    if (this.syncStatus.hasUnrecoverableError) {
      return 'sync-problem';
    }
    if (this.syncStatus.statusAction === StatusAction.REAUTHENTICATE) {
      return 'sync-paused';
    }
    return 'sync-problem';
  }

  /**
   * Returned value must match one of iron-icon's settings:(*) icon name.
   */
  private getSyncIcon_(): string {
    switch (this.getSyncIconStyle_()) {
      case 'sync-problem':
        return 'settings:sync-problem';
      case 'sync-paused':
        return 'settings:sync-disabled';
      default:
        return 'cr:sync';
    }
  }

  private getAvatarRowTitle_(
      accountName: string, syncErrorLabel: string,
      syncPasswordsOnlyErrorLabel: string, authErrorLabel: string,
      disabledLabel: string): string {
    if (this.syncStatus.disabled) {
      return disabledLabel;
    }
    if (!this.syncStatus.hasError) {
      return accountName;
    }
    // Specific error cases below.
    if (this.syncStatus.hasUnrecoverableError) {
      return syncErrorLabel;
    }
    if (this.syncStatus.statusAction === StatusAction.REAUTHENTICATE) {
      return authErrorLabel;
    }
    if (this.syncStatus.hasPasswordsOnlyError) {
      return syncPasswordsOnlyErrorLabel;
    }
    return syncErrorLabel;
  }

  /**
   * Determines if the sync button should be disabled in response to
   * either a first setup flow or chrome sign-in being disabled.
   */
  private shouldDisableSyncButton_(): boolean {
    if (this.hideButtons || this.prefs === undefined) {
      return this.computeShowSetupButtons_();
    }
    return !!this.syncStatus.firstSetupInProgress ||
        !this.getPref('signin.allowed_on_next_startup').value;
  }

  /**
   * Determines whether the banner should be hidden, in the case where the user
   * has sync enabled or if the property to hide the banner was explicitly set.
   */
  private shouldHideBanner_(): boolean {
    return this.hideBanner || (!!this.syncStatus && this.isSyncing_());
  }

  /**
   * Determines whether the sync button should be hidden, in the case where the
   * user has sync enabled, is in sign in paused, or if the property to hide
   * the banner was explicitly set.
   */
  private shouldHideSyncButton_(): boolean {
    return this.hideButtons ||
        (!!this.syncStatus &&
         (this.isSyncing_() ||
          this.syncStatus.signedInState === SignedInState.SIGNED_IN_PAUSED));
  }

  private shouldShowTurnOffButton_(): boolean {
    return !this.hideButtons && !this.showSetupButtons_ && this.isSyncing_();
  }

  private shouldShowErrorActionButton_(): boolean {
    if (this.embeddedInSubpage &&
        this.syncStatus.statusAction === StatusAction.ENTER_PASSPHRASE) {
      // In a subpage the passphrase button is not required.
      return false;
    }
    return !this.hideButtons && !this.showSetupButtons_ && this.isSyncing_() &&
        !!this.syncStatus.hasError &&
        this.syncStatus.statusAction !== StatusAction.NO_ACTION;
  }

  private shouldAllowAccountSwitch_(): boolean {
    // <if expr="chromeos_lacros">
    if (!loadTimeData.getBoolean('isSecondaryUser')) {
      // Sync account can't be changed in the main profile, it is always the
      // device account.
      return false;
    }
    // </if>
    return !this.hideButtons && !this.isSyncing_() &&
        this.syncStatus.signedInState !== SignedInState.SIGNED_IN_PAUSED &&
        (!loadTimeData.getBoolean('turnOffSyncAllowedForManagedProfiles') ||
         !this.syncStatus.domain);
  }

  private handleStoredAccounts_(accounts: StoredAccount[]) {
    this.storedAccounts_ = accounts;
  }

  private computeShouldShowAvatarRow_(): boolean {
    if (this.storedAccounts_ === undefined || this.syncStatus === undefined) {
      return false;
    }

    return (this.isSyncing_() || this.storedAccounts_.length > 0) &&
        this.syncStatus.signedInState !== SignedInState.WEB_ONLY_SIGNED_IN;
  }

  private onErrorButtonClick_() {
    const router = Router.getInstance();
    const routes = router.getRoutes();
    switch (this.syncStatus.statusAction) {
      case StatusAction.REAUTHENTICATE:
        this.syncBrowserProxy_.startSignIn();
        break;
      case StatusAction.UPGRADE_CLIENT:
        router.navigateTo(routes.ABOUT);
        break;
      case StatusAction.RETRIEVE_TRUSTED_VAULT_KEYS:
        this.syncBrowserProxy_.startKeyRetrieval();
        break;
      case StatusAction.ENTER_PASSPHRASE:
      case StatusAction.CONFIRM_SYNC_SETTINGS:
      default:
        router.navigateTo(routes.SYNC);
    }
  }

  private onSigninClick_() {
    this.syncBrowserProxy_.startSignIn();
    // Need to close here since one menu item also triggers this function.
    const actionMenu = this.shadowRoot!.querySelector('cr-action-menu');
    if (actionMenu) {
      actionMenu.close();
    }
  }

  private onSignoutClick_() {
    this.syncBrowserProxy_.signOut(false /* deleteProfile */);
    this.shadowRoot!.querySelector('cr-action-menu')!.close();
  }

  private onSyncButtonClick_() {
    assert(this.shownAccount_);
    assert(this.storedAccounts_.length > 0);
    const isDefaultPromoAccount =
        (this.shownAccount_!.email === this.storedAccounts_[0].email);

    this.syncBrowserProxy_.startSyncingWithEmail(
        this.shownAccount_!.email, isDefaultPromoAccount);
  }

  private onTurnOffButtonClick_() {
    /* This will route to people_page's disconnect dialog. */
    const router = Router.getInstance();
    router.navigateTo(router.getRoutes().SIGN_OUT);
  }

  private onMenuButtonClick_() {
    const actionMenu = this.shadowRoot!.querySelector('cr-action-menu');
    assert(actionMenu);
    const anchor =
        this.shadowRoot!.querySelector<HTMLElement>('#dropdown-arrow');
    assert(anchor);
    actionMenu.showAt(anchor);
  }

  private onShouldShowAvatarRowChange_() {
    // Close dropdown when avatar-row hides, so if it appears again, the menu
    // won't be open by default.
    const actionMenu = this.shadowRoot!.querySelector('cr-action-menu');
    if (!this.shouldShowAvatarRow_ && actionMenu && actionMenu.open) {
      actionMenu.close();
    }
  }

  private onAccountClick_(e: DomRepeatEvent<StoredAccount>) {
    this.shownAccount_ = e.model.item;
    this.shadowRoot!.querySelector('cr-action-menu')!.close();
  }

  private onShownAccountShouldChange_() {
    if (this.storedAccounts_ === undefined || this.syncStatus === undefined) {
      return;
    }

    if (this.isSyncing_()) {
      for (let i = 0; i < this.storedAccounts_.length; i++) {
        if (this.storedAccounts_[i].email ===
            this.syncStatus.signedInUsername) {
          this.shownAccount_ = this.storedAccounts_[i];
          return;
        }
      }
    } else {
      const firstStoredAccount =
          (this.storedAccounts_.length > 0) ? this.storedAccounts_[0] : null;

      // Sign-in impressions should be recorded in the following cases:
      // 1. When the promo is first shown, i.e. when |shownAccount_| is
      //   initialized;
      // 2. When the impression account state changes, i.e. promo impression
      //   state changes (WithAccount -> WithNoAccount) or
      //   (WithNoAccount -> WithAccount).
      const shouldRecordImpression = (this.shownAccount_ === undefined) ||
          (!this.shownAccount_ && firstStoredAccount) ||
          (this.shownAccount_ && !firstStoredAccount);

      this.shownAccount_ = firstStoredAccount;

      if (shouldRecordImpression) {
        this.recordImpressionUserActions_();
      }
    }
  }

  private computeShowSetupButtons_(): boolean {
    return !this.hideButtons && !!this.syncStatus.firstSetupInProgress;
  }

  private onSetupCancel_() {
    this.dispatchEvent(new CustomEvent(
        'sync-setup-done', {bubbles: true, composed: true, detail: false}));
  }

  private onSetupConfirm_() {
    this.dispatchEvent(new CustomEvent(
        'sync-setup-done', {bubbles: true, composed: true, detail: true}));
  }

  private shouldShowSigninPausedButtons_() {
    return !!this.syncStatus &&
        this.syncStatus.signedInState === SignedInState.SIGNED_IN_PAUSED;
  }

  private isSyncing_(): boolean {
    return this.syncStatus.signedInState === SignedInState.SYNCING;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-sync-account-control': SettingsSyncAccountControlElement;
  }
}

customElements.define(
    SettingsSyncAccountControlElement.is, SettingsSyncAccountControlElement);
