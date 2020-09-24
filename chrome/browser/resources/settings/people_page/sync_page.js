// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(function() {

// TODO(rbpotter): Remove this typedef when this file is no longer needed by OS
// Settings.
/**
 * @typedef {{
 *   BASIC: !settings.Route,
 *   PEOPLE: !settings.Route,
 *   SYNC: !settings.Route,
 *   SYNC_ADVANCED: !settings.Route,
 * }}
 */
let SyncRoutes;

/** @return {!SyncRoutes} */
function getSyncRoutes() {
  const router = settings.Router.getInstance();
  return /** @type {!SyncRoutes} */ (router.getRoutes());
}

/**
 * @fileoverview
 * 'settings-sync-page' is the settings page containing sync settings.
 */
Polymer({
  is: 'settings-sync-page',

  behaviors: [
    WebUIListenerBehavior,
    settings.RouteObserverBehavior,
  ],

  properties: {
    /**
     * Preferences state.
     */
    prefs: {
      type: Object,
      notify: true,
    },

    /** @private {!Map<string, (string|Function)>} */
    focusConfig: {
      type: Object,
      observer: 'onFocusConfigChange_',
    },

    /** @private */
    pages_: {
      type: Object,
      value: settings.PageStatus,
      readOnly: true,
    },

    /**
     * The current page status. Defaults to |CONFIGURE| such that the searching
     * algorithm can search useful content when the page is not visible to the
     * user.
     * @private {?settings.PageStatus}
     */
    pageStatus_: {
      type: String,
      value: settings.PageStatus.CONFIGURE,
    },

    /**
     * Dictionary defining page visibility.
     * TODO(dpapad): Restore the type information here (PrivacyPageVisibility),
     * when this file is no longer shared with chrome://os-settings.
     */
    pageVisibility: Object,

    /**
     * The current sync preferences, supplied by SyncBrowserProxy.
     * @type {settings.SyncPrefs|undefined}
     */
    syncPrefs: {
      type: Object,
    },

    /** @type {settings.SyncStatus} */
    syncStatus: {
      type: Object,
    },

    /** @private */
    encryptionExpanded_: {
      type: Boolean,
      value: false,
    },

    /** If true, override |encryptionExpanded_| to be true. */
    forceEncryptionExpanded: {
      type: Boolean,
      value: false,
    },

    /**
     * The existing passphrase input field value.
     * @private
     */
    existingPassphrase_: {
      type: String,
      value: '',
    },

    /** @private */
    signedIn_: {
      type: Boolean,
      value: true,
      computed: 'computeSignedIn_(syncStatus.signedIn)',
    },

    /** @private */
    syncDisabledByAdmin_: {
      type: Boolean,
      value: false,
      computed: 'computeSyncDisabledByAdmin_(syncStatus.managed)',
    },

    /** @private */
    syncSectionDisabled_: {
      type: Boolean,
      value: false,
      computed: 'computeSyncSectionDisabled_(' +
          'syncStatus.signedIn, syncStatus.disabled, ' +
          'syncStatus.hasError, syncStatus.statusAction, ' +
          'syncPrefs.trustedVaultKeysRequired)',
    },

    /** @private */
    showSetupCancelDialog_: {
      type: Boolean,
      value: false,
    },
  },

  observers: [
    'expandEncryptionIfNeeded_(syncPrefs.encryptAllData, forceEncryptionExpanded)',
  ],

  /** @private {?settings.SyncBrowserProxy} */
  browserProxy_: null,

  /**
   * The beforeunload callback is used to show the 'Leave site' dialog. This
   * makes sure that the user has the chance to go back and confirm the sync
   * opt-in before leaving.
   *
   * This property is non-null if the user is currently navigated on the sync
   * settings route.
   *
   * @private {?Function}
   */
  beforeunloadCallback_: null,

  /**
   * The unload callback is used to cancel the sync setup when the user hits
   * the browser back button after arriving on the page.
   * Note: Cases like closing the tab or reloading don't need to be handled,
   * because they are already caught in |PeopleHandler::~PeopleHandler|
   * from the C++ code.
   *
   * @private {?Function}
   */
  unloadCallback_: null,

  /**
   * Whether the initial layout for collapsible sections has been computed. It
   * is computed only once, the first time the sync status is updated.
   * @private {boolean}
   */
  collapsibleSectionsInitialized_: false,

  /**
   * Whether the user decided to abort sync.
   * @private {boolean}
   */
  didAbort_: true,

  /**
   * Whether the user confirmed the cancellation of sync.
   * @private {boolean}
   */
  setupCancelConfirmed_: false,

  /** @override */
  created() {
    this.browserProxy_ = settings.SyncBrowserProxyImpl.getInstance();
  },

  /** @override */
  attached() {
    this.addWebUIListener(
        'page-status-changed', this.handlePageStatusChanged_.bind(this));
    this.addWebUIListener(
        'sync-prefs-changed', this.handleSyncPrefsChanged_.bind(this));

    const router = settings.Router.getInstance();
    if (router.getCurrentRoute() === getSyncRoutes().SYNC) {
      this.onNavigateToPage_();
    }
  },

  /** @override */
  detached() {
    const router = settings.Router.getInstance();
    if (getSyncRoutes().SYNC.contains(router.getCurrentRoute())) {
      this.onNavigateAwayFromPage_();
    }

    if (this.beforeunloadCallback_) {
      window.removeEventListener('beforeunload', this.beforeunloadCallback_);
      this.beforeunloadCallback_ = null;
    }
    if (this.unloadCallback_) {
      window.removeEventListener('unload', this.unloadCallback_);
      this.unloadCallback_ = null;
    }
  },

  /**
   * Returns the encryption options SettingsSyncEncryptionOptionsElement.
   * @return {?SettingsSyncEncryptionOptionsElement}
   */
  getEncryptionOptions() {
    return /** @type {?SettingsSyncEncryptionOptionsElement} */ (
        this.$$('settings-sync-encryption-options'));
  },

  /**
   * Returns the encryption options SettingsPersonalizationOptionsElement.
   * @return {?SettingsPersonalizationOptionsElement}
   */
  getPersonalizationOptions() {
    return /** @type {?SettingsPersonalizationOptionsElement} */ (
        this.$$('settings-personalization-options'));
  },

  /**
   * @return {boolean}
   * @private
   */
  computeSignedIn_() {
    return !!this.syncStatus.signedIn;
  },

  /**
   * @return {boolean}
   * @private
   */
  computeSyncSectionDisabled_() {
    return this.syncStatus !== undefined &&
        (!this.syncStatus.signedIn || !!this.syncStatus.disabled ||
         (!!this.syncStatus.hasError &&
          this.syncStatus.statusAction !==
              settings.StatusAction.ENTER_PASSPHRASE &&
          this.syncStatus.statusAction !==
              settings.StatusAction.RETRIEVE_TRUSTED_VAULT_KEYS));
  },

  /**
   * @return {boolean}
   * @private
   */
  computeSyncDisabledByAdmin_() {
    return this.syncStatus !== undefined && !!this.syncStatus.managed;
  },

  /** @private */
  onFocusConfigChange_() {
    const router = settings.Router.getInstance();
    this.focusConfig.set(getSyncRoutes().SYNC_ADVANCED.path, () => {
      cr.ui.focusWithoutInk(assert(this.$$('#sync-advanced-row')));
    });
  },

  /** @private */
  onSetupCancelDialogBack_() {
    /** @type {!CrDialogElement} */ (this.$$('#setupCancelDialog')).cancel();
    chrome.metricsPrivate.recordUserAction(
        'Signin_Signin_CancelCancelAdvancedSyncSettings');
  },

  /** @private */
  onSetupCancelDialogConfirm_() {
    this.setupCancelConfirmed_ = true;
    /** @type {!CrDialogElement} */ (this.$$('#setupCancelDialog')).close();
    const router = settings.Router.getInstance();
    router.navigateTo(getSyncRoutes().BASIC);
    chrome.metricsPrivate.recordUserAction(
        'Signin_Signin_ConfirmCancelAdvancedSyncSettings');
  },

  /** @private */
  onSetupCancelDialogClose_() {
    this.showSetupCancelDialog_ = false;
  },

  /** @protected */
  currentRouteChanged() {
    const router = settings.Router.getInstance();
    if (router.getCurrentRoute() === getSyncRoutes().SYNC) {
      this.onNavigateToPage_();
      return;
    }

    if (getSyncRoutes().SYNC.contains(router.getCurrentRoute())) {
      return;
    }

    const searchParams =
        settings.Router.getInstance().getQueryParameters().get('search');
    if (searchParams) {
      // User navigated away via searching. Cancel sync without showing
      // confirmation dialog.
      this.onNavigateAwayFromPage_();
      return;
    }

    const userActionCancelsSetup = this.syncStatus &&
        this.syncStatus.firstSetupInProgress && this.didAbort_;
    if (userActionCancelsSetup && !this.setupCancelConfirmed_) {
      chrome.metricsPrivate.recordUserAction(
          'Signin_Signin_BackOnAdvancedSyncSettings');
      // Show the 'Cancel sync?' dialog.
      // Yield so that other |currentRouteChanged| observers are called,
      // before triggering another navigation (and another round of observers
      // firing). Triggering navigation from within an observer leads to some
      // undefined behavior and runtime errors.
      requestAnimationFrame(() => {
        router.navigateTo(getSyncRoutes().SYNC);
        this.showSetupCancelDialog_ = true;
        // Flush to make sure that the setup cancel dialog is attached.
        Polymer.dom.flush();
        this.$$('#setupCancelDialog').showModal();
      });
      return;
    }

    // Reset variable.
    this.setupCancelConfirmed_ = false;

    this.onNavigateAwayFromPage_();
  },

  /**
   * @param {!settings.PageStatus} expectedPageStatus
   * @return {boolean}
   * @private
   */
  isStatus_(expectedPageStatus) {
    return expectedPageStatus === this.pageStatus_;
  },

  /** @private */
  onNavigateToPage_() {
    const router = settings.Router.getInstance();
    assert(router.getCurrentRoute() === getSyncRoutes().SYNC);
    if (this.beforeunloadCallback_) {
      return;
    }

    this.collapsibleSectionsInitialized_ = false;

    // Display loading page until the settings have been retrieved.
    this.pageStatus_ = settings.PageStatus.SPINNER;

    this.browserProxy_.didNavigateToSyncPage();

    this.beforeunloadCallback_ = event => {
      // When the user tries to leave the sync setup, show the 'Leave site'
      // dialog.
      if (this.syncStatus && this.syncStatus.firstSetupInProgress) {
        event.preventDefault();
        event.returnValue = '';

        chrome.metricsPrivate.recordUserAction(
            'Signin_Signin_AbortAdvancedSyncSettings');
      }
    };
    window.addEventListener('beforeunload', this.beforeunloadCallback_);

    this.unloadCallback_ = this.onNavigateAwayFromPage_.bind(this);
    window.addEventListener('unload', this.unloadCallback_);
  },

  /** @private */
  onNavigateAwayFromPage_() {
    if (!this.beforeunloadCallback_) {
      return;
    }

    // Reset the status to CONFIGURE such that the searching algorithm can
    // search useful content when the page is not visible to the user.
    this.pageStatus_ = settings.PageStatus.CONFIGURE;

    this.browserProxy_.didNavigateAwayFromSyncPage(this.didAbort_);

    window.removeEventListener('beforeunload', this.beforeunloadCallback_);
    this.beforeunloadCallback_ = null;

    if (this.unloadCallback_) {
      window.removeEventListener('unload', this.unloadCallback_);
      this.unloadCallback_ = null;
    }
  },

  /**
   * Handler for when the sync preferences are updated.
   * @private
   */
  handleSyncPrefsChanged_(syncPrefs) {
    this.syncPrefs = syncPrefs;
    this.pageStatus_ = settings.PageStatus.CONFIGURE;

    // Hide the new passphrase box if (a) full data encryption is enabled,
    // (b) encrypting all data is not allowed (so far, only applies to
    // supervised accounts), or (c) the user is a supervised account.
    if (this.syncPrefs.encryptAllData ||
        !this.syncPrefs.encryptAllDataAllowed ||
        (this.syncStatus && this.syncStatus.supervisedUser)) {
      this.creatingNewPassphrase_ = false;
    }
  },

  /** @private */
  onActivityControlsClick_() {
    chrome.metricsPrivate.recordUserAction('Sync_OpenActivityControlsPage');
    this.browserProxy_.openActivityControlsUrl();
    window.open(loadTimeData.getString('activityControlsUrl'));
  },

  /** @private */
  onSyncDashboardLinkClick_() {
    window.open(loadTimeData.getString('syncDashboardUrl'));
  },

  /**
   * Whether the encryption dropdown should be expanded by default.
   * @private
   */
  expandEncryptionIfNeeded_() {
    // Force the dropdown to expand.
    if (this.forceEncryptionExpanded) {
      this.forceEncryptionExpanded = false;
      this.encryptionExpanded_ = true;
      return;
    }
    this.encryptionExpanded_ =
        !!this.syncPrefs && this.syncPrefs.encryptAllData;
  },

  /**
   * @param {!Event} event
   * @private
   */
  onResetSyncClick_(event) {
    if (event.target.tagName === 'A') {
      // Stop the propagation of events as the |cr-expand-button|
      // prevents the default which will prevent the navigation to the link.
      event.stopPropagation();
    }
  },

  /**
   * Sends the user-entered existing password to re-enable sync.
   * @private
   * @param {!Event} e
   */
  onSubmitExistingPassphraseTap_(e) {
    if (e.type === 'keypress' && e.key !== 'Enter') {
      return;
    }

    this.syncPrefs.setNewPassphrase = false;

    this.syncPrefs.passphrase = this.existingPassphrase_;
    this.existingPassphrase_ = '';

    this.browserProxy_.setSyncEncryption(this.syncPrefs)
        .then(this.handlePageStatusChanged_.bind(this));
  },

  /**
   * @private
   * @param {!CustomEvent<!settings.PageStatus>} e
   */
  onPassphraseChanged_(e) {
    this.handlePageStatusChanged_(
        /** @type {!settings.PageStatus} */ (e.detail));
  },

  /**
   * Called when the page status updates.
   * @param {!settings.PageStatus} pageStatus
   * @private
   */
  handlePageStatusChanged_(pageStatus) {
    const router = settings.Router.getInstance();
    switch (pageStatus) {
      case settings.PageStatus.SPINNER:
      case settings.PageStatus.CONFIGURE:
        this.pageStatus_ = pageStatus;
        return;
      case settings.PageStatus.DONE:
        if (router.getCurrentRoute() === getSyncRoutes().SYNC) {
          router.navigateTo(getSyncRoutes().PEOPLE);
        }
        return;
      case settings.PageStatus.PASSPHRASE_FAILED:
        if (this.pageStatus_ === this.pages_.CONFIGURE && this.syncPrefs &&
            this.syncPrefs.passphraseRequired) {
          const passphraseInput = /** @type {!CrInputElement} */ (
              this.$$('#existingPassphraseInput'));
          passphraseInput.invalid = true;
          passphraseInput.focusInput();
        }
        return;
    }

    assertNotReached();
  },

  /**
   * @param {!Event} event
   * @private
   */
  onLearnMoreTap_(event) {
    if (event.target.tagName === 'A') {
      // Stop the propagation of events, so that clicking on links inside
      // checkboxes or radio buttons won't change the value.
      event.stopPropagation();
    }
  },

  /**
   * @return {boolean}
   * @private
   */
  shouldShowSyncAccountControl_() {
    // <if expr="chromeos">
    if (!loadTimeData.getBoolean('useBrowserSyncConsent')) {
      return false;
    }
    // </if>
    return this.syncStatus !== undefined &&
        !!this.syncStatus.syncSystemEnabled &&
        loadTimeData.getBoolean('signinAllowed');
  },

  /**
   * @return {boolean}
   * @private
   */
  shouldShowExistingPassphraseBelowAccount_() {
    return this.syncPrefs !== undefined && !!this.syncPrefs.passphraseRequired;
  },

  /** @private */
  onSyncAdvancedClick_() {
    const router = settings.Router.getInstance();
    router.navigateTo(getSyncRoutes().SYNC_ADVANCED);
  },

  /**
   * @param {!CustomEvent<boolean>} e The event passed from
   *     settings-sync-account-control.
   * @private
   */
  onSyncSetupDone_(e) {
    if (e.detail) {
      this.didAbort_ = false;
      chrome.metricsPrivate.recordUserAction(
          'Signin_Signin_ConfirmAdvancedSyncSettings');
    } else {
      this.setupCancelConfirmed_ = true;
      chrome.metricsPrivate.recordUserAction(
          'Signin_Signin_CancelAdvancedSyncSettings');
    }
    const router = settings.Router.getInstance();
    router.navigateTo(getSyncRoutes().BASIC);
  },

  /**
   * Focuses the passphrase input element if it is available and the page is
   * visible.
   * @private
   */
  focusPassphraseInput_() {
    const passphraseInput =
        /** @type {!CrInputElement} */ (this.$$('#existingPassphraseInput'));
    const router = settings.Router.getInstance();
    if (passphraseInput && router.getCurrentRoute() === getSyncRoutes().SYNC) {
      passphraseInput.focus();
    }
  },
});
})();
