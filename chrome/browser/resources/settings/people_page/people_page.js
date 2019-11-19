// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'settings-people-page' is the settings page containing sign-in settings.
 */
Polymer({
  is: 'settings-people-page',

  behaviors: [
    settings.RouteObserverBehavior, I18nBehavior, WebUIListenerBehavior,
    // <if expr="chromeos">
    CrPngBehavior, LockStateBehavior,
    // </if>
  ],

  properties: {
    /**
     * Preferences state.
     */
    prefs: {
      type: Object,
      notify: true,
    },

    // <if expr="not chromeos">
    /**
     * This flag is used to conditionally show a set of new sign-in UIs to the
     * profiles that have been migrated to be consistent with the web sign-ins.
     * TODO(tangltom): In the future when all profiles are completely migrated,
     * this should be removed, and UIs hidden behind it should become default.
     * @private
     */
    diceEnabled_: {
      type: Boolean,
      value: function() {
        return loadTimeData.getBoolean('diceEnabled');
      },
    },

    /**
     * Stored accounts to the system, supplied by SyncBrowserProxy.
     * @type {?Array<!settings.StoredAccount>}
     */
    storedAccounts: Object,
    // </if>

    /**
     * The current sync status, supplied by SyncBrowserProxy.
     * @type {?settings.SyncStatus}
     */
    syncStatus: Object,

    /**
     * Dictionary defining page visibility.
     * @type {!PageVisibility}
     */
    pageVisibility: Object,

    /**
     * Authentication token provided by settings-lock-screen.
     * @private
     */
    authToken_: {
      type: String,
      value: '',
    },

    /**
     * The currently selected profile icon URL. May be a data URL.
     * @private
     */
    profileIconUrl_: String,

    /**
     * Whether the profile row is clickable. The behavior depends on the
     * platform.
     * @private
     */
    isProfileActionable_: {
      type: Boolean,
      value: function() {
        if (!cr.isChromeOS) {
          // Opens profile manager.
          return true;
        }
        if (loadTimeData.getBoolean('showOSSettings')) {
          // Pre-SplitSettings opens change picture.
          return true;
        }
        // Post-SplitSettings links out to account manager if it is available.
        return loadTimeData.getBoolean('isAccountManagerEnabled');
      },
      readOnly: true,
    },

    /**
     * The current profile name.
     * @private
     */
    profileName_: String,

    // <if expr="chromeos">
    /** @private {string} */
    profileRowIconClass_: {
      type: String,
      value: function() {
        if (loadTimeData.getBoolean('showOSSettings')) {
          // Pre-SplitSettings links internally to the change picture subpage.
          return 'subpage-arrow';
        } else {
          // Post-SplitSettings links externally to account manager. If account
          // manager isn't available the icon will be hidden.
          return 'icon-external';
        }
      },
      readOnly: true,
    },

    /** @private {string} */
    profileRowIconAriaLabel_: {
      type: String,
      value: function() {
        if (loadTimeData.getBoolean('showOSSettings')) {
          // Pre-SplitSettings.
          return this.i18n('changePictureTitle');
        } else {
          // Post-SplitSettings. If account manager isn't available the icon
          // will be hidden so the label doesn't matter.
          return this.i18n('accountManagerSubMenuLabel');
        }
      },
      readOnly: true,
    },
    // </if>

    // <if expr="not chromeos">
    /** @private {boolean} */
    shouldShowGoogleAccount_: {
      type: Boolean,
      value: false,
      computed: 'computeShouldShowGoogleAccount_(storedAccounts, syncStatus,' +
          'storedAccounts.length, syncStatus.signedIn, syncStatus.hasError)',
    },

    /** @private */
    showImportDataDialog_: {
      type: Boolean,
      value: false,
    },
    // </if>

    /** @private */
    showSignoutDialog_: Boolean,

    // <if expr="chromeos">
    /**
     * True if fingerprint settings should be displayed on this machine.
     * @private
     */
    fingerprintUnlockEnabled_: {
      type: Boolean,
      value: function() {
        return loadTimeData.getBoolean('fingerprintUnlockEnabled');
      },
      readOnly: true,
    },

    /** @private */
    showParentalControls_: {
      type: Boolean,
      value: function() {
        return loadTimeData.valueExists('showParentalControls') &&
            loadTimeData.getBoolean('showParentalControls');
      },
    },
    // </if>

    /** @private {!Map<string, string>} */
    focusConfig_: {
      type: Object,
      value: function() {
        const map = new Map();
        if (settings.routes.SYNC) {
          map.set(settings.routes.SYNC.path, '#sync-setup');
        }
        // <if expr="not chromeos">
        if (settings.routes.MANAGE_PROFILE) {
          map.set(
              settings.routes.MANAGE_PROFILE.path,
              loadTimeData.getBoolean('diceEnabled') ?
                  '#edit-profile .subpage-arrow' :
                  '#picture-subpage-trigger .subpage-arrow');
        }
        // </if>
        // <if expr="chromeos">
        if (settings.routes.CHANGE_PICTURE) {
          map.set(
              settings.routes.CHANGE_PICTURE.path,
              '#picture-subpage-trigger .subpage-arrow');
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
        // </if>
        return map;
      },
    },
  },

  /** @private {?settings.SyncBrowserProxy} */
  syncBrowserProxy_: null,

  /** @override */
  attached: function() {
    let useProfileNameAndIcon = true;
    // <if expr="chromeos">
    if (!loadTimeData.getBoolean('showOSSettings') &&
        loadTimeData.getBoolean('isAccountManagerEnabled')) {
      // If this is SplitSettings and we have the Google Account manager,
      // prefer the GAIA name and icon.
      useProfileNameAndIcon = false;
      this.addWebUIListener(
          'accounts-changed', this.updateAccounts_.bind(this));
      this.updateAccounts_();
    }
    // </if>
    if (useProfileNameAndIcon) {
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

    // <if expr="not chromeos">
    const handleStoredAccounts = accounts => {
      this.storedAccounts = accounts;
    };
    this.syncBrowserProxy_.getStoredAccounts().then(handleStoredAccounts);
    this.addWebUIListener('stored-accounts-updated', handleStoredAccounts);

    this.addWebUIListener('sync-settings-saved', () => {
      /** @type {!CrToastElement} */ (this.$.toast).show();
    });
    // </if>
  },

  /** @protected */
  currentRouteChanged: function() {
    this.showImportDataDialog_ =
        settings.getCurrentRoute() == settings.routes.IMPORT_DATA;

    if (settings.getCurrentRoute() == settings.routes.SIGN_OUT) {
      // If the sync status has not been fetched yet, optimistically display
      // the sign-out dialog. There is another check when the sync status is
      // fetched. The dialog will be closed when the user is not signed in.
      if (this.syncStatus && !this.syncStatus.signedIn) {
        settings.navigateToPreviousRoute();
      } else {
        this.showSignoutDialog_ = true;
      }
    }
  },

  /**
   * @return {!Element}
   * @private
   */
  getEditPersonAssocControl_: function() {
    return this.diceEnabled_ ? assert(this.$$('#edit-profile')) :
                               assert(this.$$('#picture-subpage-trigger'));
  },

  // <if expr="chromeos">
  /** @private */
  getPasswordState_: function(hasPin, enableScreenLock) {
    if (!enableScreenLock) {
      return this.i18n('lockScreenNone');
    }
    if (hasPin) {
      return this.i18n('lockScreenPinOrPassword');
    }
    return this.i18n('lockScreenPasswordOnly');
  },
  // </if>

  /**
   * @return {string}
   * @private
   */
  getSyncAndGoogleServicesSubtext_: function() {
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
  handleProfileInfo_: function(info) {
    this.profileName_ = info.name;
    /**
     * Extract first frame from image by creating a single frame PNG using
     * url as input if base64 encoded and potentially animated.
     */
    // <if expr="chromeos">
    if (info.iconUrl.startsWith('data:image/png;base64')) {
      this.profileIconUrl_ =
          CrPngBehavior.convertImageSequenceToPng([info.iconUrl]);
      return;
    }
    // </if>

    this.profileIconUrl_ = info.iconUrl;
  },

  // <if expr="chromeos">
  /**
   * @private
   * @suppress {checkTypes} The types only exists in Chrome OS builds, but
   * Closure doesn't understand the <if> above.
   */
  updateAccounts_: async function() {
    const /** @type {!Array<{settings.Account}>} */ accounts =
        await settings.AccountManagerBrowserProxyImpl.getInstance()
            .getAccounts();
    // The user might not have any GAIA accounts (e.g. guest mode, Kerberos,
    // Active Directory). In these cases the profile row is hidden, so there's
    // nothing to do.
    if (accounts.length == 0) {
      return;
    }
    this.profileName_ = accounts[0].fullName;
    this.profileIconUrl_ = accounts[0].pic;
  },
  // </if>

  /**
   * Handler for when the sync state is pushed from the browser.
   * @param {?settings.SyncStatus} syncStatus
   * @private
   */
  handleSyncStatus_: function(syncStatus) {
    // Sign-in impressions should be recorded only if the sign-in promo is
    // shown. They should be recorder only once, the first time
    // |this.syncStatus| is set.
    const shouldRecordSigninImpression =
        !this.syncStatus && syncStatus && this.showSignin_(syncStatus);

    this.syncStatus = syncStatus;

    if (shouldRecordSigninImpression && !this.shouldShowSyncAccountControl_()) {
      // SyncAccountControl records the impressions user actions.
      chrome.metricsPrivate.recordUserAction('Signin_Impression_FromSettings');
    }
  },

  // <if expr="not chromeos">
  /**
   * @return {boolean}
   * @private
   */
  computeShouldShowGoogleAccount_: function() {
    if (this.storedAccounts === undefined || this.syncStatus === undefined) {
      return false;
    }

    return (this.storedAccounts.length > 0 || !!this.syncStatus.signedIn) &&
        !this.syncStatus.hasError;
  },
  // </if>

  /** @private */
  onProfileTap_: function() {
    // <if expr="chromeos">
    if (loadTimeData.getBoolean('showOSSettings')) {
      // Pre-SplitSettings.
      settings.navigateTo(settings.routes.CHANGE_PICTURE);
    } else if (loadTimeData.getBoolean('isAccountManagerEnabled')) {
      // Post-SplitSettings. The browser C++ code loads OS settings in a window.
      // Don't use window.open() because that creates an extra empty tab.
      window.location.href = 'chrome://os-settings/accountManager';
    }
    // </if>
    // <if expr="not chromeos">
    settings.navigateTo(settings.routes.MANAGE_PROFILE);
    // </if>
  },

  /** @private */
  onSigninTap_: function() {
    this.syncBrowserProxy_.startSignIn();
  },

  /** @private */
  onDisconnectDialogClosed_: function(e) {
    this.showSignoutDialog_ = false;
    // <if expr="not chromeos">
    if (!this.diceEnabled_) {
      // If DICE-enabled, this button won't exist here.
      cr.ui.focusWithoutInk(assert(this.$$('#disconnectButton')));
    }
    // </if>

    // <if expr="chromeos">
    cr.ui.focusWithoutInk(assert(this.$$('#disconnectButton')));
    // </if>

    if (settings.getCurrentRoute() == settings.routes.SIGN_OUT) {
      settings.navigateToPreviousRoute();
    }
  },

  /** @private */
  onDisconnectTap_: function() {
    settings.navigateTo(settings.routes.SIGN_OUT);
  },

  /** @private */
  onSyncTap_: function() {
    // Users can go to sync subpage regardless of sync status.
    settings.navigateTo(settings.routes.SYNC);
  },

  // <if expr="chromeos">
  /**
   * @param {!Event} e
   * @private
   */
  onConfigureLockTap_: function(e) {
    // Navigating to the lock screen will always open the password prompt
    // dialog, so prevent the end of the tap event to focus what is underneath
    // it, which takes focus from the dialog.
    e.preventDefault();
    settings.navigateTo(settings.routes.LOCK_SCREEN);
  },

  /**
   * @param {!Event} e
   * @private
   */
  onAccountManagerTap_: function(e) {
    settings.navigateTo(settings.routes.ACCOUNT_MANAGER);
  },

  /**
   * @param {!Event} e
   * @private
   */
  onKerberosAccountsTap_: function(e) {
    settings.navigateTo(settings.routes.KERBEROS_ACCOUNTS);
  },

  /** @private */
  onManageOtherPeople_: function() {
    settings.navigateTo(settings.routes.ACCOUNTS);
  },
  // </if>

  // <if expr="not chromeos">
  /** @private */
  onImportDataTap_: function() {
    settings.navigateTo(settings.routes.IMPORT_DATA);
  },

  /** @private */
  onImportDataDialogClosed_: function() {
    settings.navigateToPreviousRoute();
    cr.ui.focusWithoutInk(assert(this.$.importDataDialogTrigger));
  },

  /**
   * Open URL for managing your Google Account.
   * @private
   */
  openGoogleAccount_: function() {
    settings.OpenWindowProxyImpl.getInstance().openURL(
        loadTimeData.getString('googleAccountUrl'));
    chrome.metricsPrivate.recordUserAction('ManageGoogleAccount_Clicked');
  },

  /**
   * @return {boolean}
   * @private
   */
  shouldShowSyncAccountControl_: function() {
    if (this.syncStatus == undefined) {
      return false;
    }

    return this.diceEnabled_ && !!this.syncStatus.syncSystemEnabled &&
        !!this.syncStatus.signinAllowed;
  },
  // </if>

  /**
   * @param {string} iconUrl
   * @return {string} A CSS image-set for multiple scale factors.
   * @private
   */
  getIconImageSet_: function(iconUrl) {
    return cr.icon.getImage(iconUrl);
  },

  /**
   * @param {!settings.SyncStatus} syncStatus
   * @return {boolean} Whether to show the "Sign in to Chrome" button.
   * @private
   */
  showSignin_: function(syncStatus) {
    return !!syncStatus.signinAllowed && !syncStatus.signedIn;
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
});
