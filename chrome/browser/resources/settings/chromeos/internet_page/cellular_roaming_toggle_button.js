// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'cellular-roaming-toggle-button' is responsible for encapsulating the
 * cellular roaming configuration logic, and in particular the details behind
 * the transition to a more granular approach to roaming configuration.
 */

Polymer({
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
