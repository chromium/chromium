// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
/**
 * @fileoverview
 * 'settings-account-card' component is a card that shows the user's account
 * name and picture with optional controls.
 * It's a slimmed down copy of
 * chrome/browser/resources/settings/people_page/people_page.ts
 */
import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';
import 'chrome://resources/cr_elements/cr_link_row/cr_link_row.js';
import 'chrome://resources/cr_elements/cr_shared_style.css.js';
import 'chrome://resources/cr_elements/cr_shared_vars.css.js';
import 'chrome://resources/cr_elements/icons.html.js';
import 'chrome://resources/cr_elements/policy/cr_policy_indicator.js';
import '../controls/settings_toggle_button.js';
// <if expr="not is_chromeos">
import '../people_page/sync_account_control.js';
// </if>
import '../icons.html.js';
import '../settings_shared.css.js';

import type {ProfileInfo} from '/shared/settings/people_page/profile_info_browser_proxy.js';
import {ProfileInfoBrowserProxyImpl} from '/shared/settings/people_page/profile_info_browser_proxy.js';
import type {StoredAccount, SyncBrowserProxy, SyncStatus} from '/shared/settings/people_page/sync_browser_proxy.js';
import {ChromeSigninAccessPoint, SignedInState, StatusAction, SyncBrowserProxyImpl} from '/shared/settings/people_page/sync_browser_proxy.js';
import {WebUiListenerMixin} from 'chrome://resources/cr_elements/web_ui_listener_mixin.js';
import {getImage} from 'chrome://resources/js/icon.js';
import {OpenWindowProxyImpl} from 'chrome://resources/js/open_window_proxy.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {loadTimeData} from '../i18n_setup.js';
import {routes} from '../route.js';
import {Router} from '../router.js';

// clang-format off
// <if expr="is_chromeos">
import {convertImageSequenceToPng} from 'chrome://resources/ash/common/cr_picture/png.js';

import {AccountManagerBrowserProxyImpl} from '../people_page/account_manager_browser_proxy.js';
// </if>
// clang-format on



import {getTemplate} from './account_card.html.js';

const SettingsAccountCardElementBase = WebUiListenerMixin(PolymerElement);

