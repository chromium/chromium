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
import './peripheral_data_access_protection_dialog.js';
import '../../controls/settings_toggle_button.js';
import '../../settings_shared.css.js';
import '../os_settings_page/os_settings_subpage.js';
import '../os_people_page/users_page.js';
import '../os_settings_page/os_settings_animated_pages.js';
import '../os_people_page/lock_screen.js';
import '../os_people_page/lock_screen_password_prompt_dialog.js';
import './metrics_consent_toggle_button.js';

import {loadTimeData} from 'chrome://resources/ash/common/load_time_data.m.js';
import {afterNextRender, html, mixinBehaviors, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {Setting} from '../../mojom-webui/setting.mojom-webui.js';
import {Route, Router} from '../router.js';
import {DeepLinkingBehavior, DeepLinkingBehaviorInterface} from '../deep_linking_behavior.js';
import {LockStateBehavior, LockStateBehaviorInterface} from '../os_people_page/lock_state_behavior.js';
import {routes} from '../os_route.js';
import {PrefsBehavior, PrefsBehaviorInterface} from '../prefs_behavior.js';
import {RouteObserverBehavior, RouteObserverBehaviorInterface} from '../route_observer_behavior.js';

import {PeripheralDataAccessBrowserProxy, PeripheralDataAccessBrowserProxyImpl} from './peripheral_data_access_browser_proxy.js';
import {PrivacyHubNavigationOrigin} from './privacy_hub_page.js';

/**
 * @constructor
 * @extends {PolymerElement}
 * @implements {DeepLinkingBehaviorInterface}
 * @implements {RouteObserverBehaviorInterface}
 * @implements {LockStateBehaviorInterface}
 * @implements {PrefsBehaviorInterface}
 */
const OsSettingsPrivacyPageElementBase = mixinBehaviors(
    [
      DeepLinkingBehavior,
      RouteObserverBehavior,
      LockStateBehavior,
      PrefsBehavior,
    ],
    PolymerElement);

/** @polymer */
class OsSettingsPrivacyPageElement extends OsSettingsPrivacyPageElementBase {
  static get is() {
    return 'os-settings-privacy-page';
  }

  static get template() {
    return html`{__html_template__}`;
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

      /** @private {!Map<string, string>} */
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
       * @private {!chrome.quickUnlockPrivate.TokenInfo|undefined}
       */
      authToken_: {
        type: Object,
        observer: 'onAuthTokenChanged_',
      },

      /** @private {boolean} */
      showPasswordPromptDialog_: {
        type: Boolean,
        value: false,
      },

      /**
       * setModes_ is a partially applied function that stores the current auth
       * token. It's defined only when the user has entered a valid password.
       * @type {Object|undefined}
       * @private
       */
      setModes_: {
        type: Object,
      },

      /**
       * Used by DeepLinkingBehavior to focus this page's deep links.
       * @type {!Set<!Setting>}
       */
      supportedSettingIds: {
        type: Object,
        value: () => new Set([
          Setting.kVerifiedAccess,
          Setting.kUsageStatsAndCrashReports,
        ]),
      },

      /**
       * True if fingerprint settings should be displayed on this machine.
       * @private
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
       * @private
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
       * @private
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
       * @private {boolean}
       */
      isGuestMode_: {
        type: Boolean,
        value() {
          return loadTimeData.getBoolean('isGuest');
        },
        readOnly: true,
      },

      /** @private */
      showDisableProtectionDialog_: {
        type: Boolean,
        value: false,
      },

      /** @private */
      isThunderboltSupported_: {
        type: Boolean,
        value: false,
      },

      /** @private */
      dataAccessProtectionPrefName_: {
        type: String,
        value: '',
      },

      /** @private */
      isUserConfigurable_: {
        type: Boolean,
        value: false,
        reflectToAttribute: true,
      },

      /** @private */
      dataAccessShiftTabPressed_: {
        type: Boolean,
        value: false,
      },

      /**
       * Whether the secure DNS setting should be displayed.
       * @private
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
       * @private
       */
      showPrivacyHubPage_: {
        type: Boolean,
        readOnly: true,
        value: function() {
          return loadTimeData.getBoolean('showPrivacyHubPage') &&
              !loadTimeData.getBoolean('isGuest');
        },
      },
    };
  }

  static get observers() {
    return ['onDataAccessFlagsSet_(isThunderboltSupported_.*)'];
  }

  /** @override */
  constructor() {
    super();

    /** @private {!PeripheralDataAccessBrowserProxy} */
    this.browserProxy_ = PeripheralDataAccessBrowserProxyImpl.getInstance();

    /**
     * The timeout ID to pass to clearTimeout() to cancel auth token
     * invalidation.
     * @private {number|undefined}
     */
    this.clearAccountPasswordTimeoutId_ = undefined;

    this.browserProxy_.isThunderboltSupported().then(enabled => {
      this.isThunderboltSupported_ = enabled;
      if (this.isThunderboltSupported_) {
        this.supportedSettingIds.add(Setting.kPeripheralDataAccessProtection);
      }
    });
  }

  /** @override */
  ready() {
    super.ready();

    this.addEventListener('auth-token-invalid', this.onAuthTokenInvalid_);
  }

  /**
   * @param {!Route} route
   */
  currentRouteChanged(route) {
    // Does not apply to this page.
    if (route !== routes.OS_PRIVACY) {
      return;
    }

    this.attemptDeepLink();
  }

  /**
   * Looks up the translation id, which depends on PIN login support.
   * @param {boolean} hasPinLogin
   * @private
   */
  selectLockScreenTitleString_(hasPinLogin) {
    if (hasPinLogin) {
      return this.i18n('lockScreenTitleLoginLock');
    }
    return this.i18n('lockScreenTitleLock');
  }

  /** @private */
  getPasswordState_(hasPin, enableScreenLock) {
    if (!enableScreenLock) {
      return this.i18n('lockScreenNone');
    }
    if (hasPin) {
      return this.i18n('lockScreenPinOrPassword');
    }
    return this.i18n('lockScreenPasswordOnly');
  }

  /** @private */
  onPasswordRequested_() {
    this.showPasswordPromptDialog_ = true;
  }

  /**
   * Invalidate the token to trigger a password re-prompt. Used for PIN auto
   * submit when too many attempts were made when using PrefStore based PIN.
   * @private
   */
  onInvalidateTokenRequested_() {
    this.authToken_ = undefined;
  }

  /** @private */
  onPasswordPromptDialogClose_() {
    this.showPasswordPromptDialog_ = false;
    if (!this.setModes_) {
      Router.getInstance().navigateToPreviousRoute();
    }
  }

  /**
   * @param {!CustomEvent<!chrome.quickUnlockPrivate.TokenInfo>} e
   * @private
   * */
  onAuthTokenObtained_(e) {
    this.authToken_ = e.detail;
  }

  /**
   * Should request the password again to get latest token.
   * @private
   */
  onAuthTokenInvalid_() {
    this.setModes_ = undefined;
  }

  /**
   * @param {!Event} e
   * @private
   */
  onConfigureLockTap_(e) {
    // Navigating to the lock screen will always open the password prompt
    // dialog, so prevent the end of the tap event to focus what is underneath
    // it, which takes focus from the dialog.
    e.preventDefault();
    Router.getInstance().navigateTo(routes.LOCK_SCREEN);
  }

  /** @private */
  onManageOtherPeople_() {
    Router.getInstance().navigateTo(routes.ACCOUNTS);
  }

  /** @private */
  onSmartPrivacy_() {
    Router.getInstance().navigateTo(routes.SMART_PRIVACY);
  }

  /** @private */
  onPrivacyHubClick_() {
    chrome.metricsPrivate.recordEnumerationValue(
        'ChromeOS.PrivacyHub.Opened',
        PrivacyHubNavigationOrigin.SYSTEM_SETTINGS,
        Object.keys(PrivacyHubNavigationOrigin).length);
    Router.getInstance().navigateTo(routes.PRIVACY_HUB);
  }

  /** @private */
  onAuthTokenChanged_() {
    if (this.authToken_ === undefined) {
      this.setModes_ = undefined;
    } else {
      this.setModes_ = (modes, credentials, onComplete) => {
        this.quickUnlockPrivate.setModes(
            this.authToken_.token, modes, credentials, () => {
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

  /** @private */
  onDisableProtectionDialogClosed_() {
    this.showDisableProtectionDialog_ = false;
  }

  /** @private */
  onPeripheralProtectionClick_() {
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

  /** @private */
  onDataAccessToggleFocus_() {
    if (!this.isUserConfigurable_) {
      return;
    }

    // Don't consume the shift+tab focus event here. Instead redirect it to the
    // previous element.
    if (this.dataAccessShiftTabPressed_) {
      this.dataAccessShiftTabPressed_ = false;
      this.shadowRoot.querySelector('#enableVerifiedAccess').focus();
      return;
    }

    this.shadowRoot.querySelector('.peripheral-data-access-protection').focus();
  }

  /**
   * Handles keyboard events in regards to #peripheralDataAccessProtection.
   * The underlying cr-toggle is disabled so we need to handle the keyboard
   * events manually.
   * @param {!Event} event
   * @private
   */
  onDataAccessToggleKeyPress_(event) {
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
   * @private
   */
  onDataAccessFlagsSet_() {
    if (this.isThunderboltSupported_) {
      this.browserProxy_.getPolicyState()
          .then(policy => {
            this.dataAccessProtectionPrefName_ = policy.prefName;
            this.isUserConfigurable_ = policy.isUserConfigurable;
          })
          .then(() => {
            afterNextRender(this, () => {
              this.shadowRoot
                  .querySelector('.peripheral-data-access-protection')
                  .shadowRoot.querySelector('#control')
                  .addEventListener(
                      'keydown', this.onDataAccessToggleKeyPress_.bind(this));
            });
          });
    }
  }

  /**
   * @return {boolean} returns true if the current data access pref is from the
   * local_state.
   * @private
   */
  isLocalStateDataAccessPref_() {
    return this.dataAccessProtectionPrefName_ ===
        'settings.local_state_device_pci_data_access_enabled';
  }

  /**
   * @return {boolean} returns true if the current data access pref is from the
   * CrosSetting.
   * @private
   */
  isCrosSettingDataAccessPref_() {
    return this.dataAccessProtectionPrefName_ ===
        'cros.device.peripheral_data_access_enabled';
  }
}

customElements.define(
    OsSettingsPrivacyPageElement.is, OsSettingsPrivacyPageElement);
