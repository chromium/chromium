// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'nearby-share-device-name-dialog' allows editing of the device display name
 * when using Nearby Share.
 */
Polymer({
  is: 'nearby-share-device-name-dialog',

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
  },

  attached() {
    this.open();
  },

  open() {
    const dialog = /** @type {!CrDialogElement} */ (this.$.dialog);
    if (!dialog.open) {
      dialog.showModal();
    }
  },

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
  onDoneTap_() {
    this.setPrefValue('nearby_sharing.device_name', this.$$('cr-input').value);
    this.close();
  },
});
