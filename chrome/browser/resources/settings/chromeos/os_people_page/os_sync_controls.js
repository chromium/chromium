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
  'osPreferencesSynced',
  'printersSynced',
];

/**
 * @fileoverview
 * 'os-sync-controls' contains all OS sync data type controls.
 */
Polymer({
  is: 'os-sync-controls',

  behaviors: [WebUIListenerBehavior],

  properties: {
    hidden: {
      type: Boolean,
      value: false,
      computed: 'syncControlsHidden_(' +
          'syncStatus.signedIn, syncStatus.disabled, syncStatus.hasError)',
      reflectToAttribute: true,
    },

    /**
     * The current OS sync preferences.
     * @type {settings.OsSyncPrefs|undefined}
     */
    osSyncPrefs: Object,

    /**
     * The current sync status, supplied by the parent.
     * @type {settings.SyncStatus}
     */
    syncStatus: Object,
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
  created: function() {
    this.browserProxy_ = settings.OsSyncBrowserProxyImpl.getInstance();
  },

  /** @override */
  attached: function() {
    this.addWebUIListener(
        'os-sync-prefs-changed', this.handleOsSyncPrefsChanged_.bind(this));

    // Request the initial SyncPrefs and start the sync engine if necessary.
    if (settings.getCurrentRoute() == settings.routes.OS_SYNC) {
      this.browserProxy_.didNavigateToOsSyncPage();
    }
  },

  /** @override */
  detached: function() {
    if (settings.routes.OS_SYNC.contains(settings.getCurrentRoute())) {
      this.browserProxy_.didNavigateAwayFromOsSyncPage();
    }
  },

  /**
   * Handler for when the sync preferences are updated.
   * @private
   */
  handleOsSyncPrefsChanged_: function(osSyncPrefs) {
    this.osSyncPrefs = osSyncPrefs;
  },

  /**
   * Handler for when the feature enabled checkbox is changed.
   * @param {!Event} event
   * @private
   */
  onFeatureEnabledChanged_: function(event) {
    this.set('osSyncPrefs.featureEnabled', !!event.target.checked);
    this.sendOsSyncDatatypes_();
  },

  /**
   * Handler for when the sync all data types checkbox is changed.
   * @param {!Event} event
   * @private
   */
  onSyncAllOsTypesChanged_: function(event) {
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
   * Sends the osSyncPrefs dictionary back to the C++ handler.
   * @private
   */
  sendOsSyncDatatypes_: function() {
    assert(this.osSyncPrefs);
    this.browserProxy_.setOsSyncDatatypes(this.osSyncPrefs);
  },

  /**
   * @param {boolean} syncAllOsTypes
   * @param {boolean} enforced
   * @return {boolean} Whether the sync checkbox should be disabled.
   */
  shouldSyncCheckboxBeDisabled_: function(syncAllOsTypes, enforced) {
    return syncAllOsTypes || enforced;
  },

  /**
   * @return {boolean} Whether the sync controls are hidden.
   * @private
   */
  syncControlsHidden_: function() {
    if (!this.syncStatus) {
      // Show sync controls by default.
      return false;
    }

    if (!this.syncStatus.signedIn || this.syncStatus.disabled) {
      return true;
    }

    // TODO(jamescook): Passphrase support.
    return !!this.syncStatus.hasError &&
        this.syncStatus.statusAction !== settings.StatusAction.ENTER_PASSPHRASE;
  },
});
})();
