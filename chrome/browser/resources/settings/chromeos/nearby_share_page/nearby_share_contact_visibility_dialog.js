// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'nearby-share-contact-visibility-dialog' allows editing of the users contact
 * visibility settings.
 */

import '//resources/cr_elements/cr_button/cr_button.m.js';
import '//resources/cr_elements/cr_dialog/cr_dialog.m.js';
import '../../shared/nearby_contact_visibility.js';
import '../../shared/nearby_onboarding_page.js';
import '../../shared/nearby_visibility_page.js';

import {html, Polymer} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';

import {NearbySettings} from '../../shared/nearby_share_settings_behavior.js';

Polymer({
  _template: html`{__html_template__}`,
  is: 'nearby-share-contact-visibility-dialog',

  properties: {
    /** @type {NearbySettings} */
    settings: {
      type: Object,
      value: {},
    },
  },

  /** @private */
  onSaveClick_() {
    const contactVisibility = /** @type {NearbyContactVisibilityElement} */
        (this.$.contactVisibility);
    contactVisibility.saveVisibilityAndAllowedContacts();
    const dialog = /** @type {!CrDialogElement} */ (this.$.dialog);
    if (dialog.open) {
      dialog.close();
    }
  },

  /** @private */
  onManageContactsClick_() {
    window.open(loadTimeData.getString('nearbyShareManageContactsUrl'));
  }
});
