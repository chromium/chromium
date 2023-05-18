// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'settings-people-page' is the settings page containing sign-in settings.
 */
import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';
import 'chrome://resources/cr_elements/cr_link_row/cr_link_row.js';
import 'chrome://resources/cr_elements/icons.html.js';
import 'chrome://resources/cr_elements/policy/cr_policy_indicator.js';
import 'chrome://resources/cr_elements/cr_shared_vars.css.js';
import 'chrome://resources/polymer/v3_0/iron-flex-layout/iron-flex-layout-classes.js';
import '/shared/settings/controls/settings_toggle_button.js';
import '../settings_shared.css.js';
import '../os_settings_page/os_settings_animated_pages.js';
import '../os_settings_page/os_settings_subpage.js';
import '../parental_controls_page/parental_controls_page.js';

import {ProfileInfo, ProfileInfoBrowserProxyImpl} from '/shared/settings/people_page/profile_info_browser_proxy.js';
import {SyncBrowserProxy, SyncBrowserProxyImpl, SyncStatus} from '/shared/settings/people_page/sync_browser_proxy.js';
import {convertImageSequenceToPng} from 'chrome://resources/ash/common/cr_picture/png.js';
import {sendWithPromise} from 'chrome://resources/js/cr.js';
import {getImage} from 'chrome://resources/js/icon.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {afterNextRender, flush, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {isAccountManagerEnabled} from '../common/load_time_booleans.js';
import {DeepLinkingMixin} from '../deep_linking_mixin.js';
import {LockStateMixin} from '../lock_state_mixin.js';
import {Setting} from '../mojom-webui/setting.mojom-webui.js';
import {OsPageAvailability} from '../os_page_availability.js';
import {routes} from '../os_settings_routes.js';
import {RouteObserverMixin} from '../route_observer_mixin.js';
import {Route, Router} from '../router.js';

import {Account, AccountManagerBrowserProxyImpl} from './account_manager_browser_proxy.js';
import {getTemplate} from './os_people_page.html.js';

const OsSettingsPeoplePageElementBase =
    LockStateMixin(RouteObserverMixin(DeepLinkingMixin(PolymerElement)));

