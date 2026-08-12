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
import '//resources/cr_elements/icons.html.js';
import '//resources/cr_elements/cr_icon/cr_icon.js';
import '/shared/settings/people_page/profile_info_browser_proxy.js';

import type {CrButtonElement} from '//resources/cr_elements/cr_button/cr_button.js';
import {WebUiListenerMixinLit} from '//resources/cr_elements/web_ui_listener_mixin_lit.js';
import {assert, assertNotReached, assertNotReachedCase} from '//resources/js/assert.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';
import type {PropertyValues} from '//resources/lit/v3_0/lit.rollup.js';
import type {StoredAccount, SyncBrowserProxy, SyncStatus} from '/shared/settings/people_page/sync_browser_proxy.js';
import {ChromeSigninAccessPoint, SignedInState, StatusAction, SyncBrowserProxyImpl} from '/shared/settings/people_page/sync_browser_proxy.js';
import {PrefServiceObserverMixinLit} from '/shared/settings/prefs2/pref_service_observer_mixin_lit.js';

import {loadTimeData} from '../i18n_setup.js';
import {routes} from '../route.js';
import type {Route} from '../router.js';
import {RouteObserverMixinLit, Router} from '../router.js';

import {getCss} from './sync_account_control.css.js';
import {getHtml} from './sync_account_control.html.js';

// <if expr="not is_chromeos">
export interface SettingsSyncAccountControlElement {
  $: {
    signIn: CrButtonElement,
  };
}
// </if>

export type SyncAccountControlElement = SettingsSyncAccountControlElement;

// Helper enum to determine which promo type the app should display. Used in the
// CSS styling, where the string literals are used for attributes matching.
enum PromoType {
  SIGNIN = 'signin',
  SYNC = 'sync',
}

const SettingsSyncAccountControlElementBase = WebUiListenerMixinLit(
    PrefServiceObserverMixinLit(RouteObserverMixinLit(CrLitElement)));

