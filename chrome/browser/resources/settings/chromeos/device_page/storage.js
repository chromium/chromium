// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('settings', function() {
  /**
   * @typedef {{
   *   availableSize: string,
   *   usedSize: string,
   *   usedRatio: number,
   *   spaceState: settings.StorageSpaceState,
   * }}
   */
  let StorageSizeStat;

  Polymer({
    is: 'settings-storage',

    behaviors: [
      settings.RouteObserverBehavior,
      settings.RouteOriginBehavior,
      WebUIListenerBehavior,
    ],

    properties: {
      androidEnabled: Boolean,

      /** @private */
      showCrostiniStorage_: {
        type: Boolean,
        value: false,
      },

      /** @private */
      showCrostini: Boolean,

      /** @private */
      isGuest_: {
        type: Boolean,
        value() {
          return loadTimeData.getBoolean('isGuest');
        }
      },

      /** @private */
      showOtherUsers_: {
        type: Boolean,
        // Initialize showOtherUsers_ to false if the user is in guest mode.
        value() {
          return !loadTimeData.getBoolean('isGuest');
        }
      },

      /** @private {settings.StorageSizeStat} */
      sizeStat_: Object,
    },

    /** settings.RouteOriginBehavior override */
    route_: settings.routes.STORAGE,

    observers: ['handleCrostiniEnabledChanged_(prefs.crostini.enabled.value)'],

    /**
     * Timer ID for periodic update.
     * @private {number}
     */
    updateTimerId_: -1,

    /** @private {?settings.DevicePageBrowserProxy} */
    browserProxy_: null,

    /** @override */
    attached() {
      this.addWebUIListener(
          'storage-size-stat-changed', this.handleSizeStatChanged_.bind(this));
      this.addWebUIListener(
          'storage-my-files-size-changed',
          this.handleMyFilesSizeChanged_.bind(this));
      this.addWebUIListener(
          'storage-browsing-data-size-changed',
          this.handleBrowsingDataSizeChanged_.bind(this));
      this.addWebUIListener(
          'storage-apps-size-changed', this.handleAppsSizeChanged_.bind(this));
      this.addWebUIListener(
          'storage-crostini-size-changed',
          this.handleCrostiniSizeChanged_.bind(this));
      if (!this.isGuest_) {
        this.addWebUIListener(
            'storage-other-users-size-changed',
            this.handleOtherUsersSizeChanged_.bind(this));
        this.addWebUIListener(
            'storage-system-size-changed',
            this.handleSystemSizeChanged_.bind(this));
      }
    },

    ready() {
      const r = settings.routes;
      this.addFocusConfig_(r.CROSTINI_DETAILS, '#crostiniSize');
      this.addFocusConfig_(r.ACCOUNTS, '#otherUsersSize');
      this.addFocusConfig_(
          r.EXTERNAL_STORAGE_PREFERENCES, '#externalStoragePreferences');
      this.browserProxy_ = settings.DevicePageBrowserProxyImpl.getInstance();
    },

    /**
     * settings.RouteObserverBehavior
     * @param {!settings.Route} newRoute
     * @param {!settings.Route} oldRoute
     * @protected
     */
    currentRouteChanged(newRoute, oldRoute) {
      settings.RouteOriginBehaviorImpl.currentRouteChanged.call(
          this, newRoute, oldRoute);

      if (settings.Router.getInstance().getCurrentRoute() !==
          settings.routes.STORAGE) {
        return;
      }
      this.onPageShown_();
    },

    /** @private */
    onPageShown_() {
      // Updating storage information can be expensive (e.g. computing directory
      // sizes recursively), so we delay this operation until the page is shown.
      this.browserProxy_.updateStorageInfo();
      // We update the storage usage periodically when the overlay is visible.
      this.startPeriodicUpdate_();
    },

    /**
     * Handler for tapping the "My files" item.
     * @private
     */
    onMyFilesTap_() {
      this.browserProxy_.openMyFiles();
    },

    /**
     * Handler for tapping the "Browsing data" item.
     * @private
     */
    onBrowsingDataTap_() {
      window.open('chrome://settings/clearBrowserData');
    },

    /**
     * Handler for tapping the "Apps and Extensions" item.
     * @private
     */
    onAppsTap_() {
      window.location = 'chrome://os-settings/app-management';
    },

    /**
     * Handler for tapping the "Linux storage" item.
     * @private
     */
    onCrostiniTap_() {
      settings.Router.getInstance().navigateTo(
          settings.routes.CROSTINI_DETAILS, /* dynamicParams */ null,
          /* removeSearch */ true);
    },

    /**
     * Handler for tapping the "Other users" item.
     * @private
     */
    onOtherUsersTap_() {
      settings.Router.getInstance().navigateTo(
          settings.routes.ACCOUNTS,
          /* dynamicParams */ null, /* removeSearch */ true);
    },

    /**
     * Handler for tapping the "External storage preferences" item.
     * @private
     */
    onExternalStoragePreferencesTap_() {
      settings.Router.getInstance().navigateTo(
          settings.routes.EXTERNAL_STORAGE_PREFERENCES);
    },

    /**
     * @param {!settings.StorageSizeStat} sizeStat
     * @private
     */
    handleSizeStatChanged_(sizeStat) {
      this.sizeStat_ = sizeStat;
      this.$.inUseLabelArea.style.width = (sizeStat.usedRatio * 100) + '%';
      this.$.availableLabelArea.style.width =
          ((1 - sizeStat.usedRatio) * 100) + '%';
    },

    /**
     * @param {string} size Formatted string representing the size of My files.
     * @private
     */
    handleMyFilesSizeChanged_(size) {
      this.$.myFilesSize.subLabel = size;
    },

    /**
     * @param {string} size Formatted string representing the size of Browsing
     *     data.
     * @private
     */
    handleBrowsingDataSizeChanged_(size) {
      this.$.browsingDataSize.subLabel = size;
    },

    /**
     * @param {string} size Formatted string representing the size of Apps and
     *     extensions storage.
     * @private
     */
    handleAppsSizeChanged_(size) {
      this.$$('#appsSize').subLabel = size;
    },

    /**
     * @param {string} size Formatted string representing the size of Crostini
     *     storage.
     * @private
     */
    handleCrostiniSizeChanged_(size) {
      if (this.showCrostiniStorage_) {
        this.$$('#crostiniSize').subLabel = size;
      }
    },

    /**
     * @param {string} size Formatted string representing the size of Other
     *     users.
     * @param {boolean} noOtherUsers True if there is no other registered users
     *     on the device.
     * @private
     */
    handleOtherUsersSizeChanged_(size, noOtherUsers) {
      if (this.isGuest_ || noOtherUsers) {
        this.showOtherUsers_ = false;
        return;
      }
      this.showOtherUsers_ = true;
      this.$$('#otherUsersSize').subLabel = size;
    },

    /**
     * @param {string} size Formatted string representing the System size.
     * @private
     */
    handleSystemSizeChanged_(size) {
      this.$$('#systemSizeSubLabel').innerText = size;
    },

    /**
     * @param {boolean} enabled True if Crostini is enabled.
     * @private
     */
    handleCrostiniEnabledChanged_(enabled) {
      this.showCrostiniStorage_ = enabled && this.showCrostini;
    },

    /**
     * Starts periodic update for storage usage.
     * @private
     */
    startPeriodicUpdate_() {
      // We update the storage usage every 5 seconds.
      if (this.updateTimerId_ === -1) {
        this.updateTimerId_ = window.setInterval(() => {
          if (settings.Router.getInstance().getCurrentRoute() !==
              settings.routes.STORAGE) {
            this.stopPeriodicUpdate_();
            return;
          }
          this.browserProxy_.updateStorageInfo();
        }, 5000);
      }
    },

    /**
     * Stops periodic update for storage usage.
     * @private
     */
    stopPeriodicUpdate_() {
      if (this.updateTimerId_ !== -1) {
        window.clearInterval(this.updateTimerId_);
        this.updateTimerId_ = -1;
      }
    },

    /**
     * Returns true if the remaining space is low, but not critically low.
     * @param {!settings.StorageSpaceState} spaceState Status about the
     *     remaining space.
     * @private
     */
    isSpaceLow_(spaceState) {
      return spaceState === settings.StorageSpaceState.LOW;
    },

    /**
     * Returns true if the remaining space is critically low.
     * @param {!settings.StorageSpaceState} spaceState Status about the
     *     remaining space.
     * @private
     */
    isSpaceCriticallyLow_(spaceState) {
      return spaceState === settings.StorageSpaceState.CRITICALLY_LOW;
    },

    /**
     * Computes class name of the bar based on the remaining space size.
     * @param {!settings.StorageSpaceState} spaceState Status about the
     *     remaining space.
     * @private
     */
    getBarClass_(spaceState) {
      switch (spaceState) {
        case settings.StorageSpaceState.LOW:
          return 'space-low';
        case settings.StorageSpaceState.CRITICALLY_LOW:
          return 'space-critically-low';
        default:
          return '';
      }
    },
  });

  // #cr_define_end
  return {StorageSizeStat};
});
