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
     * @private {Arrray<!settings.ExternalStorage>}
     */
    externalStorages_: {
      type: Array,
      value: function() {
        return [];
      }
    },

    /** @private {!chrome.settingsPrivate.PrefObject} */
    externalStorageVisiblePref_: {
      type: Object,
      value: function() {
        return /** @type {!chrome.settingsPrivate.PrefObject} */ ({});
      },
    },
  },

  /** @private {?settings.DevicePageBrowserProxy} */
  browserProxy_: null,

  /** @override */
  created: function() {
    this.browserProxy_ = settings.DevicePageBrowserProxyImpl.getInstance();
  },

  /** @override */
  attached: function() {
    this.browserProxy_.setExternalStoragesUpdatedCallback(
        this.handleExternalStoragesUpdated_.bind(this));
    this.browserProxy_.updateExternalStorages();
  },

  /**
   * @param {Array<!settings.ExternalStorage>} storages
   * @private
   */
  handleExternalStoragesUpdated_: function(storages) {
    this.externalStorages_ = storages;
  },

  /**
   * @param {Arrray<!settings.ExternalStorage>} externalStorages
   * @return {string}
   * @private
   */
  computeStorageListHeader_: function(externalStorages) {
    return this.i18n(
        !externalStorages || externalStorages.length == 0 ?
            'storageExternalStorageEmptyListHeader' :
            'storageExternalStorageListHeader');
  },
});
