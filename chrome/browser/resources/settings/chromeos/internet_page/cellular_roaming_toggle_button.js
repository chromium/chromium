// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'cellular-roaming-toggle-button' is responsible for encapsulating the
 * cellular roaming configuration logic, and in particular the details behind
 * the transition to a more granular approach to roaming configuration.
 */

import '../../prefs/prefs.js';

import {OncMojo} from '//resources/cr_components/chromeos/network/onc_mojo.m.js';
import {I18nBehavior} from '//resources/js/i18n_behavior.m.js';
import {loadTimeData} from '//resources/js/load_time_data.m.js';
import {afterNextRender, flush, html, Polymer, TemplateInstanceBase, Templatizer} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {SettingsToggleButtonElement} from '../../controls/settings_toggle_button.js';

Polymer({
  _template: html`{__html_template__}`,
  is: 'cellular-roaming-toggle-button',

  behaviors: [
    I18nBehavior,
  ],

  properties: {
    disabled: {
      type: Boolean,
      value: false,
    },

    /** @type {!chromeos.networkConfig.mojom.ManagedProperties|undefined} */
    managedProperties: Object,

    prefs: {
      type: Object,
      notify: true,
    },

    /**
     * Whether or not the per-network cellular roaming configuration feature
     * flag is enabled.
     * @private
     */
    allowPerNetworkRoaming_: {
      type: Boolean,
      value() {
        return loadTimeData.getBoolean('allowPerNetworkRoaming');
      }
    },
  },

  /**
   * Returns the child element responsible for controlling cellular roaming.
   * @return {?SettingsToggleButtonElement}
   */
  getCellularRoamingToggle() {
    return /** @type {?SettingsToggleButtonElement} */ (
        this.$$('#cellularRoamingToggle'));
  },

  /**
   * @return {string} The text to display with roaming details.
   * @private
   */
  getRoamingDetails_() {
    if (!this.managedProperties.typeProperties.cellular.allowRoaming) {
      return this.i18n('networkAllowDataRoamingDisabled');
    }
    return this.managedProperties.typeProperties.cellular.roamingState ===
            'Roaming' ?
        this.i18n('networkAllowDataRoamingEnabledRoaming') :
        this.i18n('networkAllowDataRoamingEnabledHome');
  },
});
