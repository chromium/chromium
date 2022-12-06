// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'settings-people-page' is the settings page containing sign-in settings.
 */
import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';
import 'chrome://resources/cr_elements/cr_link_row/cr_link_row.js';
import 'chrome://resources/cr_elements/cr_toast/cr_toast.js';
import 'chrome://resources/cr_elements/icons.html.js';
import 'chrome://resources/cr_elements/policy/cr_policy_indicator.js';
import 'chrome://resources/cr_elements/cr_shared_style.css.js';
import 'chrome://resources/cr_elements/cr_shared_vars.css.js';
import 'chrome://resources/polymer/v3_0/iron-flex-layout/iron-flex-layout-classes.js';
import '../controls/settings_toggle_button.js';
import './sync_account_control.js';
import '../icons.html.js';
import '../settings_page/settings_animated_pages.js';
import '../settings_page/settings_subpage.js';
import '../settings_shared.css.js';

// <if expr="chromeos_ash">
import {convertImageSequenceToPng} from 'chrome://resources/ash/common/cr_picture/png.js';
// </if>
import {CrToastElement} from 'chrome://resources/cr_elements/cr_toast/cr_toast.js';
import {isChromeOS} from 'chrome://resources/js/platform.js';
import {focusWithoutInk} from 'chrome://resources/js/focus_without_ink.js';
import {getImage} from 'chrome://resources/js/icon.js';
import {WebUiListenerMixin, WebUiListenerMixinInterface} from 'chrome://resources/cr_elements/web_ui_listener_mixin.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {BaseMixin} from '../base_mixin.js';
import {FocusConfig} from '../focus_config.js';
import {loadTimeData} from '../i18n_setup.js';
import {OpenWindowProxyImpl} from '../open_window_proxy.js';
import {PageVisibility} from '../page_visibility.js';
import {routes} from '../route.js';
import {RouteObserverMixin, RouteObserverMixinInterface, Router} from '../router.js';

// <if expr="chromeos_ash">
import {AccountManagerBrowserProxyImpl} from './account_manager_browser_proxy.js';
// </if>

import {getTemplate} from './people_page.html.js';
import {ProfileInfo, ProfileInfoBrowserProxyImpl} from './profile_info_browser_proxy.js';
import {StoredAccount, SyncBrowserProxy, SyncBrowserProxyImpl, SyncStatus} from './sync_browser_proxy.js';

export interface SettingsPeoplePageElement {
  $: {
    importDataDialogTrigger: HTMLElement,
    toast: CrToastElement,
  };
}

