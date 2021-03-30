// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'settings-storage-external' is the settings subpage for external storage
 * settings.
 */

Polymer({
  is: 'settings-storage-external',

  behaviors: [
    I18nBehavior,
    WebUIListenerBehavior,
  ],

  properties: {
    /**
     * List of the plugged-in external storages.
     * @private {Array<!settings.ExternalStorage>}
     */
    externalStorages_: {
      type: Array,
      value() {
        return [];
      }
    },

    /** @private {!chrome.settingsPrivate.PrefObject} */
    externalStorageVisiblePref_: {
      type: Object,
      value() {
        return /** @type {!chrome.settingsPrivate.PrefObject} */ ({});
      },
    },
  },

  /** @private {?settings.DevicePageBrowserProxy} */
  browserProxy_: null,

  /** @override */
  created() {
    this.browserProxy_ = settings.DevicePageBrowserProxyImpl.getInstance();
  },

  /** @override */
  attached() {
    this.browserProxy_.setExternalStoragesUpdatedCallback(
        this.handleExternalStoragesUpdated_.bind(this));
    this.browserProxy_.updateExternalStorages();
  },

  /**
   * @param {Array<!settings.ExternalStorage>} storages
   * @private
   */
  handleExternalStoragesUpdated_(storages) {
    this.externalStorages_ = storages;
  },

  /**
   * @param {Array<!settings.ExternalStorage>} externalStorages
   * @return {string}
   * @private
   */
  computeStorageListHeader_(externalStorages) {
    return this.i18n(
        !externalStorages || externalStorages.length === 0 ?
            'storageExternalStorageEmptyListHeader' :
            'storageExternalStorageListHeader');
  },
});
