// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'settings-people-page' is the settings page containing sign-in settings.
 */
import 'chrome://resources/ash/common/cr_elements/cr_icon_button/cr_icon_button.js';
import 'chrome://resources/ash/common/cr_elements/cr_link_row/cr_link_row.js';
import 'chrome://resources/ash/common/cr_elements/icons.html.js';
import 'chrome://resources/ash/common/cr_elements/policy/cr_policy_indicator.js';
import 'chrome://resources/ash/common/cr_elements/cr_shared_vars.css.js';
import 'chrome://resources/polymer/v3_0/iron-flex-layout/iron-flex-layout-classes.js';
import '../controls/settings_toggle_button.js';
import '../settings_shared.css.js';
import '../os_settings_page/os_settings_animated_pages.js';
import '../os_settings_page/os_settings_subpage.js';
import '../os_settings_page/settings_card.js';
import '../parental_controls_page/parental_controls_page.js';
import '../parental_controls_page/parental_controls_settings_card.js';
import './account_manager_settings_card.js';
import './additional_accounts_settings_card.js';
import './graduation/graduation_settings_card.js';

import {ProfileInfo, ProfileInfoBrowserProxyImpl} from '/shared/settings/people_page/profile_info_browser_proxy.js';
import {SignedInState, SyncBrowserProxy, SyncBrowserProxyImpl, SyncStatus} from '/shared/settings/people_page/sync_browser_proxy.js';
import {convertImageSequenceToPng} from 'chrome://resources/ash/common/cr_picture/png.js';
import {sendWithPromise} from 'chrome://resources/js/cr.js';
import {getImage} from 'chrome://resources/js/icon.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {afterNextRender, flush, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {DeepLinkingMixin} from '../common/deep_linking_mixin.js';
import {isAccountManagerEnabled, isRevampWayfindingEnabled} from '../common/load_time_booleans.js';
import {RouteOriginMixin} from '../common/route_origin_mixin.js';
import {LockStateMixin} from '../lock_state_mixin.js';
import {Section} from '../mojom-webui/routes.mojom-webui.js';
import {Setting} from '../mojom-webui/setting.mojom-webui.js';
import {Route, Router, routes} from '../router.js';

import {Account, AccountManagerBrowserProxyImpl} from './account_manager_browser_proxy.js';
import {getTemplate} from './os_people_page.html.js';

