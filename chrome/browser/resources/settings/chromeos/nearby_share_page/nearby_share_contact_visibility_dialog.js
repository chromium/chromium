// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'nearby-share-contact-visibility-dialog' allows editing of the users contact
 * visibility settings.
 */
Polymer({
  is: 'nearby-share-contact-visibility-dialog',

  properties: {
    /** @type {nearby_share.NearbySettings} */
    settings: {
      type: Object,
      value: {},
    },
  },

  /** @private */
  onDoneClick_() {
    const contactVisibility = /** @type {NearbyContactVisibilityElement} */
        (this.$.contactVisibility);
    contactVisibility.saveVisibilityAndAllowedContacts();
    const dialog = /** @type {!CrDialogElement} */ (this.$.dialog);
    if (dialog.open) {
      dialog.close();
    }
  },

  /** @private */
  onManageContactsTap_() {
    window.open(loadTimeData.getString('nearbyShareManageContactsUrl'));
  }
});
