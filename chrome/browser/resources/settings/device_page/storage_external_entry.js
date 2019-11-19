// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'storage-external-entry' is the polymer element for showing a certain
 * external storage device with a toggle switch. When the switch is ON,
 * the storage's uuid will be saved to a preference.
 */
Polymer({
  is: 'storage-external-entry',

  behaviors: [WebUIListenerBehavior, PrefsBehavior],

  properties: {
    /**
     * FileSystem UUID of an external storage.
     */
    uuid: String,

    /**
     * Label of an external storage.
     */
    label: String,

    /** @private {chrome.settingsPrivate.PrefObject} */
    visiblePref_: {
      type: Object,
      value: function() {
        return /** @type {chrome.settingsPrivate.PrefObject} */ ({});
      },
    },
  },

  observers: [
    'updateVisible_(prefs.arc.visible_external_storages.*)',
  ],

  /**
   * Handler for when the toggle button for this entry is clicked by a user.
   * @param {!Event} event
   * @private
   */
  onVisibleChange_: function(event) {
    const visible = !!event.target.checked;
    if (visible) {
      this.appendPrefListItem('arc.visible_external_storages', this.uuid);
    } else {
      this.deletePrefListItem('arc.visible_external_storages', this.uuid);
    }
    chrome.metricsPrivate.recordBoolean(
        'Arc.ExternalStorage.SetVisible', visible);
  },

  /**
   * Updates |visiblePref_| by reading the preference and check if it contains
   * UUID of this storage.
   * @private
   */
  updateVisible_: function() {
    const uuids = /** @type {!Array<string>} */ (
        this.getPref('arc.visible_external_storages').value);
    const visible = uuids.some((id) => id === this.uuid);
    const pref = {
      key: '',
      type: chrome.settingsPrivate.PrefType.BOOLEAN,
      value: visible,
    };
    this.visiblePref_ = pref;
  },
});