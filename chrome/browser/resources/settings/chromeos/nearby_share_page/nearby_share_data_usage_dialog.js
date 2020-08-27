// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'nearby-share-data-usage-dialog' allows editing of the data usage setting
 * when using Nearby Share.
 */
Polymer({
  is: 'nearby-share-data-usage-dialog',

  behaviors: [
    I18nBehavior,
    PrefsBehavior,
  ],

  properties: {
    /** Preferences state. */
    prefs: {
      type: Object,
      notify: true,
    },

    /** @type {!Object<string, number>} */
    NearbyShareDataUsage: {
      type: Object,
      value: NearbyShareDataUsage,
    },
  },

  /** @override */
  attached() {
    const dialog = /** @type {!CrDialogElement} */ (this.$.dialog);
    if (!dialog.open) {
      dialog.showModal();
    }
  },

  /** @private */
  close() {
    const dialog = /** @type {!CrDialogElement} */ (this.$.dialog);
    if (dialog.open) {
      dialog.close();
    }
  },

  /** @private */
  onCancelTap_() {
    this.close();
  },

  /** @private */
  onUpdateTap_() {
    this.setPrefValue(
        'nearby_sharing.data_usage',
        dataUsageStringToEnum(this.$$('cr-radio-group').selected));
    this.close();
  },

  /** @private */
  selectedDataUsage_(dataUsageValue) {
    if (dataUsageValue === NearbyShareDataUsage.UNKNOWN) {
      return NearbyShareDataUsage.WIFI_ONLY;
    }

    return dataUsageValue;
  },
});