export class SettingsAccountCardElement extends SettingsAccountCardElementBase {
  static get is() {
    return 'settings-account-card';
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
       * This flag is used to conditionally show a set of new sign-in UIs to the
       * profiles that have been migrated to be consistent with the web
       * sign-ins.
       */
      signinAllowed_: {
        type: Boolean,
        value() {
          return loadTimeData.getBoolean('signinAllowed');
        },
      },

      /**
       * The current sync status, supplied by SyncBrowserProxy.
       */
      syncStatus: Object,

      // <if expr="not is_chromeos">
      /**
       * Stored accounts to the system, supplied by SyncBrowserProxy.
       */
      storedAccounts: Object,

      replaceSyncPromosWithSignInPromos_: {
        type: Boolean,
        value: () =>
            loadTimeData.getBoolean('replaceSyncPromosWithSignInPromos'),
      },

      primaryAccountName_: String,
      primaryAccountEmail_: String,
      primaryAccountIconUrl_: String,

      /** Expose ChromeSigninAccessPoint enum to HTML bindings. */
      accessPointEnum_: {
        type: Object,
        value: ChromeSigninAccessPoint,
      },
      // </if>

      // <if expr="is_chromeos">
      /**
       * The currently selected profile icon URL. May be a data URL.
       */
      profileIconUrl_: String,

      /**
       * The current profile name.
       */
      profileName_: String,

      /**
       * Whether the profile row is clickable. The behavior depends on the
       * platform.
       */
      isProfileActionable_: {
        type: Boolean,
        value: () => loadTimeData.getBoolean('isAccountManagerEnabled'),
        readOnly: true,
      },
      // </if>
    };
  }

  declare prefs: {[key: string]: any};
  declare private signinAllowed_: boolean;
  declare syncStatus: SyncStatus|null;

  // <if expr="not is_chromeos">
  declare storedAccounts: StoredAccount[]|null;
  declare private replaceSyncPromosWithSignInPromos_: boolean;
  declare private primaryAccountName_: string;
  declare private primaryAccountEmail_: string;
  declare private primaryAccountIconUrl_: string;
  // </if>

  // <if expr="is_chromeos">
  declare private profileIconUrl_: string;
  declare private profileName_: string;
  declare private isProfileActionable_: boolean;
  // </if>

  private syncBrowserProxy_: SyncBrowserProxy =
      SyncBrowserProxyImpl.getInstance();

  override connectedCallback() {
    super.connectedCallback();

    // <if expr="is_chromeos">
    if (loadTimeData.getBoolean('isAccountManagerEnabled')) {
      // If this is SplitSettings and we have the Google Account manager,
      // prefer the GAIA name and icon.
      this.addWebUiListener(
          'accounts-changed', this.updateAccounts_.bind(this));
      this.updateAccounts_();
    } else {
      ProfileInfoBrowserProxyImpl.getInstance().getProfileInfo().then(
          this.handleProfileInfo_.bind(this));
      this.addWebUiListener(
          'profile-info-changed', this.handleProfileInfo_.bind(this));
    }
    // </if>

    this.syncBrowserProxy_.getSyncStatus().then(
        this.handleSyncStatus_.bind(this));
    this.addWebUiListener(
        'sync-status-changed', this.handleSyncStatus_.bind(this));

    // <if expr="not is_chromeos">
    this.syncBrowserProxy_.getStoredAccounts().then(
        this.handleStoredAccounts_.bind(this));
    this.addWebUiListener(
        'stored-accounts-updated', this.handleStoredAccounts_.bind(this));
    // </if>
  }

  // <if expr="is_chromeos">
  /**
   * Handler for when the profile's icon and name is updated.
   */
  private handleProfileInfo_(info: ProfileInfo) {
    this.profileName_ = info.name;
    /**
     * Extract first frame from image by creating a single frame PNG using
     * url as input if base64 encoded and potentially animated.
     */
    if (info.iconUrl.startsWith('data:image/png;base64')) {
      this.profileIconUrl_ = convertImageSequenceToPng([info.iconUrl]);
      return;
    }

    this.profileIconUrl_ = info.iconUrl;
  }

  private async updateAccounts_() {
    const accounts =
        await AccountManagerBrowserProxyImpl.getInstance().getAccounts();
    // The user might not have any GAIA accounts (e.g. guest mode or Active
    // Directory). In these cases the profile row is hidden, so there's nothing
    // to do.
    if (accounts.length === 0) {
      return;
    }
    this.profileName_ = accounts[0].fullName;
    this.profileIconUrl_ = accounts[0].pic;
  }

  private onProfileClick_() {
    if (loadTimeData.getBoolean('isAccountManagerEnabled')) {
      // Post-SplitSettings. The browser C++ code loads OS settings in a window.
      OpenWindowProxyImpl.getInstance().openUrl(
          loadTimeData.getString('osSettingsAccountsPageUrl'));
    }
  }
  // </if>

  /**
   * Handler for when the sync state is pushed from the browser.
   */
  private handleSyncStatus_(syncStatus: SyncStatus) {
    // <if expr="is_chromeos">
    this.syncStatus = syncStatus;
    // </if>
    // <if expr="not is_chromeos">
    // Sign-in impressions should be recorded only if the sign-in promo is
    // shown. They should be recorder only once, the first time
    // |this.syncStatus| is set.
    // With `ReplaceSyncPromosWithSignInPromos`, this is not a sign in promo, so
    // we should not record.
    const shouldRecordSigninImpression = !this.syncStatus && syncStatus &&
        this.signinAllowed_ && !this.isSyncing_() &&
        !this.replaceSyncPromosWithSignInPromos_;

    this.syncStatus = syncStatus;

    if (shouldRecordSigninImpression && !this.shouldShowSyncAccountControl_()) {
      // SyncAccountControl records the impressions user actions.
      chrome.metricsPrivate.recordUserAction('Signin_Impression_FromSettings');
    }
    // </if>
  }

  // <if expr="not is_chromeos">
  private onAccountClick_() {
    Router.getInstance().navigateTo(routes.ACCOUNT);
  }

  private shouldLinkToAccountSettingsPage_(): boolean {
    return this.replaceSyncPromosWithSignInPromos_ && !!this.syncStatus &&
        this.syncStatus.signedInState === SignedInState.SIGNED_IN;
  }

  private shouldShowSyncAccountControl_(): boolean {
    if (this.syncStatus === undefined) {
      return false;
    }
    return !!this.syncStatus!.syncSystemEnabled && this.signinAllowed_ &&
        !this.shouldLinkToAccountSettingsPage_();
  }

  private handleStoredAccounts_(accounts: StoredAccount[]) {
    this.storedAccounts = accounts;

    // The user might not have any GAIA accounts (e.g. signed out). In this case
    // the link row to the account settings page does not exist, so there's
    // nothing to do.
    if (accounts.length === 0) {
      return;
    }
    this.primaryAccountName_ = accounts[0].fullName!;
    this.primaryAccountEmail_ = accounts[0].email;
    this.primaryAccountIconUrl_ = accounts[0].avatarImage!;
  }

  private getAccountRowSubtitle_(): string {
    if (!!this.syncStatus && !!this.syncStatus.statusText &&
        this.syncStatus.statusAction === StatusAction.ENTER_PASSPHRASE) {
      return loadTimeData.substituteString(
          this.syncStatus.statusText, this.primaryAccountEmail_);
    }

    return this.primaryAccountEmail_;
  }
  // </if>

  /**
   * @return A CSS image-set for multiple scale factors.
   */
  private getIconImageSet_(iconUrl?: string): string {
    if (!iconUrl) {
      return '';
    }
    return getImage(iconUrl);
  }

  private isSyncing_() {
    return !!this.syncStatus &&
        this.syncStatus.signedInState === SignedInState.SYNCING;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-account-card': SettingsAccountCardElement;
  }
}

customElements.define(
    SettingsAccountCardElement.is, SettingsAccountCardElement);