export class SettingsSyncAccountControlElement extends
    SettingsSyncAccountControlElementBase {
  static get is() {
    return 'settings-sync-account-control';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      /**
       * The current sync status, supplied by parent element.
       */
      syncStatus: {type: Object},

      // String to be used as a title when the promo has an account.
      promoLabelWithAccount: {type: String},

      // String to be used as title of the promo has no account.
      promoLabelWithNoAccount: {type: String},

      // String to be used as a subtitle when the promo has an account.
      promoSecondaryLabelWithAccount: {type: String},

      // String to be used as subtitle of the promo has no account.
      promoSecondaryLabelWithNoAccount: {type: String},

      storedAccounts_: {type: Array},
      profileAvatarURL_: {type: String},
      shownAccount_: {type: Object},

      // This property should be set by the parent only and should not change
      // after the element is created.
      embeddedInSubpage: {
        type: Boolean,
        reflect: true,
      },

      // This property should be set by the parent only and should not change
      // after the element is created.
      hideBanner: {
        type: Boolean,
        reflect: true,
      },

      // This property should be set by the parent only and should not change
      // after the element is created.
      accessPoint: {
        type: Number,
        reflect: true,
      },

      shouldShowAvatarRow_: {type: Boolean},
      subLabel_: {type: String},
      showSetupButtons_: {type: Boolean},

      // Reflected as `promo-type_` to be used in the CSS styling with
      // attributes matching.
      promoType_: {
        type: String,
        reflect: true,
      },

      signinAllowedOnNextStartupPref_: {type: Object},

      // <if expr="not is_chromeos">
      /**
       * Proxy variable for syncStatus.signedInState to shield observer from
       * being triggered multiple times whenever syncStatus changes.
       */
      syncing_: {type: Boolean},
      shouldShowSigninPausedButtons_: {type: Boolean},
      shouldShowSignInPromo_: {type: Boolean},
      // </if>
    };
  }

  accessor syncStatus: SyncStatus = {
    signedInState: SignedInState.SIGNED_OUT,
    signedInUsername: '',
    statusAction: StatusAction.NO_ACTION,
  };
  accessor promoLabelWithAccount: string = '';
  accessor promoLabelWithNoAccount: string = '';
  accessor promoSecondaryLabelWithAccount: string = '';
  accessor promoSecondaryLabelWithNoAccount: string = '';
  // <if expr="not is_chromeos">
  protected accessor syncing_: boolean = false;
  // </if>
  protected accessor storedAccounts_: StoredAccount[] = [];
  protected accessor profileAvatarURL_: string = '';
  protected accessor shownAccount_: StoredAccount|null = null;
  accessor embeddedInSubpage: boolean = false;
  accessor hideBanner: boolean = false;
  accessor accessPoint: ChromeSigninAccessPoint =
      ChromeSigninAccessPoint.SETTINGS;
  protected accessor shouldShowAvatarRow_: boolean = false;
  protected accessor subLabel_: string = '';
  protected accessor showSetupButtons_: boolean = false;
  protected accessor signinAllowedOnNextStartupPref_:
      chrome.settingsPrivate.PrefObject<boolean>|undefined;
  // <if expr="not is_chromeos">
  protected accessor shouldShowSigninPausedButtons_: boolean = false;
  private signinPausedImpressionRecorded_: boolean = false;
  protected accessor shouldShowSignInPromo_: boolean = false;
  private signinOfferedImpressionRecorded_: boolean = false;
  // </if>
  private syncBrowserProxy_: SyncBrowserProxy =
      SyncBrowserProxyImpl.getInstance();
  protected accessor promoType_: PromoType = PromoType.SYNC;

  override connectedCallback() {
    super.connectedCallback();

    this.mirrorPref(
        'signin.allowed_on_next_startup', 'signinAllowedOnNextStartupPref_');

    this.syncBrowserProxy_.getStoredAccounts().then(
        this.handleStoredAccounts_.bind(this));

    this.syncBrowserProxy_.getProfileAvatar().then(
        this.handleUpdateAvatar_.bind(this));

    this.addWebUiListener(
        'stored-accounts-updated', this.handleStoredAccounts_.bind(this));
    this.addWebUiListener(
        'profile-avatar-changed', this.handleUpdateAvatar_.bind(this));

    this.promoType_ =
        loadTimeData.getBoolean('replaceSyncPromosWithSignInPromos') ?
        PromoType.SIGNIN :
        PromoType.SYNC;
  }

  override willUpdate(changedProperties: PropertyValues<this>) {
    super.willUpdate(changedProperties);

    const changedPrivateProperties =
        changedProperties as Map<PropertyKey, unknown>;

    if (changedProperties.has('syncStatus') ||
        changedPrivateProperties.has('storedAccounts_')) {
      this.onShownAccountShouldChange_();
    }

    if (changedProperties.has('syncStatus') ||
        changedPrivateProperties.has('storedAccounts_')) {
      this.shouldShowAvatarRow_ = this.computeShouldShowAvatarRow_();
    }

    // <if expr="not is_chromeos">
    if (changedProperties.has('syncStatus')) {
      const isSyncing = this.isSyncing_();
      if (this.syncing_ !== isSyncing) {
        this.syncing_ = isSyncing;
        this.onSyncChanged_();
      }

      const shouldShowSigninPausedButtons =
          this.computeShouldShowSigninPausedButtons_();
      if (this.shouldShowSigninPausedButtons_ !==
          shouldShowSigninPausedButtons) {
        this.shouldShowSigninPausedButtons_ = shouldShowSigninPausedButtons;
        this.maybeRecordSigninPendingOffered_();
      }
    }

    if (changedProperties.has('syncStatus') ||
        changedPrivateProperties.has('promoType_')) {
      const shouldShowSignInPromo = this.computeShouldShowSignInPromo_();
      if (this.shouldShowSignInPromo_ !== shouldShowSignInPromo) {
        this.shouldShowSignInPromo_ = shouldShowSignInPromo;
        this.maybeRecordSignInOffered_();
      }
    }
    // </if>

    if (changedProperties.has('promoSecondaryLabelWithAccount') ||
        changedProperties.has('promoSecondaryLabelWithNoAccount') ||
        changedPrivateProperties.has('shownAccount_')) {
      this.subLabel_ = this.computeSubLabel_();
    }

    if (changedProperties.has('syncStatus')) {
      this.showSetupButtons_ = this.computeShowSetupButtons_();
    }
  }

  override updated(changedProperties: PropertyValues<this>) {
    super.updated(changedProperties);

    const changedPrivateProperties =
        changedProperties as Map<PropertyKey, unknown>;

    // <if expr="not is_chromeos">
    if (changedPrivateProperties.has('shouldShowAvatarRow_')) {
      this.onShouldShowAvatarRowChange_();
    }
    // </if>
  }

  override currentRouteChanged(_newRoute: Route, _oldRoute?: Route): void {
    // <if expr="not is_chromeos">
    this.maybeRecordSigninPendingOffered_();
    this.maybeRecordSignInOffered_();
    // </if>
  }

  // <if expr="not is_chromeos">
  /**
   * Records Signin_Impression_FromSettings user action.
   */
  private recordImpressionUserActions_() {
    assert(!this.isSyncing_());

    chrome.metricsPrivate.recordUserAction('Signin_Impression_FromSettings');
  }

  private onSyncChanged_() {
    if (this.embeddedInSubpage) {
      return;
    }

    if (!this.isSyncing_() && this.shownAccount_ !== undefined) {
      this.recordImpressionUserActions_();
    }
  }
  // </if>

  protected getLabel_(labelWithAccount: string, labelWithNoAccount: string):
      string {
    return this.shownAccount_ ? labelWithAccount : labelWithNoAccount;
  }

  private computeSubLabel_(): string {
    return this.getLabel_(
        this.promoSecondaryLabelWithAccount,
        this.promoSecondaryLabelWithNoAccount);
  }

  protected getAccountLabel_(): string {
    if (!this.shownAccount_) {
      return '';
    }
    const email = this.shownAccount_.email;
    // When in sign in paused, only show the email address.
    if (this.syncStatus.signedInState === SignedInState.SIGNED_IN_PAUSED) {
      return email;
    }

    if (this.syncStatus.firstSetupInProgress) {
      return this.syncStatus.statusText || email;
    }

    if (this.isSyncing_() && !this.syncStatus.hasError &&
        !this.syncStatus.disabled) {
      return loadTimeData.substituteString(
          loadTimeData.getString('syncingTo'), email);
    }

    return (this.shownAccount_.isPrimaryAccount &&
            this.promoType_ === PromoType.SYNC) ?
        loadTimeData.substituteString(
            loadTimeData.getString('signedInTo'), email) :
        email;
  }

  // Determines whether the subtitle should show account specific information or
  // not. This matters because showing account specific information needs to be
  // trimmed using ellipsis for potentially long texts, whereas fixed
  // information needs to be fully displayed regardless of the length.
  protected shouldHideSubtitleWithAccountInfoText_(): boolean {
    if (this.syncStatus.signedInState === SignedInState.SIGNED_IN_PAUSED) {
      return true;
    }

    if (this.syncStatus && this.syncStatus.hasError &&
        this.syncStatus.statusText) {
      return true;
    }

    if (this.syncStatus.signedInState === SignedInState.WEB_ONLY_SIGNED_IN) {
      return true;
    }

    return false;
  }

  protected getAvatarSubtitleLabel_(): string {
    if (!this.shownAccount_) {
      return '';
    }
    const email = this.shownAccount_.email;
    if (this.syncStatus.signedInState === SignedInState.WEB_ONLY_SIGNED_IN) {
      return loadTimeData.substituteString(
          loadTimeData.getString('accountAwareRowSubtitle'), email);
    }

    if (this.syncStatus.signedInState === SignedInState.SIGNED_IN_PAUSED) {
      return loadTimeData.substituteString(
          loadTimeData.getString('pendingStateAvatarRowSubtitle'), email);
    }

    if (this.syncStatus &&
        this.syncStatus.hasError && this.syncStatus.statusText) {
      if (this.syncStatus.statusAction === StatusAction.ENTER_PASSPHRASE) {
        return loadTimeData.substituteString(this.syncStatus.statusText, email);
      }

      return this.syncStatus.statusText;
    }
    return '';
  }

  protected getAccountAwareSigninButtonLabel_(): string {
    return loadTimeData.substituteString(
        loadTimeData.getString('accountAwareSigninButtonLabel'),
        this.shownAccount_?.givenName || '');
  }

  protected getProfileImageSrc_(image: string|null, profileAvatarURL: string):
      string {
    if (this.syncStatus.signedInState === SignedInState.WEB_ONLY_SIGNED_IN) {
      return profileAvatarURL;
    }

    // image can be undefined if the account has not set an avatar photo.
    return image || 'chrome://theme/IDR_PROFILE_AVATAR_PLACEHOLDER_LARGE';
  }

  protected getAccountImageSrc_(image: string|null): string {
    // image can be undefined if the account has not set an avatar photo.
    return image || 'chrome://theme/IDR_PROFILE_AVATAR_PLACEHOLDER_LARGE';
  }

  // <if expr="not is_chromeos">
  /**
   * @return The CSS class of the sync icon.
   */
  protected getSyncIconStyle_(): string {
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
  protected getSyncIcon_(): string {
    switch (this.getSyncIconStyle_()) {
      case 'sync-problem':
        return 'settings:sync-problem';
      case 'sync-paused':
        return 'settings:sync-disabled';
      default:
        return 'cr:sync';
    }
  }
  // </if>

  protected getAvatarRowTitle_(): string {
    if (this.syncStatus.signedInState === SignedInState.WEB_ONLY_SIGNED_IN) {
      return loadTimeData.getString('accountAwareRowTitle');
    }

    if (this.promoType_ === PromoType.SIGNIN &&
        this.syncStatus.signedInState === SignedInState.SIGNED_IN) {
      return this.shownAccount_?.fullName || '';
    }

    if (this.syncStatus && this.syncStatus.hasError &&
        this.syncStatus.statusText) {
      return this.shownAccount_?.fullName || '';
    }

    if (this.syncStatus.disabled) {
      return loadTimeData.getString('syncDisabled');
    }
    if (!this.syncStatus.hasError) {
      return this.shownAccount_?.fullName || '';
    }
    // Specific error cases below.
    if (this.syncStatus.hasUnrecoverableError) {
      return loadTimeData.getString('syncNotWorking');
    }
    if (this.syncStatus.statusAction === StatusAction.REAUTHENTICATE) {
      return loadTimeData.getString('syncPaused');
    }
    if (this.syncStatus.hasPasswordsOnlyError) {
      return loadTimeData.getString('syncPasswordsNotWorking');
    }
    return loadTimeData.getString('syncNotWorking');
  }

  // <if expr="not is_chromeos">
  /**
   * Determines if the signout button should be hidden.
   */
  protected shouldHideSignoutButton_(): boolean {
    if (this.syncStatus.domain) {
      return true;
    }

    return this.syncStatus.signedInState !== SignedInState.SIGNED_IN ||
        this.syncStatus.statusAction !== StatusAction.NO_ACTION;
  }

  /**
   * Determines if the remove account button should be hidden.
   */
  protected shouldHideRemoveAccountButton_(): boolean {
    return !!this.syncStatus.domain;
  }

  /**
   * Determines if the sync button should be disabled in response to
   * either a first setup flow or chrome sign-in being disabled.
   */
  protected shouldDisableSyncButton_(): boolean {
    if (!this.signinAllowedOnNextStartupPref_) {
      return this.computeShowSetupButtons_();
    }
    return !this.syncStatus || !!this.syncStatus.firstSetupInProgress ||
        !this.signinAllowedOnNextStartupPref_.value;
  }

  /**
   * Determines whether the banner should be hidden, in the case where the user
   * has sync enabled or if the property to hide the banner was explicitly set.
   */
  protected shouldHideBanner_(): boolean {
    if (this.hideBanner) {
      return true;
    }

    if (this.syncStatus && this.syncStatus.hasError &&
        this.syncStatus.statusText) {
      return true;
    }

    switch (this.syncStatus.signedInState) {
      case SignedInState.SIGNED_IN:
      case SignedInState.SIGNED_OUT:
      case SignedInState.WEB_ONLY_SIGNED_IN:
        return false;
      case SignedInState.SYNCING:
      case SignedInState.SIGNED_IN_PAUSED:
        return true;
      case undefined:
        assertNotReached('Invalid SignedInState');
      default:
        assertNotReachedCase(
            this.syncStatus.signedInState, 'Invalid SignedInState');
    }
  }

  /**
   * Determines whether the sync button should be hidden, in the case where
   * `replaceSyncPromosWithSignInPromos` is enabled, the user has sync enabled,
   * is in sign in paused, or if the property to hide the banner was explicitly
   * set.
   */
  protected shouldHideSyncButton_(): boolean {
    if (this.promoType_ === PromoType.SIGNIN) {
      return true;
    }

    if (this.syncStatus.signedInState === SignedInState.WEB_ONLY_SIGNED_IN) {
      return true;
    }

    if (this.syncStatus.statusAction !== StatusAction.NO_ACTION) {
      return true;
    }

    return !!this.syncStatus &&
        (this.isSyncing_() ||
         this.syncStatus.signedInState === SignedInState.SIGNED_IN_PAUSED);
  }

  protected shouldShowTurnOffButton_(): boolean {
    if (this.showSetupButtons_) {
      return false;
    }

    if (this.syncStatus.statusAction !== StatusAction.NO_ACTION) {
      return true;
    }

    return this.isSyncing_();
  }

  protected getTurnOffSyncLabel_(): string {
    if (this.syncStatus.hasError && this.syncStatus.secondaryButtonActionText &&
        this.isSyncing_()) {
      return this.syncStatus.secondaryButtonActionText;
    }

    if (this.syncStatus.statusAction !== StatusAction.NO_ACTION &&
        this.syncStatus.secondaryButtonActionText) {
      return this.syncStatus.secondaryButtonActionText;
    }
    return loadTimeData.getString('turnOffSync');
  }
  // </if>

  protected shouldShowErrorActionButton_(): boolean {
    if (this.showSetupButtons_) {
      return false;
    }

    // <if expr="is_chromeos">
    return this.syncStatus.statusAction === StatusAction.ENTER_PASSPHRASE ||
        this.syncStatus.statusAction ===
        StatusAction.SHOW_BOOKMARKS_LIMIT_HELP_ARTICLE;
    // </if>
    // <if expr="not is_chromeos">
    if (this.embeddedInSubpage &&
        this.syncStatus.statusAction === StatusAction.ENTER_PASSPHRASE) {
      // In the sync subpage the passphrase button is not required.
      return !this.isSyncing_();
    }

    if (this.syncStatus.statusAction !== StatusAction.NO_ACTION) {
      return true;
    }

    return this.isSyncing_() && !!this.syncStatus.hasError &&
        this.syncStatus.statusAction !== StatusAction.NO_ACTION;
    // </if>
  }

  // <if expr="not is_chromeos">
  protected shouldShowAccountAwareSigninButton_(): boolean {
    // Only show the button when user is in sync paused state
    return this.syncStatus.signedInState === SignedInState.WEB_ONLY_SIGNED_IN;
  }

  protected shouldAllowAccountSwitch_(): boolean {
    if (this.syncStatus.domain) {
      return false;
    }

    switch (this.syncStatus.signedInState) {
      case SignedInState.SIGNED_OUT:
      case SignedInState.WEB_ONLY_SIGNED_IN:
        return true;
      case SignedInState.SIGNED_IN_PAUSED:
      case SignedInState.SYNCING:
      case SignedInState.SIGNED_IN:
        return false;
      case undefined:
        assertNotReached('Invalid SignedInState');
      default:
        assertNotReachedCase(
            this.syncStatus.signedInState, 'Invalid SignedInState');
    }
  }
  // </if>

  private handleStoredAccounts_(accounts: StoredAccount[]) {
    this.storedAccounts_ = accounts;
  }

  private handleUpdateAvatar_(profileAvatarURL: string) {
    this.profileAvatarURL_ = profileAvatarURL;
  }

  private computeShouldShowAvatarRow_(): boolean {
    if (this.storedAccounts_ === undefined || this.syncStatus === undefined) {
      return false;
    }
    if (this.syncStatus.signedInState === SignedInState.WEB_ONLY_SIGNED_IN) {
      return true;
    }

    return (this.isSyncing_() || this.storedAccounts_.length > 0);
  }

  protected onErrorButtonClick_() {
    // <if expr="not is_chromeos">
    const router = Router.getInstance();
    const routes = router.getRoutes();
    // </if>
    switch (this.syncStatus.statusAction) {
      // <if expr="not is_chromeos">
      case StatusAction.REAUTHENTICATE:
        this.syncBrowserProxy_.startSignIn(this.accessPoint);
        break;
      case StatusAction.UPGRADE_CLIENT:
        router.navigateTo(routes.ABOUT);
        break;
      case StatusAction.RETRIEVE_TRUSTED_VAULT_KEYS:
        this.syncBrowserProxy_.startKeyRetrieval();
        break;
      // </if>
      case StatusAction.ENTER_PASSPHRASE:
        this.syncBrowserProxy_.showSyncPassphraseDialog();
        break;
      case StatusAction.SHOW_BOOKMARKS_LIMIT_HELP_ARTICLE:
        this.syncBrowserProxy_.showBookmarkLimitExceededHelp();
        break;
      // <if expr="not is_chromeos">
      case StatusAction.CONFIRM_SYNC_SETTINGS:
      // </if>
      default:
        // <if expr="is_chromeos">
        assertNotReached();
        // </if>
        // <if expr="not is_chromeos">
        router.navigateTo(routes.SYNC);
        // </if>
    }
  }

  // <if expr="not is_chromeos">
  protected onSigninClick_() {
    this.syncBrowserProxy_.startSignIn(this.accessPoint);
    // Need to close here since one menu item also triggers this function.
    const actionMenu = this.shadowRoot.querySelector('cr-action-menu');
    if (actionMenu) {
      actionMenu.close();
    }
  }

  protected onSignoutClick_() {
    this.syncBrowserProxy_.signOut(false /* deleteProfile */);

    const actionMenu = this.shadowRoot.querySelector('cr-action-menu');
    if (actionMenu) {
      actionMenu.close();
    }
  }

  protected onDropdownClose_() {
    const menuAnchor =
        this.shadowRoot.querySelector<HTMLElement>('#dropdown-arrow');
    assert(menuAnchor);
    menuAnchor.setAttribute('aria-expanded', 'false');
  }

  protected onSyncButtonClick_() {
    assert(this.shownAccount_);
    assert(this.storedAccounts_.length > 0);
    const isDefaultPromoAccount =
        (this.shownAccount_.email === this.storedAccounts_[0].email);

    this.syncBrowserProxy_.startSyncingWithEmail(
        this.shownAccount_.email, isDefaultPromoAccount);
  }

  protected onTurnOffButtonClick_() {
    /* This will route to people_page's disconnect dialog. */
    if (!this.isSyncing_() &&
        this.syncStatus.statusAction !== StatusAction.NO_ACTION) {
      this.onSignoutClick_();
    }
    const router = Router.getInstance();
    router.navigateTo(router.getRoutes().SIGN_OUT);
  }

  protected onMenuButtonClick_() {
    const actionMenu = this.shadowRoot.querySelector('cr-action-menu');
    assert(actionMenu);
    const anchor =
        this.shadowRoot.querySelector<HTMLElement>('#dropdown-arrow');
    assert(anchor);
    actionMenu.showAt(anchor);
    anchor.setAttribute('aria-expanded', 'true');
  }

  private onShouldShowAvatarRowChange_() {
    // Close dropdown when avatar-row hides, so if it appears again, the menu
    // won't be open by default.
    const actionMenu = this.shadowRoot.querySelector('cr-action-menu');
    if (!this.shouldShowAvatarRow_ && actionMenu && actionMenu.open) {
      actionMenu.close();
    }
  }

  protected onAccountClick_(e: Event) {
    const target = e.currentTarget as HTMLElement;
    const index = Number(target.dataset['index']);
    this.shownAccount_ = this.storedAccounts_[index];
    this.shadowRoot.querySelector('cr-action-menu')!.close();
  }
  // </if>

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

      // <if expr="not is_chromeos">
      // Sign-in impressions should be recorded in the following cases:
      // 1. When the promo is first shown, i.e. when |shownAccount_| is
      //   initialized;
      // 2. When the impression account state changes, i.e. promo impression
      //   state changes (WithAccount -> WithNoAccount) or
      //   (WithNoAccount -> WithAccount).
      const shouldRecordImpression = (this.shownAccount_ === undefined) ||
          (!this.shownAccount_ && firstStoredAccount) ||
          (this.shownAccount_ && !firstStoredAccount);
      // </if>

      this.shownAccount_ = firstStoredAccount;

      // <if expr="not is_chromeos">
      if (shouldRecordImpression) {
        this.recordImpressionUserActions_();
      }
      // </if>
    }
  }

  private computeShowSetupButtons_(): boolean {
    return !!this.syncStatus && !!this.syncStatus.firstSetupInProgress;
  }

  // <if expr="not is_chromeos">
  protected onSetupCancelClick_() {
    this.fire('sync-setup-done', false);
  }

  protected onSetupConfirmClick_() {
    this.fire('sync-setup-done', true);
  }

  private computeShouldShowSigninPausedButtons_() {
    return !!this.syncStatus &&
        this.syncStatus.signedInState === SignedInState.SIGNED_IN_PAUSED;
  }

  private computeShouldShowSignInPromo_() {
    if (!this.syncStatus) {
      return false;
    }
    const state = this.syncStatus.signedInState;
    return state === SignedInState.SIGNED_OUT ||
        state === SignedInState.WEB_ONLY_SIGNED_IN;
  }

  private maybeRecordSignInOffered_() {
    if (!this.shouldShowSignInPromo_) {
      return;
    }

    // Only record if we are currently on a page that could have an account
    // control in promo state.
    const currentRoute = Router.getInstance().getCurrentRoute();
    if (![routes.BASIC, routes.PEOPLE, routes.AUTOFILL].includes(
            currentRoute)) {
      return;
    }

    // Only record for account controls that are visible.
    if (this.embeddedInSubpage) {
      return;
    }

    // Don't record twice.
    if (this.signinOfferedImpressionRecorded_) {
      return;
    }

    this.syncBrowserProxy_.recordSigninOffered(this.accessPoint);
    this.signinOfferedImpressionRecorded_ = true;
  }

  private maybeRecordSigninPendingOffered_() {
    if (!this.shouldShowSigninPausedButtons_) {
      return;
    }

    // Only record if we are currently on a page that could have an account
    // control in pending state.
    const currentRoute = Router.getInstance().getCurrentRoute();
    if (![routes.BASIC, routes.PEOPLE, routes.AUTOFILL].includes(
            currentRoute)) {
      return;
    }

    // Only record for account controls that are visible in pending state.
    if (this.embeddedInSubpage) {
      return;
    }

    // Don't record twice.
    if (this.signinPausedImpressionRecorded_) {
      return;
    }

    this.syncBrowserProxy_.recordSigninPendingOffered();
    this.signinPausedImpressionRecorded_ = true;
  }
  // </if>

  protected isSyncing_(): boolean {
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