const OsSettingsPeoplePageElementBase =
    LockStateMixin(RouteOriginMixin(DeepLinkingMixin(PolymerElement)));

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

      section_: {
        type: Number,
        value: Section.kPeople,
        readOnly: true,
      },

      /**
       * The current sync status, supplied by SyncBrowserProxy.
       */
      syncStatus: Object,

      accounts_: {
        type: Array,
        value() {
          return [];
        },
      },

      deviceAccount_: {
        type: Object,
        value() {
          return null;
        },
      },

      authTokenInfo_: {
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

      isRevampWayfindingEnabled_: {
        type: Boolean,
        value: () => {
          return isRevampWayfindingEnabled();
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

      showPasswordPromptDialog_: {
        type: Boolean,
        value: false,
      },

      showGraduationApp_: {
        type: Boolean,
        value: () => {
          return loadTimeData.getBoolean('isGraduationFlagEnabled') &&
              loadTimeData.getBoolean('isGraduationAppEnabled');
        },
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
  private accounts_: Account[];
  private deviceAccount_: Account|null;
  private authTokenInfo_: chrome.quickUnlockPrivate.TokenInfo|undefined;
  private profileIconUrl_: string;
  private profileName_: string;
  private profileEmail_: string;
  private profileLabel_: string;
  private fingerprintUnlockEnabled_: boolean;
  private isAccountManagerEnabled_: boolean;
  private readonly isRevampWayfindingEnabled_: boolean;
  private showParentalControls_: boolean;
  private section_: Section;
  private showPasswordPromptDialog_: boolean;
  private showSyncSettingsRevamp_: boolean;
  private syncBrowserProxy_: SyncBrowserProxy;
  private clearAccountPasswordTimeoutId_: number|undefined;


  constructor() {
    super();

    /** RouteOriginMixin override */
    this.route = routes.OS_PEOPLE;

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

  override ready(): void {
    super.ready();

    this.addFocusConfig(routes.SYNC, '#syncSetupRow');
    this.addFocusConfig(
        routes.ACCOUNT_MANAGER, '#accountManagerSubpageTrigger');
  }

  private onPasswordRequested_(): void {
    this.showPasswordPromptDialog_ = true;
  }

  // Invalidate the token to trigger a password re-prompt. Used for PIN auto
  // submit when too many attempts were made when using PrefStore based PIN.
  private onInvalidateTokenRequested_(): void {
    this.authTokenInfo_ = undefined;
  }

  private onPasswordPromptDialogClose_(): void {
    this.showPasswordPromptDialog_ = false;
    if (!this.authTokenInfo_) {
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

  // TODO(b/302374851) The manual deep linking below can be removed once the
  // Revamp feature is fully launched.
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

  override currentRouteChanged(newRoute: Route, oldRoute?: Route): void {
    super.currentRouteChanged(newRoute, oldRoute);

    // The old sync page is a shared subpage, so we handle deep links for
    // both this page and the sync page. Not ideal.
    if (newRoute === routes.SYNC || newRoute === this.route) {
      this.attemptDeepLink();
    }
  }

  private onAuthTokenObtained_(
      e: CustomEvent<chrome.quickUnlockPrivate.TokenInfo>): void {
    this.authTokenInfo_ = e.detail;
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
    this.accounts_ = accounts;

    // The user might not have any GAIA accounts (e.g. guest mode or Active
    // Directory). In these cases the profile row is hidden, so there's nothing
    // to do.
    if (accounts.length === 0) {
      return;
    }

    // Device account is always first per account_manager_ui_handler.cc.
    // TODO(b/325142618) Investigate why `isDeviceAccount` is not always true.
    this.deviceAccount_ = accounts[0];
    this.profileName_ = this.deviceAccount_.fullName;
    this.profileEmail_ = this.deviceAccount_.email;
    this.profileIconUrl_ = this.deviceAccount_.pic;

    // Template: "$1 Google accounts" with correct plural of "account".
    const labelTemplate = await sendWithPromise(
        'getPluralString', 'profileLabel', this.accounts_.length);
    // Final output: "X Google accounts"
    this.profileLabel_ = loadTimeData.substituteString(
        labelTemplate, this.profileEmail_, this.accounts_.length);
  }

  /**
   * Handler for when the sync state is pushed from the browser.
   */
  private handleSyncStatus_(syncStatus: SyncStatus): void {
    this.syncStatus = syncStatus;

    // When ChromeOSAccountManager is disabled, fall back to using the sync
    // username ("alice@gmail.com") as the profile label.
    if (!this.isAccountManagerEnabled_ && syncStatus &&
        syncStatus.signedInState === SignedInState.SYNCING &&
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

  private onAuthTokenChanged_(): void {
    if (this.clearAccountPasswordTimeoutId_) {
      clearTimeout(this.clearAccountPasswordTimeoutId_);
    }
    if (this.authTokenInfo_ === undefined) {
      return;
    }
    // Clear |this.authTokenInfo_| after
    // |this.authTokenInfo_.tokenInfo.lifetimeSeconds|.
    // Subtract time from the expiration time to account for IPC delays.
    // Treat values less than the minimum as 0 for testing.
    const IPC_SECONDS = 2;
    const lifetimeMs = this.authTokenInfo_.lifetimeSeconds > IPC_SECONDS ?
        (this.authTokenInfo_.lifetimeSeconds - IPC_SECONDS) * 1000 :
        0;
    this.clearAccountPasswordTimeoutId_ = setTimeout(() => {
      this.authTokenInfo_ = undefined;
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