export class OsSettingsPeoplePageElement extends
    OsSettingsPeoplePageElementBase {
  static get is() {
    return 'os-settings-people-page' as const;
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      prefs: {
        type: Object,
        notify: true,
      },

      /**
       * The current sync status, supplied by SyncBrowserProxy.
       */
      syncStatus: Object,

      /**
       * Dictionary defining page availability.
       */
      pageAvailability: Object,

      authToken_: {
        type: Object,
        observer: 'onAuthTokenChanged_',
      },

      /**
       * The current profile icon URL. Usually a data:image/png URL.
       */
      profileIconUrl_: String,
      profileName_: String,

      profileEmail_: String,

      profileLabel_: String,

      fingerprintUnlockEnabled_: {
        type: Boolean,
        value() {
          return loadTimeData.getBoolean('fingerprintUnlockEnabled');
        },
        readOnly: true,
      },

      isAccountManagerEnabled_: {
        type: Boolean,
        value() {
          return isAccountManagerEnabled();
        },
        readOnly: true,
      },

      showParentalControls_: {
        type: Boolean,
        value() {
          return loadTimeData.valueExists('showParentalControls') &&
              loadTimeData.getBoolean('showParentalControls');
        },
      },

      focusConfig_: {
        type: Object,
        value() {
          const map = new Map();
          if (routes.SYNC) {
            map.set(routes.SYNC.path, '#sync-setup');
          }
          if (routes.LOCK_SCREEN) {
            map.set(routes.LOCK_SCREEN.path, '#lock-screen-subpage-trigger');
          }
          if (routes.ACCOUNTS) {
            map.set(
                routes.ACCOUNTS.path, '#manage-other-people-subpage-trigger');
          }
          if (routes.ACCOUNT_MANAGER) {
            map.set(
                routes.ACCOUNT_MANAGER.path,
                '#account-manager-subpage-trigger');
          }
          return map;
        },
      },

      showPasswordPromptDialog_: {
        type: Boolean,
        value: false,
      },

      /**
       * setModes_ is a partially applied function that stores the current auth
       * token. It's defined only when the user has entered a valid password.
       */
      setModes_: {
        type: Object,
      },

      /**
       * Used by DeepLinkingMixin to focus this page's deep links.
       */
      supportedSettingIds: {
        type: Object,
        value: () => new Set<Setting>([
          Setting.kSetUpParentalControls,

          // Perform Sync page deep links here since it's a shared page.
          Setting.kNonSplitSyncEncryptionOptions,
          Setting.kImproveSearchSuggestions,
          Setting.kMakeSearchesAndBrowsingBetter,
          Setting.kGoogleDriveSearchSuggestions,
        ]),
      },

      /**
       * Whether to show the new UI for OS Sync Settings
       * which include sublabel and Apps toggle
       * shared between Ash and Lacros.
       */
      showSyncSettingsRevamp_: {
        type: Boolean,
        value: loadTimeData.getBoolean('showSyncSettingsRevamp'),
        readOnly: true,
      },

    };
  }

  syncStatus: SyncStatus;
  pageAvailability: OsPageAvailability;
  private authToken_: chrome.quickUnlockPrivate.TokenInfo|undefined;
  private profileIconUrl_: string;
  private profileName_: string;
  private profileEmail_: string;
  private profileLabel_: string;
  private fingerprintUnlockEnabled_: boolean;
  private isAccountManagerEnabled_: boolean;
  private showParentalControls_: boolean;
  private focusConfig_: Map<string, string>;
  private showPasswordPromptDialog_: boolean;
  private showSyncSettingsRevamp_: boolean;
  private setModes_: Object|undefined;
  private syncBrowserProxy_: SyncBrowserProxy;
  private clearAccountPasswordTimeoutId_: number|undefined;


  constructor() {
    super();

    this.syncBrowserProxy_ = SyncBrowserProxyImpl.getInstance();

    /**
     * The timeout ID to pass to clearTimeout() to cancel auth token
     * invalidation.
     */
    this.clearAccountPasswordTimeoutId_ = undefined;
  }

  override connectedCallback(): void {
    super.connectedCallback();

    if (this.isAccountManagerEnabled_) {
      // If we have the Google Account manager, use GAIA name and icon.
      this.addWebUiListener(
          'accounts-changed', this.updateAccounts_.bind(this));
      this.updateAccounts_();
    } else {
      // Otherwise use the Profile name and icon.
      ProfileInfoBrowserProxyImpl.getInstance().getProfileInfo().then(
          this.handleProfileInfo_.bind(this));
      this.addWebUiListener(
          'profile-info-changed', this.handleProfileInfo_.bind(this));
    }

    this.syncBrowserProxy_.getSyncStatus().then(
        this.handleSyncStatus_.bind(this));
    this.addWebUiListener(
        'sync-status-changed', this.handleSyncStatus_.bind(this));
  }

  private onPasswordRequested_(): void {
    this.showPasswordPromptDialog_ = true;
  }

  // Invalidate the token to trigger a password re-prompt. Used for PIN auto
  // submit when too many attempts were made when using PrefStore based PIN.
  private onInvalidateTokenRequested_(): void {
    this.authToken_ = undefined;
  }

  private onPasswordPromptDialogClose_(): void {
    this.showPasswordPromptDialog_ = false;
    if (!this.setModes_) {
      Router.getInstance().navigateToPreviousRoute();
    }
  }

  private getSyncAdvancedTitle_(): string {
    if (this.showSyncSettingsRevamp_) {
      return this.i18n('syncAdvancedDevicePageTitle');
    }
    return this.i18n('syncAdvancedPageTitle');
  }

  private afterRenderShowDeepLink_(
      settingId: Setting,
      getElementCallback: () => (HTMLElement | null)): void {
    // Wait for element to load.
    afterNextRender(this, () => {
      const deepLinkElement = getElementCallback();
      if (!deepLinkElement || deepLinkElement.hidden) {
        console.warn(`Element with deep link id ${settingId} not focusable.`);
        return;
      }
      this.showDeepLinkElement(deepLinkElement);
    });
  }

  override beforeDeepLinkAttempt(settingId: Setting): boolean {
    switch (settingId) {
      // Manually show the deep links for settings nested within elements.
      case Setting.kSetUpParentalControls:
        this.afterRenderShowDeepLink_(settingId, () => {
          const parentalPage =
              this.shadowRoot!.querySelector('settings-parental-controls-page');
          return parentalPage && parentalPage.getSetupButton();
        });
        // Stop deep link attempt since we completed it manually.
        return false;

      // Handle the settings within the old sync page since its a shared
      // component.
      case Setting.kNonSplitSyncEncryptionOptions:
        this.afterRenderShowDeepLink_(settingId, () => {
          const syncPage =
              this.shadowRoot!.querySelector('os-settings-sync-subpage');
          // Expand the encryption collapse.
          syncPage!.forceEncryptionExpanded = true;
          flush();
          return syncPage && syncPage.getEncryptionOptions() &&
              syncPage.getEncryptionOptions()!.getEncryptionsRadioButtons();
        });
        return false;

      case Setting.kImproveSearchSuggestions:
        this.afterRenderShowDeepLink_(settingId, () => {
          const syncPage =
              this.shadowRoot!.querySelector('os-settings-sync-subpage');
          return syncPage && syncPage.getPersonalizationOptions() &&
              syncPage.getPersonalizationOptions()!.getSearchSuggestToggle();
        });
        return false;

      case Setting.kMakeSearchesAndBrowsingBetter:
        this.afterRenderShowDeepLink_(settingId, () => {
          const syncPage =
              this.shadowRoot!.querySelector('os-settings-sync-subpage');
          return syncPage && syncPage.getPersonalizationOptions() &&
              syncPage.getPersonalizationOptions()!.getUrlCollectionToggle();
        });
        return false;

      case Setting.kGoogleDriveSearchSuggestions:
        this.afterRenderShowDeepLink_(settingId, () => {
          const syncPage =
              this.shadowRoot!.querySelector('os-settings-sync-subpage');
          return syncPage && syncPage.getPersonalizationOptions() &&
              syncPage.getPersonalizationOptions()!.getDriveSuggestToggle();
        });
        return false;

      default:
        // Continue with deep linking attempt.
        return true;
    }
  }

  override currentRouteChanged(route: Route): void {
    // The old sync page is a shared subpage, so we handle deep links for
    // both this page and the sync page. Not ideal.
    if (route === routes.SYNC || route === routes.OS_PEOPLE) {
      this.attemptDeepLink();
    }
  }

  private onAuthTokenObtained_(
      e: CustomEvent<chrome.quickUnlockPrivate.TokenInfo>): void {
    this.authToken_ = e.detail;
  }

  private getSyncAndGoogleServicesSubtext_(): string {
    if (this.syncStatus && this.syncStatus.hasError &&
        this.syncStatus.statusText) {
      return this.syncStatus.statusText;
    }
    return '';
  }

  private handleProfileInfo_(info: ProfileInfo): void {
    this.profileName_ = info.name;
    // Extract first frame from image by creating a single frame PNG using
    // url as input if base64 encoded and potentially animated.
    if (info.iconUrl.startsWith('data:image/png;base64')) {
      this.profileIconUrl_ = convertImageSequenceToPng([info.iconUrl]);
      return;
    }
    this.profileIconUrl_ = info.iconUrl;
  }

  /**
   * Handler for when the account list is updated.
   */
  private async updateAccounts_(): Promise<void> {
    const accounts =
        await AccountManagerBrowserProxyImpl.getInstance().getAccounts();
    // The user might not have any GAIA accounts (e.g. guest mode or Active
    // Directory). In these cases the profile row is hidden, so there's nothing
    // to do.
    if (accounts.length === 0) {
      return;
    }
    this.profileName_ = accounts[0].fullName;
    this.profileEmail_ = accounts[0].email;
    this.profileIconUrl_ = accounts[0].pic;

    await this.setProfileLabel(accounts);
  }

  private async setProfileLabel(accounts: Account[]): Promise<void> {
    // Template: "$1 Google accounts" with correct plural of "account".
    const labelTemplate = await sendWithPromise(
        'getPluralString', 'profileLabel', accounts.length);

    // Final output: "X Google accounts"
    this.profileLabel_ = loadTimeData.substituteString(
        labelTemplate, accounts[0].email, accounts.length);
  }

  /**
   * Handler for when the sync state is pushed from the browser.
   */
  private handleSyncStatus_(syncStatus: SyncStatus): void {
    this.syncStatus = syncStatus;

    // When ChromeOSAccountManager is disabled, fall back to using the sync
    // username ("alice@gmail.com") as the profile label.
    if (!this.isAccountManagerEnabled_ && syncStatus && syncStatus.signedIn &&
        syncStatus.signedInUsername) {
      this.profileLabel_ = syncStatus.signedInUsername;
    }
  }

  private onSyncClick_(): void {
    // Users can go to sync subpage regardless of sync status.
    Router.getInstance().navigateTo(routes.SYNC);
  }

  private onAccountManagerClick_(): void {
    if (this.isAccountManagerEnabled_) {
      Router.getInstance().navigateTo(routes.ACCOUNT_MANAGER);
    }
  }

  private getIconImageSet_(iconUrl: string): string {
    return getImage(iconUrl);
  }

  private getProfileName_(): string {
    if (this.isAccountManagerEnabled_) {
      return loadTimeData.getString('osProfileName');
    }
    return this.profileName_;
  }

  private showSignin_(syncStatus: SyncStatus): boolean {
    return loadTimeData.getBoolean('signinAllowed') && !(syncStatus.signedIn);
  }

  private onAuthTokenChanged_(): void {
    if (this.authToken_ === undefined) {
      this.setModes_ = undefined;
    } else {
      const token = this.authToken_.token;
      this.setModes_ =
          (modes: chrome.quickUnlockPrivate.QuickUnlockMode[],
           credentials: string[], onComplete: (result: boolean) => void) => {
            this.quickUnlockPrivate.setModes(token, modes, credentials, () => {
              let result = true;
              if (chrome.runtime.lastError) {
                console.error(
                    'setModes failed: ' + chrome.runtime.lastError.message);
                result = false;
              }
              onComplete(result);
            });
          };
    }

    if (this.clearAccountPasswordTimeoutId_) {
      clearTimeout(this.clearAccountPasswordTimeoutId_);
    }
    if (this.authToken_ === undefined) {
      return;
    }
    // Clear |this.authToken_| after
    // |this.authToken_.tokenInfo.lifetimeSeconds|.
    // Subtract time from the expiration time to account for IPC delays.
    // Treat values less than the minimum as 0 for testing.
    const IPC_SECONDS = 2;
    const lifetimeMs = this.authToken_.lifetimeSeconds > IPC_SECONDS ?
        (this.authToken_.lifetimeSeconds - IPC_SECONDS) * 1000 :
        0;
    this.clearAccountPasswordTimeoutId_ = setTimeout(() => {
      this.authToken_ = undefined;
    }, lifetimeMs);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [OsSettingsPeoplePageElement.is]: OsSettingsPeoplePageElement;
  }
}

customElements.define(
    OsSettingsPeoplePageElement.is, OsSettingsPeoplePageElement);
