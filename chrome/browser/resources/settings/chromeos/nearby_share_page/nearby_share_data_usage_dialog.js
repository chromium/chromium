// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'nearby-share-data-usage-dialog' allows editing of the data usage setting
 * when using Nearby Share.
 */

import '//resources/cr_elements/cr_button/cr_button.m.js';
import '//resources/cr_elements/cr_dialog/cr_dialog.m.js';
import '//resources/cr_elements/cr_radio_button/cr_radio_button.m.js';
import '//resources/cr_elements/cr_radio_group/cr_radio_group.m.js';

import {I18nBehavior} from '//resources/js/i18n_behavior.m.js';
import {html, Polymer} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getNearbyShareSettings} from '../../shared/nearby_share_settings.js';
import {NearbySettings} from '../../shared/nearby_share_settings_behavior.js';

import {dataUsageStringToEnum, NearbyShareDataUsage} from './types.js';

Polymer({
  _template: html`{__html_template__}`,
  is: 'nearby-share-data-usage-dialog',

  behaviors: [
    I18nBehavior,
  ],

  properties: {
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
  onCancelClick_() {
    this.close();
  },

  /** @private */
  onSaveClick_() {
    getNearbyShareSettings().setDataUsage(
        (dataUsageStringToEnum(this.$$('cr-radio-group').selected)));
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
