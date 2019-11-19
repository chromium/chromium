// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(function() {
'use strict';

const mojom = chromeos.networkConfig.mojom;

Polymer({
  is: 'tether-connection-dialog',

  behaviors: [I18nBehavior],

  properties: {
    /** @private {!chromeos.networkConfig.mojom.ManagedProperties|undefined} */
    managedProperties: Object,

    /**
     * Whether the network has been lost (e.g., has gone out of range).
     * @type {boolean}
     */
    outOfRange: Boolean,
  },

  open: function() {
    const dialog = this.getDialog_();
    if (!dialog.open) {
      this.getDialog_().showModal();
    }

    this.$.connectButton.focus();
  },

  close: function() {
    const dialog = this.getDialog_();
    if (dialog.open) {
      dialog.close();
    }
  },

  /**
   * @return {!CrDialogElement}
   * @private
   */
  getDialog_: function() {
    return /** @type {!CrDialogElement} */ (this.$.dialog);
  },

  /** @private */
  onNotNowTap_: function() {
    this.getDialog_().cancel();
  },

  /**
   * Fires the 'connect-tap' event.
   * @private
   */
  onConnectTap_: function() {
    this.fire('tether-connect');
  },

  /**
   * @param {!mojom.ManagedProperties} managedProperties
   * @return {boolean}
   * @private
   */
  shouldShowDisconnectFromWifi_: function(managedProperties) {
    // TODO(khorimoto): Pipe through a new network property which describes
    // whether the tether host is currently connected to a Wi-Fi network. Return
    // whether it is here.
    return true;
  },

  /**
   * @param {!mojom.ManagedProperties} managedProperties
   * @return {string} The battery percentage integer value converted to a
   *     string. Note that this will not return a string with a "%" suffix.
   * @private
   */
  getBatteryPercentageAsString_: function(managedProperties) {
    return managedProperties.typeProperties.tether.batteryPercentage.toString();
  },

  /**
   * Retrieves an image that corresponds to signal strength of the tether host.
   * Custom icons are used here instead of a <network-icon> because this
   * dialog uses a special color scheme.
   * @param {!mojom.ManagedProperties} managedProperties
   * @return {string} The name of the icon to be used to represent the network's
   *     signal strength.
   */
  getSignalStrengthIconName_: function(managedProperties) {
    const signalStrength =
        managedProperties.typeProperties.tether.signalStrength;
    return 'os-settings:signal-cellular-' +
        Math.min(4, Math.max(signalStrength, 0)) + '-bar';
  },

  /**
   * @param {!mojom.ManagedProperties} managedProperties
   * @return {string}
   * @private
   */
  getDeviceName_: function(managedProperties) {
    return managedProperties ? OncMojo.getNetworkName(managedProperties) : '';
  },

  /**
   * @param {!mojom.ManagedProperties} managedProperties
   * @return {string}
   * @private
   */
  getBatteryPercentageString_: function(managedProperties) {
    return this.i18n(
        'tetherConnectionBatteryPercentage',
        this.getBatteryPercentageAsString_(managedProperties));
  },

  /**
   * @param {!mojom.ManagedProperties} managedProperties
   * @return {string}
   * @private
   */
  getExplanation_: function(managedProperties) {
    return this.i18n(
        'tetherConnectionExplanation',
        HTMLEscape(OncMojo.getNetworkName(managedProperties)));
  },

  /**
   * @param {!mojom.ManagedProperties} managedProperties
   * @return {string}
   * @private
   */
  getDescriptionTitle_: function(managedProperties) {
    return this.i18n(
        'tetherConnectionDescriptionTitle',
        HTMLEscape(OncMojo.getNetworkName(managedProperties)));
  },

  /**
   * @param {!mojom.ManagedProperties} managedProperties
   * @return {string}
   * @private
   */
  getBatteryDescription_: function(managedProperties) {
    return this.i18n(
        'tetherConnectionDescriptionBattery',
        this.getBatteryPercentageAsString_(managedProperties));
  },
});
})();
