// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'settings-signout-dialog' is a dialog that allows the
 * user to turn off sync and sign out of Chromium.
 */
Polymer({
  is: 'settings-signout-dialog',

  behaviors: [WebUIListenerBehavior],

  properties: {
    /**
     * The current sync status, supplied by the parent.
     * @type {?settings.SyncStatus}
     */
    syncStatus: {
      type: Object,
      observer: 'syncStatusChanged_',
    },

    /**
     * True if the checkbox to delete the profile has been checked.
     * @private
     */
    deleteProfile_: Boolean,

    /**
     * True if the profile deletion warning is visible.
     * @private
     */
    deleteProfileWarningVisible_: Boolean,

    /**
     * The profile deletion warning. The message indicates the number of
     * profile stats that will be deleted if a non-zero count for the profile
     * stats is returned from the browser.
     * @private
     */
    deleteProfileWarning_: String,
  },

  /** @override */
  attached: function() {
    this.addWebUIListener(
        'profile-stats-count-ready', this.handleProfileStatsCount_.bind(this));
    // <if expr="not chromeos">
    settings.ProfileInfoBrowserProxyImpl.getInstance().getProfileStatsCount();
    // </if>
    this.async(() => {
      this.$.dialog.showModal();
    });
  },

  /**
   * Returns true when the user selected 'Confirm'.
   * @return {boolean}
   */
  wasConfirmed: function() {
    return this.$.dialog.getNative().returnValue == 'success';
  },

  /**
   * Handler for when the profile stats count is pushed from the browser.
   * @param {number} count
   * @private
   */
  handleProfileStatsCount_: function(count) {
    const username = this.syncStatus.signedInUsername || '';
    if (count == 0) {
      this.deleteProfileWarning_ = loadTimeData.getStringF(
          'deleteProfileWarningWithoutCounts', username);
    } else if (count == 1) {
      this.deleteProfileWarning_ = loadTimeData.getStringF(
          'deleteProfileWarningWithCountsSingular', username);
    } else {
      this.deleteProfileWarning_ = loadTimeData.getStringF(
          'deleteProfileWarningWithCountsPlural', count, username);
    }
  },

  /**
   * Polymer observer for syncStatus.
   * @private
   */
  syncStatusChanged_: function() {
    if (!this.syncStatus.signedIn && this.$.dialog.open)
      this.$.dialog.close();
  },

  /**
   * @private
   * @param {string} domain
   * @return {string}
   */
  getDisconnectExplanationHtml_: function(domain) {
    // <if expr="not chromeos">
    if (domain) {
      return loadTimeData.getStringF(
          'syncDisconnectManagedProfileExplanation',
          '<span id="managed-by-domain-name">' + domain + '</span>');
    }
    // </if>
    return loadTimeData.getString('syncDisconnectExplanation');
  },

  /** @private */
  onDisconnectCancel_: function() {
    this.$.dialog.cancel();
  },

  /** @private */
  onDisconnectConfirm_: function() {
    this.$.dialog.close();
    const deleteProfile = !!this.syncStatus.domain || this.deleteProfile_;
    settings.SyncBrowserProxyImpl.getInstance().signOut(deleteProfile);
  },
});
