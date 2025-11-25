// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'settings-people-page' is the settings page containing sign-in settings.
 */
import 'chrome://resources/cr_elements/cr_icon/cr_icon.js';
import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';
import 'chrome://resources/cr_elements/cr_link_row/cr_link_row.js';
import 'chrome://resources/cr_elements/cr_toast/cr_toast.js';
import 'chrome://resources/cr_elements/icons.html.js';
import 'chrome://resources/cr_elements/policy/cr_policy_indicator.js';
import 'chrome://resources/cr_elements/cr_shared_style.css.js';
import 'chrome://resources/cr_elements/cr_shared_vars.css.js';
import '../controls/settings_toggle_button.js';
// <if expr="not is_chromeos">
import './sync_account_control.js';
// </if>
import '../icons.html.js';
import '../settings_page/settings_section.js';
import '../settings_shared.css.js';

import type {ProfileInfo} from '/shared/settings/people_page/profile_info_browser_proxy.js';
import {ProfileInfoBrowserProxyImpl} from '/shared/settings/people_page/profile_info_browser_proxy.js';
import type {StoredAccount, SyncBrowserProxy, SyncStatus} from '/shared/settings/people_page/sync_browser_proxy.js';
import {ChromeSigninAccessPoint, SignedInState, StatusAction, SyncBrowserProxyImpl} from '/shared/settings/people_page/sync_browser_proxy.js';
// <if expr="is_chromeos">
import {convertImageSequenceToPng} from 'chrome://resources/ash/common/cr_picture/png.js';
// </if>
import type {CrToastElement} from 'chrome://resources/cr_elements/cr_toast/cr_toast.js';
import {WebUiListenerMixin} from 'chrome://resources/cr_elements/web_ui_listener_mixin.js';
import {assert} from 'chrome://resources/js/assert.js';
import {focusWithoutInk} from 'chrome://resources/js/focus_without_ink.js';
import {getImage} from 'chrome://resources/js/icon.js';
import {OpenWindowProxyImpl} from 'chrome://resources/js/open_window_proxy.js';
import {isChromeOS} from 'chrome://resources/js/platform.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {loadTimeData} from '../i18n_setup.js';
import {routes} from '../route.js';
import type {Route} from '../router.js';
import {Router} from '../router.js';
import {SettingsViewMixin} from '../settings_page/settings_view_mixin.js';

// <if expr="not is_chromeos">
import {RouteObserverMixin} from '../router.js';
// </if>

// <if expr="is_chromeos">
import {AccountManagerBrowserProxyImpl} from './account_manager_browser_proxy.js';
// </if>

import {getTemplate} from './people_page.html.js';

export interface SettingsPeoplePageElement {
  $: {
    importDataDialogTrigger: HTMLElement,
    toast: CrToastElement,
  };
}

// <if expr="not is_chromeos">
const SettingsPeoplePageElementBase =
    SettingsViewMixin(RouteObserverMixin(WebUiListenerMixin(PolymerElement)));
// </if>
// <if expr="is_chromeos">
const SettingsPeoplePageElementBase =
    SettingsViewMixin(WebUiListenerMixin(PolymerElement));
// </if>

