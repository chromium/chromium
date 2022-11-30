// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Polymer element for displaying a list of cellular
 * APNs
 */

import 'chrome://resources/cr_components/localized_link/localized_link.js';
import './network_shared.css.js';
import 'chrome://resources/polymer/v3_0/iron-list/iron-list.js';
import 'chrome://resources/ash/common/network/apn_list_item.js';

import {I18nBehavior} from '//resources/ash/common/i18n_behavior.js';
import {Polymer} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {OncMojo} from 'chrome://resources/ash/common/network/onc_mojo.js';
import {ApnProperties, ApnState, ManagedCellularProperties} from 'chrome://resources/mojo/chromeos/services/network_config/public/mojom/cros_network_config.mojom-webui.js';

import {getTemplate} from './apn_list.html.js';

Polymer({
  _template: getTemplate(),
  is: 'apn-list',

  behaviors: [I18nBehavior],

  properties: {
    /**@type {!ManagedCellularProperties}*/
    managedCellularProperties: {
      type: Object,
    },

    shouldOmitLinks: {
      type: Boolean,
      value: false,
    },

    /** @private */
    isConnectedApnAutoDetected_: {
      type: Boolean,
      value: false,
    },
  },

  /**
   * Returns an array with all the APN properties that need to be displayed.
   * TODO(b/162365553): Implement logic for customApnList.
   * TODO(b/162365553): Handle managedCellularProperties.apnList.policyValue
   * when policies are included.
   * @return {Array<!ApnProperties>}
   * @private
   */
  getApns_() {
    if (!this.managedCellularProperties) {
      return [];
    }

    const connectedApn = this.managedCellularProperties.connectedApn;
    this.isConnectedApnAutoDetected_ = false;
    if (connectedApn) {
      // TODO(b/162365553) Check whether connectedApn is a custom APN or not
      // when assigning this property.
      this.isConnectedApnAutoDetected_ = true;
      return [connectedApn];
    }
    // TODO(b/162365553): Handle the case when there is no connected APN.
    return [];
  },

  /**
   * Returns true if the APN on this index is connected.
   * @param {number} index index in the APNs array.
   * @return {boolean}
   * @private
   */
  isApnConnected_(index) {
    return !!this.managedCellularProperties &&
        !!this.managedCellularProperties.connectedApn && index === 0;
  },

  /**
   * Returns true if the APN is automatically detected.
   * @param {number} index index in the APNs array.
   * @return {boolean}
   * @private
   */
  isApnAutoDetected_(index) {
    return this.isApnConnected_(index) && this.isConnectedApnAutoDetected_;
  },

  /**
   * Redirects to "Lean more about APN" page.
   * TODO(b/162365553): Implement.
   * @private
   */
  onLearnMoreClicked_() {},
});