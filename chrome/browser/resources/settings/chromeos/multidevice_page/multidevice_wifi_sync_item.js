// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'settings-multidevice-wifi-sync-item' encapsulates special
 * logic for the wifi sync item used in the multidevice subpage.
 *
 * Wifi sync depends on Chrome Sync being activated. This component uses sync
 * proxies from the people page to check whether chrome sync is enabled.
 *
 * If it is enabled the multidevice feature item is used in the standard way,
 * otherwise the feature-controller and localized-link slots are overridden with
 * a disabled toggle and the wifi sync localized string component that is a
 * special case containing two links.
 */
Polymer({
  is: 'settings-multidevice-wifi-sync-item',

  behaviors: [
    MultiDeviceFeatureBehavior,
    WebUIListenerBehavior,
  ],

  properties: {
    /** @private */
    isWifiSyncV1Enabled_: Boolean,
  },

  /** @private {?settings.OsSyncBrowserProxy} */
  osSyncBrowserProxy_: null,

  /** @private {?settings.SyncBrowserProxy} */
  syncBrowserProxy_: null,

  /** @override */
  attached() {
    if (loadTimeData.getBoolean('splitSettingsSyncEnabled')) {
      this.addWebUIListener(
          'os-sync-prefs-changed', this.handleOsSyncPrefsChanged_.bind(this));
      this.osSyncBrowserProxy_.sendOsSyncPrefsChanged();
    } else {
      this.addWebUIListener(
          'sync-prefs-changed', this.handleSyncPrefsChanged_.bind(this));
      this.syncBrowserProxy_.sendSyncPrefsChanged();
    }
  },

  /** @override */
  created() {
    if (loadTimeData.getBoolean('splitSettingsSyncEnabled')) {
      this.osSyncBrowserProxy_ = settings.OsSyncBrowserProxyImpl.getInstance();
    } else {
      this.syncBrowserProxy_ = settings.SyncBrowserProxyImpl.getInstance();
    }
  },

  /**
   * Handler for when the sync preferences are updated.
   * @param {!settings.SyncPrefs} syncPrefs
   * @private
   */
  handleSyncPrefsChanged_(syncPrefs) {
    this.isWifiSyncV1Enabled_ =
        !!syncPrefs && syncPrefs.wifiConfigurationsSynced;
  },

  /**
   * Handler for when os sync preferences are updated.
   * @param {!settings.OsSyncPrefs} osSyncPrefs
   * @param {!boolean} osSyncFeatureEnabled
   * @private
   */
  handleOsSyncPrefsChanged_(osSyncFeatureEnabled, osSyncPrefs) {
    this.isWifiSyncV1Enabled_ = osSyncFeatureEnabled && !!osSyncPrefs &&
        osSyncPrefs.osWifiConfigurationsSynced;
  },

  /** @override */
  focus() {
    this.$$('settings-multidevice-feature-item').focus();
  },
});