export class SettingsPeoplePageElement extends SettingsPeoplePageElementBase {
  static get is() {
    return 'settings-people-page';
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
       * TODO(tangltom): In the future when all profiles are completely
       * migrated, this should be removed, and UIs hidden behind it should
       * become default.
       */
      signinAllowed_: {
        type: Boolean,
        value() {
          return loadTimeData.getBoolean('signinAllowed');
        },
      },

      /**
       * This property stores whether the profile is a Dasherless profiles,
       * which is associated with a non-Dasher account. Some UIs related to
       * sign in and sync service will be different because they are not
       * available for these profiles.
       */
      isDasherlessProfile_: {
        type: Boolean,
        value() {
          return loadTimeData.getBoolean('isDasherlessProfile');
        },
      },

      // <if expr="not is_chromeos">
      /**
       * Stored accounts to the system, supplied by SyncBrowserProxy.
       */
      storedAccounts: Object,
      // </if>

      /**
       * The current sync status, supplied by SyncBrowserProxy.
       */
      syncStatus: Object,

      /**
       * Authentication token provided by settings-lock-screen.
       */
      authToken_: {
        type: String,
        value: '',
      },

      /**
       * The currently selected profile icon URL. May be a data URL.
       */
      profileIconUrl_: String,

      /**
       * Whether the profile row is clickable. The behavior depends on the
       * platform.
       */
      isProfileActionable_: {
        type: Boolean,
        value() {
          if (!isChromeOS) {
            // Opens profile manager.
            return true;
          }
          // Post-SplitSettings links out to account manager if it is available.
          return loadTimeData.getBoolean('isAccountManagerEnabled');
        },
        readOnly: true,
      },

      /**
       * The current profile name.
       */
      profileName_: String,

      // <if expr="not is_chromeos">
      shouldShowGoogleAccount_: {
        type: Boolean,
        value: false,
        computed:
            'computeShouldShowGoogleAccount_(storedAccounts, syncStatus,' +
            'storedAccounts.length, syncStatus.signedIn, syncStatus.hasError)',
      },

      replaceSyncPromosWithSignInPromos_: {
        type: Boolean,
        value: () =>
            loadTimeData.getBoolean('replaceSyncPromosWithSignInPromos'),
      },

      showImportDataDialog_: {
        type: Boolean,
        value: false,
      },

      showSignoutDialog_: Boolean,
      primaryAccountName_: String,
      primaryAccountEmail_: String,
      primaryAccountIconUrl_: String,
      // </if>

      // Exposes ChromeSigninAccessPoint enum to HTML bindings.
      accessPointEnum_: {
        type: Object,
        value: ChromeSigninAccessPoint,
      },
    };
  }

  declare prefs: any;
  declare private signinAllowed_: boolean;
  declare private isDasherlessProfile_: boolean;
  declare syncStatus: SyncStatus|null;
  declare private authToken_: string;
  declare private profileIconUrl_: string;
  declare private isProfileActionable_: boolean;
  declare private profileName_: string;

  // <if expr="not is_chromeos">
  declare storedAccounts: StoredAccount[]|null;
  declare private shouldShowGoogleAccount_: boolean;
  declare private replaceSyncPromosWithSignInPromos_: boolean;
  declare private showImportDataDialog_: boolean;
  declare private showSignoutDialog_: boolean;
  declare private primaryAccountName_: string;
  declare private primaryAccountEmail_: string;
  declare private primaryAccountIconUrl_: string;
  // </if>

  private syncBrowserProxy_: SyncBrowserProxy =
      SyncBrowserProxyImpl.getInstance();

  override connectedCallback() {
    super.connectedCallback();

    let useProfileNameAndIcon = true;
    // <if expr="is_chromeos">
    if (loadTimeData.getBoolean('isAccountManagerEnabled')) {
      // If this is SplitSettings and we have the Google Account manager,
      // prefer the GAIA name and icon.
      useProfileNameAndIcon = false;
      this.addWebUiListener(
          'accounts-changed', this.updateAccounts_.bind(this));
      this.updateAccounts_();
    }
    // </if>
    if (useProfileNameAndIcon) {
      ProfileInfoBrowserProxyImpl.getInstance().getProfileInfo().then(
          this.handleProfileInfo_.bind(this));
      this.addWebUiListener(
          'profile-info-changed', this.handleProfileInfo_.bind(this));
    }

    this.syncBrowserProxy_.getSyncStatus().then(
        this.handleSyncStatus_.bind(this));
    this.addWebUiListener(
        'sync-status-changed', this.handleSyncStatus_.bind(this));

    // <if expr="not is_chromeos">
    this.syncBrowserProxy_.getStoredAccounts().then(
        this.handleStoredAccounts_.bind(this));
    this.addWebUiListener(
        'stored-accounts-updated', this.handleStoredAccounts_.bind(this));

    this.addWebUiListener('sync-settings-saved', () => {
      this.$.toast.show();
    });
    // </if>
  }

  // <if expr="not is_chromeos">
  override currentRouteChanged(newRoute: Route, oldRoute?: Route) {
    super.currentRouteChanged(newRoute, oldRoute);

    this.showImportDataDialog_ =
        Router.getInstance().getCurrentRoute() === routes.IMPORT_DATA;

    if (Router.getInstance().getCurrentRoute() === routes.SIGN_OUT) {
      // If the sync status has not been fetched yet, optimistically display
      // the sign-out dialog. There is another check when the sync status is
      // fetched. The dialog will be closed when the user is not signed in.
      if (this.syncStatus && !this.isSyncing_()) {
        Router.getInstance().navigateToPreviousRoute();
      } else {
        this.showSignoutDialog_ = true;
      }
    }
  }
  // </if>

  /**
   * Handler for when the profile's icon and name is updated.
   */
  private handleProfileInfo_(info: ProfileInfo) {
    this.profileName_ = info.name;
    /**
     * Extract first frame from image by creating a single frame PNG using
     * url as input if base64 encoded and potentially animated.
     */
    // <if expr="is_chromeos">
    if (info.iconUrl.startsWith('data:image/png;base64')) {
      this.profileIconUrl_ = convertImageSequenceToPng([info.iconUrl]);
      return;
    }
    // </if>

    this.profileIconUrl_ = info.iconUrl;
  }

  // <if expr="is_chromeos">
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
  private computeShouldShowGoogleAccount_(): boolean {
    if (this.replaceSyncPromosWithSignInPromos_) {
      return false;
    }

    if (this.storedAccounts === undefined || this.syncStatus === undefined) {
      return false;
    }

    return (this.storedAccounts!.length > 0 || this.isSyncing_()) &&
        !this.syncStatus!.hasError;
  }
  // </if>

  private onProfileClick_() {
    // <if expr="is_chromeos">
    if (loadTimeData.getBoolean('isAccountManagerEnabled')) {
      // Post-SplitSettings. The browser C++ code loads OS settings in a window.
      OpenWindowProxyImpl.getInstance().openUrl(
          loadTimeData.getString('osSettingsAccountsPageUrl'));
    }
    // </if>
    // <if expr="not is_chromeos">
    Router.getInstance().navigateTo(routes.MANAGE_PROFILE);
    // </if>
  }

  // <if expr="not is_chromeos">
  private onDisconnectDialogClosed_() {
    this.showSignoutDialog_ = false;

    if (Router.getInstance().getCurrentRoute() === routes.SIGN_OUT) {
      Router.getInstance().navigateToPreviousRoute();
    }
  }
  // </if>

  private onSyncClick_() {
    // Users can go to sync subpage regardless of sync status.
    Router.getInstance().navigateTo(routes.SYNC);
  }

  // <if expr="not is_chromeos">
  private onAccountClick_() {
    Router.getInstance().navigateTo(routes.ACCOUNT);
  }

  private onGoogleServicesClick_() {
    Router.getInstance().navigateTo(routes.GOOGLE_SERVICES);
  }

  private onImportDataClick_() {
    Router.getInstance().navigateTo(routes.IMPORT_DATA);
  }

  private onImportDataDialogClosed_() {
    Router.getInstance().navigateToPreviousRoute();
    focusWithoutInk(this.$.importDataDialogTrigger);
  }

  private shouldLinkToAccountSettingsPage_(): boolean {
    return this.replaceSyncPromosWithSignInPromos_ && !!this.syncStatus &&
        this.syncStatus.signedInState === SignedInState.SIGNED_IN;
  }

  private shouldLinkToProfileRow_(): boolean {
    return !this.shouldShowSyncAccountControl_() &&
        !this.shouldLinkToAccountSettingsPage_();
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
  // </if>

  /**
   * Open URL for managing your Google Account.
   */
  private openGoogleAccount_() {
    OpenWindowProxyImpl.getInstance().openUrl(
        loadTimeData.getString('googleAccountUrl'));
    chrome.metricsPrivate.recordUserAction('ManageGoogleAccount_Clicked');
  }

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

  // <if expr="not is_chromeos">
  private shouldHideSyncSetupLinkRow_() {
    return this.replaceSyncPromosWithSignInPromos_ &&
        (!this.syncStatus ||
         this.syncStatus.signedInState !== SignedInState.SYNCING);
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

  // SettingsViewMixin implementation.
  override getFocusConfig() {
    const map = new Map();
    if (routes.SYNC) {
      map.set(routes.SYNC.path, '#sync-setup');
    }
    // <if expr="not is_chromeos">
    if (routes.MANAGE_PROFILE) {
      map.set(
          routes.MANAGE_PROFILE.path,
          loadTimeData.getBoolean('signinAllowed') ?
              '#edit-profile' :
              '#profile-row .subpage-arrow');
    }
    if (routes.ACCOUNT) {
      map.set(routes.ACCOUNT.path, '#account-subpage-row');
    }
    if (routes.GOOGLE_SERVICES) {
      map.set(routes.GOOGLE_SERVICES.path, '#google-services');
    }
    // </if>
    return map;
  }

  // SettingsViewMixin implementation.
  override getAssociatedControlFor(childViewId: string): HTMLElement {
    const ids = [
      'sync', 'syncControls',
      // <if expr="not is_chromeos">
      'manageProfile', 'account', 'googleServices',
      // </if>
    ];
    assert(ids.includes(childViewId));

    let triggerId: string|null = null;
    switch (childViewId) {
      case 'sync':
      case 'syncControls':
        triggerId = 'sync-setup';
        break;
      // <if expr="not is_chromeos">
      case 'manageProfile':
        triggerId = this.signinAllowed_ ? 'edit-profile' : 'profile-row';
        break;
      case 'account':
        assert(loadTimeData.getBoolean('replaceSyncPromosWithSignInPromos'));
        triggerId = 'account-subpage-row';
        break;
      case 'googleServices':
        assert(loadTimeData.getBoolean('replaceSyncPromosWithSignInPromos'));
        triggerId = 'google-services';
        break;
        // </if>
    }

    assert(triggerId);

    const control =
        this.shadowRoot!.querySelector<HTMLElement>(`#${triggerId}`);
    assert(control);
    return control;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-people-page': SettingsPeoplePageElement;
  }
}

customElements.define(SettingsPeoplePageElement.is, SettingsPeoplePageElement);