const SettingsPeoplePageElementBase =
    RouteObserverMixin(WebUiListenerMixin(BaseMixin(PolymerElement))) as {
      new (): PolymerElement & WebUiListenerMixinInterface &
          RouteObserverMixinInterface,
    };

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

      // <if expr="not chromeos_ash">
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
       * Dictionary defining page visibility.
       */
      pageVisibility: Object,

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

      // <if expr="not chromeos_ash">
      shouldShowGoogleAccount_: {
        type: Boolean,
        value: false,
        computed:
            'computeShouldShowGoogleAccount_(storedAccounts, syncStatus,' +
            'storedAccounts.length, syncStatus.signedIn, syncStatus.hasError)',
      },

      showImportDataDialog_: {
        type: Boolean,
        value: false,
      },
      // </if>

      showSignoutDialog_: Boolean,

      focusConfig_: {
        type: Object,
        value() {
          const map = new Map();
          if (routes.SYNC) {
            map.set(routes.SYNC.path, '#sync-setup');
          }
          // <if expr="not chromeos_ash">
          if (routes.MANAGE_PROFILE) {
            map.set(
                routes.MANAGE_PROFILE.path,
                loadTimeData.getBoolean('signinAllowed') ?
                    '#edit-profile' :
                    '#profile-row .subpage-arrow');
          }
          // </if>
          return map;
        },
      },
    };
  }

  prefs: any;
  private signinAllowed_: boolean;
  syncStatus: SyncStatus|null;
  pageVisibility: PageVisibility;
  private authToken_: string;
  private profileIconUrl_: string;
  private isProfileActionable_: boolean;
  private profileName_: string;

  // <if expr="not chromeos_ash">
  storedAccounts: StoredAccount[]|null;
  private shouldShowGoogleAccount_: boolean;
  private showImportDataDialog_: boolean;
  // </if>

  private showSignoutDialog_: boolean;
  private focusConfig_: FocusConfig;

  private syncBrowserProxy_: SyncBrowserProxy =
      SyncBrowserProxyImpl.getInstance();

  override connectedCallback() {
    super.connectedCallback();

    let useProfileNameAndIcon = true;
    // <if expr="chromeos_ash">
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

    // <if expr="not chromeos_ash">
    const handleStoredAccounts = (accounts: StoredAccount[]) => {
      this.storedAccounts = accounts;
    };
    this.syncBrowserProxy_.getStoredAccounts().then(handleStoredAccounts);
    this.addWebUiListener('stored-accounts-updated', handleStoredAccounts);

    this.addWebUiListener('sync-settings-saved', () => {
      this.$.toast.show();
    });
    // </if>
  }

  override currentRouteChanged() {
    // <if expr="not chromeos_ash">
    this.showImportDataDialog_ =
        Router.getInstance().getCurrentRoute() === routes.IMPORT_DATA;
    // </if>

    if (Router.getInstance().getCurrentRoute() === routes.SIGN_OUT) {
      // If the sync status has not been fetched yet, optimistically display
      // the sign-out dialog. There is another check when the sync status is
      // fetched. The dialog will be closed when the user is not signed in.
      if (this.syncStatus && !this.syncStatus.signedIn) {
        Router.getInstance().navigateToPreviousRoute();
      } else {
        this.showSignoutDialog_ = true;
      }
    }
  }

  private getEditPersonAssocControl_(): Element {
    return this.signinAllowed_ ?
        this.shadowRoot!.querySelector('#edit-profile')! :
        this.shadowRoot!.querySelector('#profile-row')!;
  }

  private getSyncAndGoogleServicesSubtext_(): string {
    if (this.syncStatus && this.syncStatus.hasError &&
        this.syncStatus.statusText) {
      return this.syncStatus.statusText;
    }
    return '';
  }

  /**
   * Handler for when the profile's icon and name is updated.
   */
  private handleProfileInfo_(info: ProfileInfo) {
    this.profileName_ = info.name;
    /**
     * Extract first frame from image by creating a single frame PNG using
     * url as input if base64 encoded and potentially animated.
     */
    // <if expr="chromeos_ash">
    if (info.iconUrl.startsWith('data:image/png;base64')) {
      this.profileIconUrl_ = convertImageSequenceToPng([info.iconUrl]);
      return;
    }
    // </if>

    this.profileIconUrl_ = info.iconUrl;
  }

  // <if expr="chromeos_ash">
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
  private handleSyncStatus_(syncStatus: SyncStatus|null) {
    // Sign-in impressions should be recorded only if the sign-in promo is
    // shown. They should be recorder only once, the first time
    // |this.syncStatus| is set.
    const shouldRecordSigninImpression = !this.syncStatus && syncStatus &&
        this.signinAllowed_ && !syncStatus.signedIn;

    this.syncStatus = syncStatus;

    if (shouldRecordSigninImpression && !this.shouldShowSyncAccountControl_()) {
      // SyncAccountControl records the impressions user actions.
      chrome.metricsPrivate.recordUserAction('Signin_Impression_FromSettings');
    }
  }

  // <if expr="not chromeos_ash">
  private computeShouldShowGoogleAccount_(): boolean {
    if (this.storedAccounts === undefined || this.syncStatus === undefined) {
      return false;
    }

    return (this.storedAccounts!.length > 0 || !!this.syncStatus!.signedIn) &&
        !this.syncStatus!.hasError;
  }
  // </if>

  private onProfileTap_() {
    // <if expr="chromeos_ash">
    if (loadTimeData.getBoolean('isAccountManagerEnabled')) {
      // Post-SplitSettings. The browser C++ code loads OS settings in a window.
      // Don't use window.open() because that creates an extra empty tab.
      window.location.href = 'chrome://os-settings/accountManager';
    }
    // </if>
    // <if expr="not chromeos_ash">
    Router.getInstance().navigateTo(routes.MANAGE_PROFILE);
    // </if>
  }

  private onDisconnectDialogClosed_() {
    this.showSignoutDialog_ = false;

    if (Router.getInstance().getCurrentRoute() === routes.SIGN_OUT) {
      Router.getInstance().navigateToPreviousRoute();
    }
  }

  private onSyncTap_() {
    // Users can go to sync subpage regardless of sync status.
    Router.getInstance().navigateTo(routes.SYNC);
  }

  // <if expr="not is_chromeos">
  private onImportDataTap_() {
    Router.getInstance().navigateTo(routes.IMPORT_DATA);
  }

  private onImportDataDialogClosed_() {
    Router.getInstance().navigateToPreviousRoute();
    focusWithoutInk(this.$.importDataDialogTrigger);
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

  private shouldShowSyncAccountControl_(): boolean {
    // <if expr="chromeos_ash">
    return false;
    // </if>
    // <if expr="not chromeos_ash">
    if (this.syncStatus === undefined) {
      return false;
    }
    return !!this.syncStatus!.syncSystemEnabled && this.signinAllowed_;
    // </if>
  }

  /**
   * @return A CSS image-set for multiple scale factors.
   */
  private getIconImageSet_(iconUrl: string): string {
    return getImage(iconUrl);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-people-page': SettingsPeoplePageElement;
  }
}

customElements.define(SettingsPeoplePageElement.is, SettingsPeoplePageElement);
