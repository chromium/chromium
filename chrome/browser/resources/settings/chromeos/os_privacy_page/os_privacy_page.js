// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'os-settings-privacy-page' is the settings page containing privacy and
 * security settings.
 */

Polymer({
  is: 'os-settings-privacy-page',

  behaviors: [
    DeepLinkingBehavior,
    settings.RouteObserverBehavior,
    LockStateBehavior,
    PrefsBehavior,
  ],

  properties: {
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
        if (settings.routes.ACCOUNTS) {
          map.set(
              settings.routes.ACCOUNTS.path,
              '#manageOtherPeopleSubpageTrigger');
        }
        if (settings.routes.LOCK_SCREEN) {
          map.set(
              settings.routes.LOCK_SCREEN.path, '#lockScreenSubpageTrigger');
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
     * @type {!Set<!chromeos.settings.mojom.Setting>}
     */
    supportedSettingIds: {
      type: Object,
      value: () => new Set([
        chromeos.settings.mojom.Setting.kVerifiedAccess,
        chromeos.settings.mojom.Setting.kUsageStatsAndCrashReports,
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
     * True if redesign of account management flows is enabled.
     * @private
     */
    isAccountManagementFlowsV2Enabled_: {
      type: Boolean,
      value() {
        return loadTimeData.getBoolean('isAccountManagementFlowsV2Enabled');
      },
      readOnly: true,
    },

    /**
     * True if Pciguard UI is enabled.
     * @private
     */
    isPciguardUiEnabled_: {
      type: Boolean,
      value() {
        return loadTimeData.getBoolean('pciguardUiEnabled');
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
    isPeripheralProtectionToggleEnforced_: {
      type: Boolean,
      computed: 'computeIsPeripheralProtectionToggleEnforced_(' +
          'prefs.cros.device.peripheral_data_access_enabled.*)',
      reflectToAttribute: true,
    },

    /** @private */
    dataAccessShiftTabPressed_: {
      type: Boolean,
      value: false,
    },

    /** @private */
    shouldShowSubsections_: {
      type: Boolean,
      computed: 'computeShouldShowSubsections_(' +
          'isAccountManagementFlowsV2Enabled_, isGuestMode_)',
    },
  },

  /** @private {?settings.PeripheralDataAccessBrowserProxy} */
  browserProxy_: null,

  observers: ['onDataAccessFlagsSet_(isThunderboltSupported_.*)'],

  /** @override */
  created() {
    this.browserProxy_ =
        settings.PeripheralDataAccessBrowserProxyImpl.getInstance();

    this.browserProxy_.isThunderboltSupported().then(enabled => {
      this.isThunderboltSupported_ = enabled;
      if (this.isPciguardUiEnabled_ && this.isThunderboltSupported_) {
        this.supportedSettingIds.add(
            chromeos.settings.mojom.Setting.kPeripheralDataAccessProtection);
      }
    });
  },

  /**
   * @param {!settings.Route} route
   * @param {!settings.Route} oldRoute
   */
  currentRouteChanged(route, oldRoute) {
    // Does not apply to this page.
    if (route !== settings.routes.OS_PRIVACY) {
      return;
    }

    this.attemptDeepLink();
  },

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
  },

  /** @private */
  getPasswordState_(hasPin, enableScreenLock) {
    if (!enableScreenLock) {
      return this.i18n('lockScreenNone');
    }
    if (hasPin) {
      return this.i18n('lockScreenPinOrPassword');
    }
    return this.i18n('lockScreenPasswordOnly');
  },

  /** @private */
  onPasswordRequested_() {
    this.showPasswordPromptDialog_ = true;
  },

  /**
   * Invalidate the token to trigger a password re-prompt. Used for PIN auto
   * submit when too many attempts were made when using PrefStore based PIN.
   * @private
   */
  onInvalidateTokenRequested_() {
    this.authToken_ = undefined;
  },

  /** @private */
  onPasswordPromptDialogClose_() {
    this.showPasswordPromptDialog_ = false;
    if (!this.setModes_) {
      settings.Router.getInstance().navigateToPreviousRoute();
    }
  },

  /**
   * @param {!CustomEvent<!chrome.quickUnlockPrivate.TokenInfo>} e
   * @private
   * */
  onAuthTokenObtained_(e) {
    this.authToken_ = e.detail;
  },

  /**
   * @param {!Event} e
   * @private
   */
  onConfigureLockTap_(e) {
    // Navigating to the lock screen will always open the password prompt
    // dialog, so prevent the end of the tap event to focus what is underneath
    // it, which takes focus from the dialog.
    e.preventDefault();
    settings.Router.getInstance().navigateTo(settings.routes.LOCK_SCREEN);
  },

  /** @private */
  onManageOtherPeople_() {
    settings.Router.getInstance().navigateTo(settings.routes.ACCOUNTS);
  },

  /**
   * The timeout ID to pass to clearTimeout() to cancel auth token
   * invalidation.
   * @private {number|undefined}
   */
  clearAccountPasswordTimeoutId_: undefined,

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

    if (this.clearAuthTokenTimeoutId_) {
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
  },

  /** @private */
  onDisableProtectionDialogClosed_() {
    this.showDisableProtectionDialog_ = false;
  },

  /** @private */
  onPeripheralProtectionClick_() {
    if (this.isPeripheralProtectionToggleEnforced_) {
      return;
    }

    // Do not flip the actual toggle as this will flip the underlying pref.
    // Instead if the user is attempting to disable the toggle, present the
    // warning dialog.
    if (!this.prefs['cros']['device']['peripheral_data_access_enabled'].value) {
      this.showDisableProtectionDialog_ = true;
      return;
    }

    // The underlying settings-toggle-button is disabled, therefore we will have
    // to set the pref value manually to flip the toggle.
    this.setPrefValue('cros.device.peripheral_data_access_enabled', false);
  },

  /**
   * @return {boolean} True is the toggle is enforced.
   * @private
   */
  computeIsPeripheralProtectionToggleEnforced_() {
    return this.prefs['cros']['device']['peripheral_data_access_enabled']
               .enforcement === chrome.settingsPrivate.Enforcement.ENFORCED;
  },

  /** @private */
  onDataAccessToggleFocus_() {
    if (this.isPeripheralProtectionToggleEnforced_) {
      return;
    }

    // Don't consume the shift+tab focus event here. Instead redirect it to the
    // previous element.
    if (this.dataAccessShiftTabPressed_) {
      this.dataAccessShiftTabPressed_ = false;
      this.$$('#enableVerifiedAccess').focus();
      return;
    }

    this.$$('#peripheralDataAccessProtection').focus();
  },

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
        this.isPeripheralProtectionToggleEnforced_) {
      return;
    }

    event.stopPropagation();

    if (!this.prefs['cros']['device']['peripheral_data_access_enabled'].value) {
      this.showDisableProtectionDialog_ = true;
      return;
    }
    this.setPrefValue('cros.device.peripheral_data_access_enabled', false);
  },

  /**
   * This is used to add a keydown listener event for handling keyboard
   * navigation inputs. We have to wait until #peripheralDataAccessProtection
   * is rendered before adding the observer.
   * @private
   */
  onDataAccessFlagsSet_() {
    if (this.isThunderboltSupported_ && this.isPciguardUiEnabled_) {
      Polymer.RenderStatus.afterNextRender(this, () => {
        this.$$('#peripheralDataAccessProtection')
            .$$('#control')
            .addEventListener(
                'keydown', this.onDataAccessToggleKeyPress_.bind(this));
      });
    }
  },

  /**
   * @return {boolean} whether 'accounts' and 'lock screen' subsections should
   * be shown.
   * @private
   */
  computeShouldShowSubsections_() {
    return this.isAccountManagementFlowsV2Enabled_ && !this.isGuestMode_;
  }
});
