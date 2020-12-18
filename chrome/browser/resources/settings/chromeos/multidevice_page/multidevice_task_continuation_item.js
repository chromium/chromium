// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'settings-multidevice-task-continuation-item' encapsulates
 * special logic for the phonehub task continuation item used in the multidevice
 * subpage.
 *
 * Task continuation depends on the 'Open Tabs' Chrome sync type being
 * activated. This component uses sync proxies from the people page to check
 * whether chrome sync is enabled.
 *
 * If it is enabled the multidevice feature item is used in the standard way,
 * otherwise the feature-controller and localized-link slots are overridden with
 * a disabled toggle and the task continuation localized string component that
 * is a special case containing two links.
 */
Polymer({
  is: 'settings-multidevice-task-continuation-item',

  behaviors: [
    MultiDeviceFeatureBehavior,
    WebUIListenerBehavior,
  ],

  properties: {
    /** @private */
    isChromeTabsSyncEnabled_: {
      type: Boolean,
      value: false,
    },
  },

  /** @private {?settings.SyncBrowserProxy} */
  syncBrowserProxy_: null,

  /** @override */
  created() {
    this.syncBrowserProxy_ = settings.SyncBrowserProxyImpl.getInstance();
  },

  /** @override */
  attached() {
    this.addWebUIListener(
        'sync-prefs-changed', this.handleSyncPrefsChanged_.bind(this));

    // Cause handleSyncPrefsChanged_() to be called when the element is first
    // attached so that the state of |syncPrefs.tabsSynced| is known.
    this.syncBrowserProxy_.sendSyncPrefsChanged();
  },

  /** @override */
  focus() {
    if (!this.isChromeTabsSyncEnabled_) {
      this.$$('cr-toggle').focus();
    } else {
      this.$.phoneHubTaskContinuationItem.focus();
    }
  },

  /**
   * Handler for when the sync preferences are updated.
   * @param {!settings.SyncPrefs} syncPrefs
   * @private
   */
  handleSyncPrefsChanged_(syncPrefs) {
    this.isChromeTabsSyncEnabled_ = !!syncPrefs && syncPrefs.tabsSynced;
  },
});
