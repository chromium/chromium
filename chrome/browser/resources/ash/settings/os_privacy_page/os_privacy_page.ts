// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'os-settings-privacy-page' is the settings page containing privacy and
 * security settings.
 */

import 'chrome://resources/ash/common/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/ash/common/cr_elements/cr_dialog/cr_dialog.js';
import 'chrome://resources/ash/common/cr_elements/cr_link_row/cr_link_row.js';
import 'chrome://resources/polymer/v3_0/paper-spinner/paper-spinner-lite.js';
import '../controls/settings_toggle_button.js';
import '../settings_shared.css.js';
import '../os_settings_page/os_settings_animated_pages.js';
import '../os_settings_page/os_settings_subpage.js';
import '../os_settings_page/settings_card.js';
import './metrics_consent_toggle_button.js';
import './peripheral_data_access_protection_dialog.js';
import '../os_people_page/lock_screen_password_prompt_dialog.js';
import '../os_people_page/os_sync_browser_proxy.js';

import {SignedInState, SyncBrowserProxy, SyncBrowserProxyImpl, SyncStatus} from '/shared/settings/people_page/sync_browser_proxy.js';
import {PrefsMixin} from '/shared/settings/prefs/prefs_mixin.js';
import {AUTH_TOKEN_INVALID_EVENT_TYPE} from 'chrome://resources/ash/common/quick_unlock/utils.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {InSessionAuth, Reason, RequestTokenReply} from 'chrome://resources/mojo/chromeos/components/in_session_auth/mojom/in_session_auth.mojom-webui.js';
import {afterNextRender, flush, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {DeepLinkingMixin} from '../common/deep_linking_mixin.js';
import {isAccountManagerEnabled, isRevampWayfindingEnabled} from '../common/load_time_booleans.js';
import {RouteOriginMixin} from '../common/route_origin_mixin.js';
import {SettingsToggleButtonElement} from '../controls/settings_toggle_button.js';
import {LockStateMixin} from '../lock_state_mixin.js';
import {recordSettingChange} from '../metrics_recorder.js';
import {Section} from '../mojom-webui/routes.mojom-webui.js';
import {Setting} from '../mojom-webui/setting.mojom-webui.js';
import {Route, Router, routes} from '../router.js';

import {getTemplate} from './os_privacy_page.html.js';
import {PeripheralDataAccessBrowserProxy, PeripheralDataAccessBrowserProxyImpl} from './peripheral_data_access_browser_proxy.js';
import {PrivacyHubNavigationOrigin} from './privacy_hub_subpage.js';

export interface OsSettingsPrivacyPageElement {
  $: {
    verifiedAccessToggle: SettingsToggleButtonElement,
  };
}

const OsSettingsPrivacyPageElementBase = PrefsMixin(
    LockStateMixin(RouteOriginMixin(DeepLinkingMixin(PolymerElement))));

export class OsSettingsPrivacyPageElement extends
    OsSettingsPrivacyPageElementBase {
  static get is() {
    return 'os-settings-privacy-page' as const;
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      section_: {
        type: Number,
        value: Section.kPrivacyAndSecurity,
        readOnly: true,
      },

      /**
       * Authentication token.
       * This is only used if `isAuthPanelInSessionEnabled_` is set to false.
       */
      authTokenInfo_: {
        type: Object,
        observer: 'onAuthTokenChanged_',
      },

      /**
       * The variable that stores the authentication token we receive
       * from AuthPanel or ActiveSessionAuth.
       * This is only used if `isAuthPanelInSessionEnabled_`
       */
      authTokenReply_: {
        type: Object,
      },

      showPasswordPromptDialog_: {
        type: Boolean,
        value: false,
      },

      /**
       * The current sync status, supplied by SyncBrowserProxy.
       */
      syncStatus: Object,

      /**
       * Used by DeepLinkingMixin to focus this page's deep links.
       */
      supportedSettingIds: {
        type: Object,
        value: () => new Set<Setting>([
          Setting.kVerifiedAccess,
        ]),
      },

      /**
       * True if fingerprint settings should be displayed on this machine.
       */
      fingerprintUnlockEnabled_: {
        type: Boolean,
        value() {
          return loadTimeData.getBoolean('fingerprintUnlockEnabled');
        },
        readOnly: true,
      },

      /**
       * True if auth panel will be used for authentication instead of
       * password prompt dialog.
       */
      isAuthPanelInSessionEnabled_: {
        type: Boolean,
        value() {
          return loadTimeData.getBoolean('isAuthPanelEnabled');
        },
        readOnly: true,
      },

      /**
       * True if snooping protection or screen lock is enabled.
       */
      isSmartPrivacyEnabled_: {
        type: Boolean,
        value() {
          return loadTimeData.getBoolean('isSnoopingProtectionEnabled') ||
              loadTimeData.getBoolean('isQuickDimEnabled');
        },
        readOnly: true,
      },

      /**
       * True if OS is running on reven board.
       */
      isRevenBranding_: {
        type: Boolean,
        value() {
          return loadTimeData.getBoolean('isRevenBranding');
        },
        readOnly: true,
      },

      /**
       * Whether the user is in guest mode.
       */
      isGuestMode_: {
        type: Boolean,
        value() {
          return loadTimeData.getBoolean('isGuest');
        },
        readOnly: true,
      },

      showDisableProtectionDialog_: {
        type: Boolean,
        value: false,
      },

      isThunderboltSupported_: {
        type: Boolean,
        value: false,
      },

      dataAccessProtectionPrefName_: {
        type: String,
        value: '',
      },

      isUserConfigurable_: {
        type: Boolean,
        value: false,
        reflectToAttribute: true,
      },

      dataAccessShiftTabPressed_: {
        type: Boolean,
        value: false,
      },

      profileLabel_: String,

      /**
       * Whether the secure DNS setting should be displayed.
       */
      showSecureDnsSetting_: {
        type: Boolean,
        value: function() {
          return loadTimeData.getBoolean('showSecureDnsSetting');
        },
        readOnly: true,
      },

      isHatsSurveyEnabled_: {
        type: Boolean,
        value: function() {
          return loadTimeData.getBoolean('isPrivacyHubHatsEnabled');
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

      rowIcons_: {
        type: Object,
        value() {
          if (isRevampWayfindingEnabled()) {
            return {
              privacyHub: 'os-settings:privacy-controls',
              sync: 'os-settings:sync-revamp',
              lockScreen: 'os-settings:lock-revamp',
              manageOtherPeople: 'os-settings:privacy-manage-people',
              smartPrivacy: 'os-settings:privacy-smart-privacy',
              suggestedContent: 'os-settings:content-recommend',
              verifiedAccess: 'os-settings:privacy-verified-access',
              dataAccessProtection:
                  'os-settings:privacy-data-access-protection',
            };
          }

          return {
            privacyHub: '',
            sync: '',
            lockScreen: '',
            manageOtherPeople: '',
            smartPrivacy: '',
            suggestedContent: '',
            verifiedAccess: '',
            dataAccessProtection: '',
          };
        },
      },

      isAuthenticating_: {
        type: Boolean,
        value: false,
      },
    };
  }

  static get observers() {
    return ['onDataAccessFlagsSet_(isThunderboltSupported_.*)'];
  }

  syncStatus: SyncStatus;
  private authTokenInfo_: chrome.quickUnlockPrivate.TokenInfo|undefined;
  private browserProxy_: PeripheralDataAccessBrowserProxy;
  private rowIcons_: Record<string, string>;
  private authTokenReply_: RequestTokenReply|undefined|null;

  /**
   * The timeout ID to pass to clearTimeout() to cancel auth token
   * invalidation.
   */
  private clearAccountPasswordTimeoutId_: number|undefined = undefined;
  private dataAccessProtectionPrefName_: string;
  private dataAccessShiftTabPressed_: boolean;
  private fingerprintUnlockEnabled_: boolean;
  private isAccountManagerEnabled_: boolean;
  private isAuthPanelInSessionEnabled_: boolean;
  private isGuestMode_: boolean;
  private isRevampWayfindingEnabled_: boolean;
  private isRevenBranding_: boolean;
  private isSmartPrivacyEnabled_: boolean;
  private isThunderboltSupported_: boolean;
  private isUserConfigurable_: boolean;
  private profileLabel_: string;
  private section_: Section;
  private showDisableProtectionDialog_: boolean;
  private showPasswordPromptDialog_: boolean;
  private showSecureDnsSetting_: boolean;
  private showSyncSettingsRevamp_: boolean;
  private syncBrowserProxy_: SyncBrowserProxy;
  private isAuthenticating_: boolean;

  constructor() {
    super();

    /** RouteOriginMixin override */
    this.route = routes.OS_PRIVACY;

    this.browserProxy_ = PeripheralDataAccessBrowserProxyImpl.getInstance();
    this.syncBrowserProxy_ = SyncBrowserProxyImpl.getInstance();

    if (isRevampWayfindingEnabled()) {
      // When revamp wayfinding is enabled, Sync settings is moved to the
      // privacy page, hence add the Sync deep links here.
      this.supportedSettingIds.add(Setting.kNonSplitSyncEncryptionOptions);
      this.supportedSettingIds.add(Setting.kImproveSearchSuggestions);
      this.supportedSettingIds.add(Setting.kMakeSearchesAndBrowsingBetter);
      this.supportedSettingIds.add(Setting.kGoogleDriveSearchSuggestions);
    }

    this.browserProxy_.isThunderboltSupported().then(enabled => {
      this.isThunderboltSupported_ = enabled;
      if (this.isThunderboltSupported_) {
        this.supportedSettingIds.add(Setting.kPeripheralDataAccessProtection);
      }
    });
  }

  override connectedCallback(): void {
    super.connectedCallback();

    if (this.isRevampWayfindingEnabled_) {
      this.syncBrowserProxy_.getSyncStatus().then(
          this.handleSyncStatus_.bind(this));
      this.addWebUiListener(
          'sync-status-changed', this.handleSyncStatus_.bind(this));
    }
  }


  override ready(): void {
    super.ready();

    this.addEventListener(
        AUTH_TOKEN_INVALID_EVENT_TYPE, this.onAuthTokenInvalid_);

    this.addFocusConfig(routes.ACCOUNTS, '#manageOtherPeopleRow');
    this.addFocusConfig(routes.LOCK_SCREEN, '#lockScreenRow');
    if (this.isRevampWayfindingEnabled_) {
      this.addFocusConfig(routes.SYNC, '#syncSetupRow');
    }
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
      // Handle the settings within the sync setup subpage since its a shared
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

    // Since the sync setup subpage is a shared subpage, so we handle deep links
    // for both this page and the sync setup subpage.
    if (newRoute === routes.SYNC || newRoute === this.route) {
      this.attemptDeepLink();
    }
  }

  /**
   * Looks up the translation id, which depends on PIN login support.
   */
  private selectLockScreenTitleString_(hasPinLogin: boolean): string {
    if (hasPinLogin) {
      return this.i18n('lockScreenTitleLoginLock');
    }
    return this.i18n('lockScreenTitleLock');
  }

  private getPasswordState_(hasPin: boolean, enableScreenLock: boolean):
      string {
    if (!enableScreenLock) {
      return this.i18n('lockScreenNone');
    }
    if (hasPin) {
      return this.i18n('lockScreenPinOrPassword');
    }
    return this.i18n('lockScreenPasswordOnly');
  }

  private getSyncAdvancedTitle_(): string {
    if (this.showSyncSettingsRevamp_) {
      return this.i18n('syncAdvancedDevicePageTitle');
    }
    return this.i18n('syncAdvancedPageTitle');
  }

  private getSyncAndGoogleServicesSubtext_(): string {
    if (this.syncStatus && this.syncStatus.hasError &&
        this.syncStatus.statusText) {
      return this.syncStatus.statusText;
    }
    return '';
  }

  private async onPasswordRequested_(): Promise<void> {
    // We get called twice from `settings-lock-screen-subpage` and
    // from `settings-fingerprint-list-subpage`. Once when the current route
    // changed after entering those pages, via the `currentRouteChanged`
    // overrides, and once from `onAuthTokenChanged` listeners that listen to
    // changes in `authToken` value, and potentially request a new token.
    // Prevent double token requests.
    if (this.isAuthenticating_) {
      return;
    }

    this.isAuthenticating_ = true;

    if (!this.isAuthPanelInSessionEnabled_) {
      this.showPasswordPromptDialog_ = true;
      this.isAuthenticating_ = false;
      return;
    }

    const tokenInfo = await InSessionAuth.getRemote().requestToken(
        Reason.kAccessAuthenticationSettings,
        loadTimeData.getString('authPrompt'));

    this.isAuthenticating_ = false;

    if (!tokenInfo.reply) {
      Router.getInstance().navigateToPreviousRoute();
      return;
    }

    this.authTokenReply_ = tokenInfo.reply;
  }

  private getAuthToken_(): string|undefined {
    if (!this.isAuthPanelInSessionEnabled_) {
      return this.authTokenInfo_?.token;
    }
    return this.authTokenReply_?.token;
  }

  /**
   * Invalidate the token to trigger a password re-prompt. Used for PIN auto
   * submit when too many attempts were made when using PrefStore based PIN.
   */
  private async onInvalidateTokenRequested_(): Promise<void> {
    if (!this.isAuthPanelInSessionEnabled_) {
      this.authTokenInfo_ = undefined;
      return;
    }

    if (this.authTokenReply_) {
      const token = this.authTokenReply_.token;
      this.authTokenReply_ = undefined;
      await InSessionAuth.getRemote().invalidateToken(token);
    }
  }

  private onPasswordPromptDialogClose_(): void {
    if (this.isAuthPanelInSessionEnabled_ && !this.authTokenReply_) {
      Router.getInstance().navigateToPreviousRoute();
      return;
    }

    if (!this.isAuthPanelInSessionEnabled_) {
      this.showPasswordPromptDialog_ = false;
      this.isAuthenticating_ = false;
      if (!this.authTokenInfo_) {
        Router.getInstance().navigateToPreviousRoute();
      }
    }
  }

  private onAuthTokenObtained_(
      e: CustomEvent<chrome.quickUnlockPrivate.TokenInfo>): void {
    this.authTokenInfo_ = e.detail;
  }

  /**
   * Should request the password again to get latest token.
   */
  private onAuthTokenInvalid_(): void {
    if (this.isAuthPanelInSessionEnabled_) {
      this.authTokenReply_ = undefined;
      return;
    }
    this.authTokenInfo_ = undefined;
  }

  private onConfigureLockClick_(e: Event): void {
    // Navigating to the lock screen will always open the password prompt
    // dialog, so prevent the end of the tap event to focus what is underneath
    // it, which takes focus from the dialog.
    e.preventDefault();
    Router.getInstance().navigateTo(routes.LOCK_SCREEN);
  }

  private onManageOtherPeople_(): void {
    Router.getInstance().navigateTo(routes.ACCOUNTS);
  }

  private onSmartPrivacy_(): void {
    Router.getInstance().navigateTo(routes.SMART_PRIVACY);
  }

  /**
   * Handler for when the sync state is pushed from the browser.
   */
  private handleSyncStatus_(syncStatus: SyncStatus): void {
    this.syncStatus = syncStatus;

    // When ChromeOSAccountManager is disabled, fall back to using the sync
    // username ("alice@gmail.com") as the profile label.
    if (!this.isAccountManagerEnabled_ && syncStatus &&
        this.syncStatus.signedInState === SignedInState.SYNCING &&
        syncStatus.signedInUsername) {
      this.profileLabel_ = syncStatus.signedInUsername;
    }
  }

  // Users can go to sync setup subpage regardless of sync status.
  private onSyncClick_(): void {
    Router.getInstance().navigateTo(routes.SYNC);
  }

  private onPrivacyHubClick_(): void {
    chrome.metricsPrivate.recordEnumerationValue(
        'ChromeOS.PrivacyHub.Opened',
        PrivacyHubNavigationOrigin.SYSTEM_SETTINGS,
        Object.keys(PrivacyHubNavigationOrigin).length);
    Router.getInstance().navigateTo(routes.PRIVACY_HUB);
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

  private onDisableProtectionDialogClosed_(): void {
    this.showDisableProtectionDialog_ = false;
  }

  private onPeripheralProtectionClick_(): void {
    if (!this.isUserConfigurable_) {
      return;
    }

    // Do not flip the actual toggle as this will flip the underlying pref.
    // Instead if the user is attempting to disable the toggle, present the
    // warning dialog.
    if (!this.getPref(this.dataAccessProtectionPrefName_).value) {
      this.showDisableProtectionDialog_ = true;
      return;
    }

    // The underlying settings-toggle-button is disabled, therefore we will have
    // to set the pref value manually to flip the toggle.
    this.setPrefValue(this.dataAccessProtectionPrefName_, false);
  }

  private onDataAccessToggleFocus_(): void {
    if (!this.isUserConfigurable_) {
      return;
    }

    // Don't consume the shift+tab focus event here. Instead redirect it to the
    // previous element.
    if (this.dataAccessShiftTabPressed_) {
      this.dataAccessShiftTabPressed_ = false;
      this.$.verifiedAccessToggle.focus();
      return;
    }

    this.shadowRoot!
        .querySelector<SettingsToggleButtonElement>(
            '.peripheral-data-access-protection')!.focus();
  }

  /**
   * Handles keyboard events in regards to #peripheralDataAccessProtection.
   * The underlying cr-toggle is disabled so we need to handle the keyboard
   * events manually.
   */
  private onDataAccessToggleKeyPress_(event: KeyboardEvent): void {
    // Handle Shift + Tab, we don't want to redirect back to the same toggle.
    if (event.shiftKey && event.key === 'Tab') {
      this.dataAccessShiftTabPressed_ = true;
      return;
    }

    if ((event.key !== 'Enter' && event.key !== ' ') ||
        !this.isUserConfigurable_) {
      return;
    }

    event.stopPropagation();

    if (!this.getPref(this.dataAccessProtectionPrefName_).value) {
      this.showDisableProtectionDialog_ = true;
      return;
    }
    this.setPrefValue(this.dataAccessProtectionPrefName_, false);
  }

  /**
   * This is used to add a keydown listener event for handling keyboard
   * navigation inputs. We have to wait until either
   * #crosSettingDataAccessToggle or #localStateDataAccessToggle is rendered
   * before adding the observer.
   */
  private onDataAccessFlagsSet_(): void {
    if (this.isThunderboltSupported_) {
      this.browserProxy_.getPolicyState()
          .then(policy => {
            this.dataAccessProtectionPrefName_ = policy.prefName;
            this.isUserConfigurable_ = policy.isUserConfigurable;
          })
          .then(() => {
            afterNextRender(this, () => {
              this.shadowRoot!
                  .querySelector<SettingsToggleButtonElement>(
                      '.peripheral-data-access-protection')!.shadowRoot!
                  .querySelector<HTMLElement>('#control')!.addEventListener(
                      'keydown', this.onDataAccessToggleKeyPress_.bind(this));
            });
          });
    }
  }

  private onVerifiedAccessChange_(): void {
    const enabled = this.$.verifiedAccessToggle.checked;
    recordSettingChange(Setting.kVerifiedAccess, {boolValue: enabled});
  }

  /**
   * @return true if the current data access pref is from the local_state.
   */
  private isLocalStateDataAccessPref_(): boolean {
    return this.dataAccessProtectionPrefName_ ===
        'settings.local_state_device_pci_data_access_enabled';
  }

  /**
   * @return true if the current data access pref is from the CrosSetting.
   */
  private isCrosSettingDataAccessPref_(): boolean {
    return this.dataAccessProtectionPrefName_ ===
        'cros.device.peripheral_data_access_enabled';
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [OsSettingsPrivacyPageElement.is]: OsSettingsPrivacyPageElement;
  }
}

customElements.define(
    OsSettingsPrivacyPageElement.is, OsSettingsPrivacyPageElement);
