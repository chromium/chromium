// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(function() {

/**
 * Names of the individual data type properties to be cached from
 * settings.OsSyncPrefs when the user checks 'Sync All'.
 * @type {!Array<string>}
 */
const SyncPrefsIndividualDataTypes = [
  'osAppsSynced',
  'osPreferencesSynced',
  'osWifiConfigurationsSynced',

  // Note: Wallpaper uses a different naming scheme because it's stored as its
  // own separate pref instead of through the sync service.
  'wallpaperEnabled',
];

/**
 * @fileoverview
 * 'os-sync-controls' contains all OS sync data type controls.
 */
Polymer({
  is: 'os-sync-controls',

  behaviors: [
    DeepLinkingBehavior,
    I18nBehavior,
    settings.RouteObserverBehavior,
    WebUIListenerBehavior,
  ],

  properties: {
    hidden: {
      type: Boolean,
      value: true,
      computed: 'syncControlsHidden_(osSyncPrefs)',
      reflectToAttribute: true,
    },

    /**
     * Injected sync system status. Undefined until the parent component injects
     * the value.
     * @type {settings.SyncStatus|undefined}
     */
    syncStatus: Object,

    /**
     * Injected profile icon URL, usually a data:image/png URL.
     * @private
     */
    profileIconUrl: String,

    /**
     * Injected profile name, e.g. "John Cena".
     * @private
     */
    profileName: String,

    /**
     * Injected profile email address, e.g. "john.cena@gmail.com".
     * @private
     */
    profileEmail: String,

    /**
     * Whether the OS sync feature is enabled. This object does not directly
     * manipulate prefs so we can defer turning on OS sync until the user
     * navigates away from the page.
     */
    osSyncFeatureEnabled: Boolean,

    /**
     * The current OS sync preferences. Cached so we can restore individual
     * toggle state when turning "sync everything" on and off, without affecting
     * the underlying chrome prefs.
     * @type {settings.OsSyncPrefs|undefined}
     */
    osSyncPrefs: Object,

    /** @private */
    areDataTypeTogglesDisabled_: {
      type: Boolean,
      value: true,
      computed: `computeDataTypeTogglesDisabled_(osSyncFeatureEnabled,
          osSyncPrefs.syncAllOsTypes)`,
    },

    /**
     * Used by DeepLinkingBehavior to focus this page's deep links.
     * @type {!Set<!chromeos.settings.mojom.Setting>}
     */
    supportedSettingIds: {
      type: Object,
      value: () => new Set([chromeos.settings.mojom.Setting.kSplitSyncOnOff]),
    },
  },

  /** @private {?settings.OsSyncBrowserProxy} */
  browserProxy_: null,

  /**
   * Caches the individually selected synced data types. This is used to
   * be able to restore the selections after checking and unchecking Sync All.
   * @private {?Object}
   */
  cachedOsSyncPrefs_: null,

  /** @override */
  created() {
    this.browserProxy_ = settings.OsSyncBrowserProxyImpl.getInstance();
  },

  /** @override */
  attached() {
    this.addWebUIListener(
        'os-sync-prefs-changed', this.handleOsSyncPrefsChanged_.bind(this));
  },

  /**
   * settings.RouteObserverBehavior
   * @param {!settings.Route|undefined} newRoute
   * @param {!settings.Route|undefined} oldRoute
   * @protected
   */
  currentRouteChanged(newRoute, oldRoute) {
    if (newRoute === settings.routes.OS_SYNC) {
      this.browserProxy_.didNavigateToOsSyncPage();
      this.attemptDeepLink();
    }
    if (oldRoute === settings.routes.OS_SYNC) {
      this.browserProxy_.didNavigateAwayFromOsSyncPage();
    }
  },

  /**
   * @return {string} The top label for the account row.
   * @private
   */
  getAccountTitle_() {
    if (!this.syncStatus) {
      return '';
    }
    return this.syncStatus.hasError ? this.i18n('syncNotWorking') :
                                      this.profileName;
  },

  /**
   * @return {string} The bottom label for the account row.
   * @private
   */
  getAccountSubtitle_() {
    if (!this.syncStatus) {
      return '';
    }
    return this.osSyncFeatureEnabled && !this.syncStatus.hasError ?
        this.i18n('syncingTo', this.profileEmail) :
        this.profileEmail;
  },

  /**
   * @return {string}
   * @private
   */
  getSyncOnOffButtonLabel_() {
    if (!this.osSyncFeatureEnabled) {
      return this.i18n('osSyncTurnOn');
    }
    return this.i18n('osSyncTurnOff');
  },

  /**
   * Returns the CSS class for the sync status icon.
   * @return {string}
   * @private
   */
  getSyncIconStyle_() {
    if (!this.syncStatus) {
      return 'sync';
    }
    if (this.syncStatus.disabled) {
      return 'sync-disabled';
    }
    if (!this.syncStatus.hasError) {
      return 'sync';
    }
    // Specific error cases below.
    if (this.syncStatus.hasUnrecoverableError) {
      return 'sync-problem';
    }
    if (this.syncStatus.statusAction === settings.StatusAction.REAUTHENTICATE) {
      return 'sync-paused';
    }
    return 'sync-problem';
  },

  /**
   * Returns the image to use for the sync status icon. The value must match
   * one of iron-icon's settings:(*) icon names.
   * @return {string}
   * @private
   */
  getSyncIcon_() {
    switch (this.getSyncIconStyle_()) {
      case 'sync-problem':
        return 'settings:sync-problem';
      case 'sync-paused':
        return 'settings:sync-disabled';
      default:
        return 'cr:sync';
    }
  },

  /**
   * Handler for when the sync preferences are updated.
   * @private
   */
  handleOsSyncPrefsChanged_(osSyncFeatureEnabled, osSyncPrefs) {
    this.osSyncFeatureEnabled = osSyncFeatureEnabled;
    this.osSyncPrefs = osSyncPrefs;

    // If the feature is disabled the checkboxes appear toggled off, regardless
    // of the underlying chrome pref.
    if (!this.osSyncFeatureEnabled) {
      this.set('osSyncPrefs.syncAllOsTypes', false);
      for (const dataType of SyncPrefsIndividualDataTypes) {
        this.set(['osSyncPrefs', dataType], false);
      }
    }

    // If apps are not registered or synced, force wallpaper off.
    if (!this.osSyncPrefs.osAppsRegistered || !this.osSyncPrefs.osAppsSynced) {
      this.set('osSyncPrefs.wallpaperEnabled', false);
    }
  },

  /** @private */
  onSyncOnOffButtonClick_() {
    this.browserProxy_.setOsSyncFeatureEnabled(!this.osSyncFeatureEnabled);
    settings.recordSettingChange();
  },

  /**
   * Handler for when the sync all data types checkbox is changed.
   * @param {!Event} event
   * @private
   */
  onSyncAllOsTypesChanged_(event) {
    if (event.target.checked) {
      this.set('osSyncPrefs.syncAllOsTypes', true);

      // Cache the previously selected preference before checking every box.
      this.cachedOsSyncPrefs_ = {};
      for (const dataType of SyncPrefsIndividualDataTypes) {
        // These are all booleans, so this shallow copy is sufficient.
        this.cachedOsSyncPrefs_[dataType] = this.osSyncPrefs[dataType];

        this.set(['osSyncPrefs', dataType], true);
      }
    } else if (this.cachedOsSyncPrefs_) {
      // Restore the previously selected preference.
      for (const dataType of SyncPrefsIndividualDataTypes) {
        this.set(['osSyncPrefs', dataType], this.cachedOsSyncPrefs_[dataType]);
      }
    }

    this.sendOsSyncDatatypes_();
  },

  /**
   * Handler for when any sync data type checkbox is changed.
   * @private
   */
  onSingleSyncDataTypeChanged_() {
    this.sendOsSyncDatatypes_();
  },

  /**
   * Handler for changes to the apps sync state; apps have a special handler
   * instead of relying on onSingleSyncDataTypeChanged_() because wallpaper has
   * a dependency on apps.
   * @private
   */
  onAppsSyncedChanged_() {
    this.set('osSyncPrefs.wallpaperEnabled', this.osSyncPrefs.osAppsSynced);

    this.onSingleSyncDataTypeChanged_();
  },

  /**
   * Sends the osSyncPrefs dictionary back to the C++ handler.
   * @private
   */
  sendOsSyncDatatypes_() {
    assert(this.osSyncPrefs);
    this.browserProxy_.setOsSyncDatatypes(this.osSyncPrefs);
  },

  /**
   * @return {boolean} Whether the sync data type toggles should be disabled.
   * @private
   */
  computeDataTypeTogglesDisabled_() {
    return !this.osSyncFeatureEnabled ||
        (this.osSyncPrefs !== undefined && this.osSyncPrefs.syncAllOsTypes);
  },

  /**
   * @return {boolean} Whether the sync controls are hidden.
   * @private
   */
  syncControlsHidden_() {
    // Hide everything until the initial prefs are received from C++,
    // otherwise there is a visible layout reshuffle on first load.
    return !this.osSyncPrefs;
  },

  /**
   * @return {boolean} Whether the wallpaper checkbox and label should be
   *     disabled.
   * @private
   */
  shouldWallpaperSyncSectionBeDisabled_() {
    return this.areDataTypeTogglesDisabled_ || !this.osSyncPrefs ||
        !this.osSyncPrefs.osAppsSynced;
  },
});
})();
