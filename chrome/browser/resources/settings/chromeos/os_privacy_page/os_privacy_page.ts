// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'os-settings-privacy-page' is the settings page containing privacy and
 * security settings.
 */

import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import 'chrome://resources/cr_elements/cr_link_row/cr_link_row.js';
import 'chrome://resources/polymer/v3_0/paper-spinner/paper-spinner-lite.js';
import '/shared/settings/controls/settings_toggle_button.js';
import '../settings_shared.css.js';
import '../os_settings_page/os_settings_animated_pages.js';
import '../os_settings_page/os_settings_section.js';
import '../os_settings_page/os_settings_subpage.js';
import './metrics_consent_toggle_button.js';
import './peripheral_data_access_protection_dialog.js';

import {SettingsToggleButtonElement} from '/shared/settings/controls/settings_toggle_button.js';
import {PrefsMixin} from 'chrome://resources/cr_components/settings_prefs/prefs_mixin.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {afterNextRender, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {DeepLinkingMixin} from '../deep_linking_mixin.js';
import {LockStateMixin} from '../lock_state_mixin.js';
import {recordSettingChange} from '../metrics_recorder.js';
import {Section} from '../mojom-webui/routes.mojom-webui.js';
import {Setting} from '../mojom-webui/setting.mojom-webui.js';
import {RouteObserverMixin} from '../route_observer_mixin.js';
import {Route, Router, routes} from '../router.js';

import {getTemplate} from './os_privacy_page.html.js';
import {PeripheralDataAccessBrowserProxy, PeripheralDataAccessBrowserProxyImpl} from './peripheral_data_access_browser_proxy.js';
import {PrivacyHubBrowserProxy, PrivacyHubBrowserProxyImpl} from './privacy_hub_browser_proxy.js';
import {PrivacyHubNavigationOrigin} from './privacy_hub_subpage.js';

export interface OsSettingsPrivacyPageElement {
  $: {
    verifiedAccessToggle: SettingsToggleButtonElement,
  };
}

const OsSettingsPrivacyPageElementBase = PrefsMixin(
    LockStateMixin(RouteObserverMixin(DeepLinkingMixin(PolymerElement))));

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

      focusConfig_: {
        type: Object,
        value() {
          const map = new Map();
          if (routes.ACCOUNTS) {
            map.set(routes.ACCOUNTS.path, '#manageOtherPeopleSubpageTrigger');
          }
          if (routes.LOCK_SCREEN) {
            map.set(routes.LOCK_SCREEN.path, '#lockScreenSubpageTrigger');
          }
          return map;
        },
      },

      /**
       * Authentication token.
       */
      authToken_: {
        type: Object,
        observer: 'onAuthTokenChanged_',
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
          Setting.kVerifiedAccess,
          Setting.kUsageStatsAndCrashReports,
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

      /**
       * Whether the secure DNS setting should be displayed.
       */
      showSecureDnsSetting_: {
        type: Boolean,
        readOnly: true,
        value: function() {
          return loadTimeData.getBoolean('showSecureDnsSetting');
        },
      },

      /**
       * Whether privacy hub should be displayed.
       */
      showPrivacyHubPage_: {
        type: Boolean,
        readOnly: true,
        value: function() {
          return loadTimeData.getBoolean('showPrivacyHubPage') &&
              !loadTimeData.getBoolean('isGuest');
        },
      },

      isHatsSurveyEnabled_: {
        type: Boolean,
        readOnly: true,
        value: function() {
          return loadTimeData.getBoolean('isPrivacyHubHatsEnabled');
        },
      },
    };
  }

  static get observers() {
    return ['onDataAccessFlagsSet_(isThunderboltSupported_.*)'];
  }

  private authToken_: chrome.quickUnlockPrivate.TokenInfo|undefined;
  private browserProxy_: PeripheralDataAccessBrowserProxy;
  private privacyHubBrowserProxy_: PrivacyHubBrowserProxy;

  /**
   * The timeout ID to pass to clearTimeout() to cancel auth token
   * invalidation.
   */
  private clearAccountPasswordTimeoutId_: number|undefined = undefined;
  private dataAccessProtectionPrefName_: string;
  private dataAccessShiftTabPressed_: boolean;
  private fingerprintUnlockEnabled_: boolean;
  private focusConfig_: Map<string, string>;
  private isGuestMode_: boolean;
  private isHatsSurveyEnabled_: boolean;
  private isRevenBranding_: boolean;
  private isSmartPrivacyEnabled_: boolean;
  private isThunderboltSupported_: boolean;
  private isUserConfigurable_: boolean;
  private section_: Section;
  private setModes_: Object|undefined;
  private showDisableProtectionDialog_: boolean;
  private showPasswordPromptDialog_: boolean;
  private showPrivacyHubPage_: boolean;
  private showSecureDnsSetting_: boolean;

  constructor() {
    super();

    this.browserProxy_ = PeripheralDataAccessBrowserProxyImpl.getInstance();

    this.browserProxy_.isThunderboltSupported().then(enabled => {
      this.isThunderboltSupported_ = enabled;
      if (this.isThunderboltSupported_) {
        this.supportedSettingIds.add(Setting.kPeripheralDataAccessProtection);
      }
    });

    this.privacyHubBrowserProxy_ = PrivacyHubBrowserProxyImpl.getInstance();
  }

  override ready(): void {
    super.ready();

    this.addEventListener('auth-token-invalid', this.onAuthTokenInvalid_);
  }

  override currentRouteChanged(route: Route): void {
    // Does not apply to this page.
    if (route !== routes.OS_PRIVACY) {
      if (this.isHatsSurveyEnabled_) {
        this.privacyHubBrowserProxy_.sendLeftOsPrivacyPage();
      }
      return;
    }
    if (this.isHatsSurveyEnabled_) {
      this.privacyHubBrowserProxy_.sendOpenedOsPrivacyPage();
    }
    this.attemptDeepLink();
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

  private onPasswordRequested_(): void {
    this.showPasswordPromptDialog_ = true;
  }

  /**
   * Invalidate the token to trigger a password re-prompt. Used for PIN auto
   * submit when too many attempts were made when using PrefStore based PIN.
   */
  private onInvalidateTokenRequested_(): void {
    this.authToken_ = undefined;
  }

  private onPasswordPromptDialogClose_(): void {
    this.showPasswordPromptDialog_ = false;
    if (!this.setModes_) {
      Router.getInstance().navigateToPreviousRoute();
    }
  }

  private onAuthTokenObtained_(
      e: CustomEvent<chrome.quickUnlockPrivate.TokenInfo>): void {
    this.authToken_ = e.detail;
  }

  /**
   * Should request the password again to get latest token.
   */
  private onAuthTokenInvalid_(): void {
    this.setModes_ = undefined;
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

  private onPrivacyHubClick_(): void {
    chrome.metricsPrivate.recordEnumerationValue(
        'ChromeOS.PrivacyHub.Opened',
        PrivacyHubNavigationOrigin.SYSTEM_SETTINGS,
        Object.keys(PrivacyHubNavigationOrigin).length);
    Router.getInstance().navigateTo(routes.PRIVACY_HUB);
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
