// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'nearby-share-contact-visibility-dialog' allows editing of the users contact
 * visibility settings.
 */

import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import '../../shared/nearby_onboarding_page.js';
import '../../shared/nearby_visibility_page.js';

import {loadTimeData} from 'chrome://resources/ash/common/load_time_data.m.js';
import {html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {NearbyContactVisibilityElement} from '../../shared/nearby_contact_visibility.js';
import {NearbySettings} from '../../shared/nearby_share_settings_behavior.js';

/** @polymer */
class NearbyShareContactVisibilityDialogElement extends PolymerElement {
  static get is() {
    return 'nearby-share-contact-visibility-dialog';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      /** @type {NearbySettings} */
      settings: {
        type: Object,
        value: {},
      },
    };
  }

  /** @private */
  onSaveClick_() {
    const contactVisibility = /** @type {NearbyContactVisibilityElement} */
        (this.$.contactVisibility);
    contactVisibility.saveVisibilityAndAllowedContacts();
    const dialog = /** @type {!CrDialogElement} */ (this.$.dialog);
    if (dialog.open) {
      dialog.close();
    }
  }

  /** @private */
  onManageContactsClick_() {
    window.open(loadTimeData.getString('nearbyShareManageContactsUrl'));
  }
}

customElements.define(
    NearbyShareContactVisibilityDialogElement.is,
    NearbyShareContactVisibilityDialogElement);
