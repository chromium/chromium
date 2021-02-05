// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'settings-people-page' is the settings page containing sign-in settings.
 */
Polymer({
  is: 'os-settings-people-page',

  behaviors: [
    DeepLinkingBehavior,
    settings.RouteObserverBehavior,
    I18nBehavior,
    WebUIListenerBehavior,
    LockStateBehavior,
  ],

  properties: {
    /**
     * Preferences state.
     */
    prefs: {
      type: Object,
      notify: true,
    },

    splitSettingsSyncEnabled_: {
      type: Boolean,
      value() {
        return loadTimeData.getBoolean('splitSettingsSyncEnabled');
      },
    },

    /**
     * The current sync status, supplied by SyncBrowserProxy.
     * @type {?settings.SyncStatus}
     */
    syncStatus: Object,

    /**
     * Dictionary defining page visibility.
     * @type {!OSPageVisibility}
     */
    pageVisibility: Object,

    /**
     * Authentication token.
     * @private {!chrome.quickUnlockPrivate.TokenInfo|undefined}
     */
    authToken_: {
      type: Object,
      observer: 'onAuthTokenChanged_',
    },

    /**
     * The current profile icon URL. Usually a data:image/png URL.
     * @private
     */
    profileIconUrl_: String,

    /**
     * The current profile name, e.g. "John Cena".
     * @private
     */
    profileName_: String,

    /**
     * The current profile email, e.g. "john.cena@gmail.com".
     * @private
     */
    profileEmail_: String,

    /**
     * The label may contain additional text, for example:
     * "john.cena@gmail, + 2 more accounts".
     * @private
     */
    profileLabel_: String,

    /** @private */
    showSignoutDialog_: Boolean,

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
     * True if Chrome OS Account Manager is enabled.
     * @private
     */
    isAccountManagerEnabled_: {
      type: Boolean,
      value() {
        return loadTimeData.getBoolean('isAccountManagerEnabled');
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

    /** @private */
    showParentalControls_: {
      type: Boolean,
      value() {
        return loadTimeData.valueExists('showParentalControls') &&
            loadTimeData.getBoolean('showParentalControls');
      },
    },

    /** @private {!Map<string, string>} */
    focusConfig_: {
      type: Object,
      value() {
        const map = new Map();
        if (settings.routes.SYNC) {
          map.set(settings.routes.SYNC.path, '#sync-setup');
        }
        if (settings.routes.LOCK_SCREEN) {
          map.set(
              settings.routes.LOCK_SCREEN.path, '#lock-screen-subpage-trigger');
        }
        if (settings.routes.ACCOUNTS) {
          map.set(
              settings.routes.ACCOUNTS.path,
              '#manage-other-people-subpage-trigger');
        }
        if (settings.routes.ACCOUNT_MANAGER) {
          map.set(
              settings.routes.ACCOUNT_MANAGER.path,
              '#account-manager-subpage-trigger');
        }
        if (settings.routes.KERBEROS_ACCOUNTS) {
          map.set(
              settings.routes.KERBEROS_ACCOUNTS.path,
              '#kerberos-accounts-subpage-trigger');
        }
        return map;
      },
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
        chromeos.settings.mojom.Setting.kSetUpParentalControls,

        // Perform Sync page deep links here since it's a shared page.
        chromeos.settings.mojom.Setting.kNonSplitSyncEncryptionOptions,
        chromeos.settings.mojom.Setting.kAutocompleteSearchesAndUrls,
        chromeos.settings.mojom.Setting.kMakeSearchesAndBrowsingBetter,
        chromeos.settings.mojom.Setting.kGoogleDriveSearchSuggestions,
      ]),
    },
  },

  /** @private {?settings.SyncBrowserProxy} */
  syncBrowserProxy_: null,

  /** @override */
  attached() {
    if (this.isAccountManagerEnabled_) {
      // If we have the Google Account manager, use GAIA name and icon.
      this.addWebUIListener(
          'accounts-changed', this.updateAccounts_.bind(this));
      this.updateAccounts_();
    } else {
      // Otherwise use the Profile name and icon.
      settings.ProfileInfoBrowserProxyImpl.getInstance().getProfileInfo().then(
          this.handleProfileInfo_.bind(this));
      this.addWebUIListener(
          'profile-info-changed', this.handleProfileInfo_.bind(this));
    }

    this.syncBrowserProxy_ = settings.SyncBrowserProxyImpl.getInstance();
    this.syncBrowserProxy_.getSyncStatus().then(
        this.handleSyncStatus_.bind(this));
    this.addWebUIListener(
        'sync-status-changed', this.handleSyncStatus_.bind(this));
  },

  /** @private */
  onPasswordRequested_() {
    this.showPasswordPromptDialog_ = true;
  },

  // Invalidate the token to trigger a password re-prompt. Used for PIN auto
  // submit when too many attempts were made when using PrefStore based PIN.
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
   * Helper function for manually showing deep links on this page.
   * @param {!chromeos.settings.mojom.Setting} settingId
   * @param {!function():?Element} getElementCallback
   * @private
   */
  afterRenderShowDeepLink_(settingId, getElementCallback) {
    // Wait for element to load.
    Polymer.RenderStatus.afterNextRender(this, () => {
      const deepLinkElement = getElementCallback();
      if (!deepLinkElement || deepLinkElement.hidden) {
        console.warn(`Element with deep link id ${settingId} not focusable.`);
        return;
      }
      this.showDeepLinkElement(deepLinkElement);
    });
  },

  /**
   * Overridden from DeepLinkingBehavior.
   * @param {!chromeos.settings.mojom.Setting} settingId
   * @return {boolean}
   */
  beforeDeepLinkAttempt(settingId) {
    switch (settingId) {
      // Manually show the deep links for settings nested within elements.
      case chromeos.settings.mojom.Setting.kSetUpParentalControls:
        this.afterRenderShowDeepLink_(settingId, () => {
          const parentalPage =
              /** @type {?SettingsParentalControlsPageElement} */ (
                  this.$$('settings-parental-controls-page'));
          return parentalPage && parentalPage.getSetupButton();
        });
        // Stop deep link attempt since we completed it manually.
        return false;

      // Handle the settings within the old sync page since its a shared
      // component.
      case chromeos.settings.mojom.Setting.kNonSplitSyncEncryptionOptions:
        this.afterRenderShowDeepLink_(settingId, () => {
          const syncPage = /** @type {?SettingsSyncPageElement} */ (
              this.$$('settings-sync-page'));
          // Expand the encryption collapse.
          syncPage.forceEncryptionExpanded = true;
          Polymer.dom.flush();
          return syncPage && syncPage.getEncryptionOptions() &&
              syncPage.getEncryptionOptions().getEncryptionsRadioButtons();
        });
        return false;

      case chromeos.settings.mojom.Setting.kAutocompleteSearchesAndUrls:
        this.afterRenderShowDeepLink_(settingId, () => {
          const syncPage = /** @type {?SettingsSyncPageElement} */ (
              this.$$('settings-sync-page'));
          return syncPage && syncPage.getPersonalizationOptions() &&
              syncPage.getPersonalizationOptions().getSearchSuggestToggle();
        });
        return false;

      case chromeos.settings.mojom.Setting.kMakeSearchesAndBrowsingBetter:
        this.afterRenderShowDeepLink_(settingId, () => {
          const syncPage = /** @type {?SettingsSyncPageElement} */ (
              this.$$('settings-sync-page'));
          return syncPage && syncPage.getPersonalizationOptions() &&
              syncPage.getPersonalizationOptions().getUrlCollectionToggle();
        });
        return false;

      case chromeos.settings.mojom.Setting.kGoogleDriveSearchSuggestions:
        this.afterRenderShowDeepLink_(settingId, () => {
          const syncPage = /** @type {?SettingsSyncPageElement} */ (
              this.$$('settings-sync-page'));
          return syncPage && syncPage.getPersonalizationOptions() &&
              syncPage.getPersonalizationOptions().getDriveSuggestToggle();
        });
        return false;

      default:
        // Continue with deep linking attempt.
        return true;
    }
  },

  /**
   * settings.RouteObserverBehavior
   * @param {!settings.Route} route
   * @param {!settings.Route} oldRoute
   * @protected
   */
  currentRouteChanged(route, oldRoute) {
    if (settings.Router.getInstance().getCurrentRoute() ===
        settings.routes.OS_SIGN_OUT) {
      // If the sync status has not been fetched yet, optimistically display
      // the sign-out dialog. There is another check when the sync status is
      // fetched. The dialog will be closed when the user is not signed in.
      if (this.syncStatus && !this.syncStatus.signedIn) {
        settings.Router.getInstance().navigateToPreviousRoute();
      } else {
        this.showSignoutDialog_ = true;
      }
    }

    // The old sync page is a shared subpage, so we handle deep links for
    // both this page and the sync page. Not ideal.
    if (route === settings.routes.SYNC || route === settings.routes.OS_PEOPLE) {
      this.attemptDeepLink();
    }
  },

  /**
   * @param {!CustomEvent<!chrome.quickUnlockPrivate.TokenInfo>} e
   * @private
   * */
  onAuthTokenObtained_(e) {
    this.authToken_ = e.detail;
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

  /**
   * @return {string}
   * @private
   */
  getSyncRowLabel_() {
    if (this.splitSettingsSyncEnabled_) {
      return this.i18n('osSyncPageTitle');
    } else {
      return this.i18n('syncAndNonPersonalizedServices');
    }
  },

  /**
   * @return {string}
   * @private
   */
  getSyncAndGoogleServicesSubtext_() {
    if (this.syncStatus && this.syncStatus.hasError &&
        this.syncStatus.statusText) {
      return this.syncStatus.statusText;
    }
    return '';
  },

  /**
   * Handler for when the profile's icon and name is updated.
   * @private
   * @param {!settings.ProfileInfo} info
   */
  handleProfileInfo_(info) {
    this.profileName_ = info.name;
    // Extract first frame from image by creating a single frame PNG using
    // url as input if base64 encoded and potentially animated.
    if (info.iconUrl.startsWith('data:image/png;base64')) {
      this.profileIconUrl_ = cr.png.convertImageSequenceToPng([info.iconUrl]);
      return;
    }
    this.profileIconUrl_ = info.iconUrl;
  },

  /**
   * Handler for when the account list is updated.
   * @private
   */
  updateAccounts_: async function() {
    const /** @type {!Array<settings.Account>} */ accounts =
        await settings.AccountManagerBrowserProxyImpl.getInstance()
            .getAccounts();
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
  },

  /**
   * @param {!Array<settings.Account>} accounts
   * @private
   */
  async setProfileLabel(accounts) {
    if (this.isAccountManagementFlowsV2Enabled_) {
      // Template: "$1 Google accounts" with correct plural of "account".
      const labelTemplate = await cr.sendWithPromise(
          'getPluralString', 'profileLabel', accounts.length);

      // Final output: "X Google accounts"
      this.profileLabel_ = loadTimeData.substituteString(
          labelTemplate, accounts[0].email, accounts.length);
      return;
    }
    const moreAccounts = accounts.length - 1;
    // Template: "$1, +$2 more accounts" with correct plural of "account".
    // Localization handles the case of 0 more accounts.
    const labelTemplate = await cr.sendWithPromise(
        'getPluralString', 'profileLabel', moreAccounts);

    // Final output: "alice@gmail.com, +2 more accounts"
    this.profileLabel_ = loadTimeData.substituteString(
        labelTemplate, accounts[0].email, moreAccounts);
  },

  /**
   * Handler for when the sync state is pushed from the browser.
   * @param {?settings.SyncStatus} syncStatus
   * @private
   */
  handleSyncStatus_(syncStatus) {
    this.syncStatus = syncStatus;

    // When ChromeOSAccountManager is disabled, fall back to using the sync
    // username ("alice@gmail.com") as the profile label.
    if (!this.isAccountManagerEnabled_ && syncStatus && syncStatus.signedIn &&
        syncStatus.signedInUsername) {
      this.profileLabel_ = syncStatus.signedInUsername;
    }
  },

  /** @private */
  onSigninTap_() {
    this.syncBrowserProxy_.startSignIn();
  },

  /** @private */
  onDisconnectDialogClosed_(e) {
    this.showSignoutDialog_ = false;
    cr.ui.focusWithoutInk(assert(this.$$('#disconnectButton')));

    if (settings.Router.getInstance().getCurrentRoute() ===
        settings.routes.OS_SIGN_OUT) {
      settings.Router.getInstance().navigateToPreviousRoute();
    }
  },

  /** @private */
  onDisconnectTap_() {
    settings.Router.getInstance().navigateTo(settings.routes.OS_SIGN_OUT);
  },

  /** @private */
  onSyncTap_() {
    if (this.splitSettingsSyncEnabled_) {
      settings.Router.getInstance().navigateTo(settings.routes.OS_SYNC);
      return;
    }

    // Users can go to sync subpage regardless of sync status.
    settings.Router.getInstance().navigateTo(settings.routes.SYNC);
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

  /**
   * @param {!Event} e
   * @private
   */
  onAccountManagerTap_(e) {
    if (this.isAccountManagerEnabled_) {
      settings.Router.getInstance().navigateTo(settings.routes.ACCOUNT_MANAGER);
    }
  },

  /** @private */
  onKerberosAccountsTap_() {
    settings.Router.getInstance().navigateTo(settings.routes.KERBEROS_ACCOUNTS);
  },

  /** @private */
  onManageOtherPeople_() {
    assert(
        !this.isAccountManagementFlowsV2Enabled_,
        'onManageOtherPeople_ was called when kAccountManagementFlowsV2 is enabled');
    settings.Router.getInstance().navigateTo(settings.routes.ACCOUNTS);
  },

  /**
   * @param {string} iconUrl
   * @return {string} A CSS image-set for multiple scale factors.
   * @private
   */
  getIconImageSet_(iconUrl) {
    return cr.icon.getImage(iconUrl);
  },

  /**
   * @return {string}
   * @private
   */
  getProfileName_() {
    if (this.isAccountManagerEnabled_ &&
        this.isAccountManagementFlowsV2Enabled_) {
      return loadTimeData.getString('osProfileName');
    }
    return this.profileName_;
  },

  /**
   * @return {string}
   * @private
   */
  getSyncSetupIcon_() {
    return this.isAccountManagementFlowsV2Enabled_ ? 'cr:sync' : '';
  },

  /**
   * @param {!settings.SyncStatus} syncStatus
   * @return {boolean} Whether to show the "Sign in to Chrome" button.
   * @private
   */
  showSignin_(syncStatus) {
    return loadTimeData.getBoolean('signinAllowed') && !syncStatus.signedIn;
  },

  /**
   * Looks up the translation id, which depends on PIN login support.
   * @param {boolean} hasPinLogin
   * @private
   */
  selectLockScreenTitleString(hasPinLogin) {
    if (hasPinLogin) {
      return this.i18n('lockScreenTitleLoginLock');
    }
    return this.i18n('lockScreenTitleLock');
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
});
