// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Maps signal strength from [0, 100] to [0, 4] which represents the number
 * of bars in the signal icon displayed to the user. This is used to select
 * the correct icon.
 * @param {number} strength The signal strength from [0 - 100].
 * @return {number} The number of signal bars from [0, 4] as an integer
 */
function signalStrengthToBarCount(strength) {
  if (strength > 75) {
    return 4;
  }
  if (strength > 50) {
    return 3;
  }
  if (strength > 25) {
    return 2;
  }
  if (strength > 0) {
    return 1;
  }
  return 0;
}

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

  open() {
    const dialog = this.getDialog_();
    if (!dialog.open) {
      this.getDialog_().showModal();
    }

    this.$.connectButton.focus();
  },

  close() {
    const dialog = this.getDialog_();
    if (dialog.open) {
      dialog.close();
    }
  },

  /**
   * @return {!CrDialogElement}
   * @private
   */
  getDialog_() {
    return /** @type {!CrDialogElement} */ (this.$.dialog);
  },

  /** @private */
  onNotNowTap_() {
    this.getDialog_().cancel();
  },

  /**
   * Fires the 'connect-tap' event.
   * @private
   */
  onConnectTap_() {
    this.fire('tether-connect');
  },

  /**
   * @param {!chromeos.networkConfig.mojom.ManagedProperties|undefined}
   *    managedProperties
   * @return {boolean}
   * @private
   */
  shouldShowDisconnectFromWifi_(managedProperties) {
    // TODO(khorimoto): Pipe through a new network property which describes
    // whether the tether host is currently connected to a Wi-Fi network. Return
    // whether it is here.
    return true;
  },

  /**
   * @param {!chromeos.networkConfig.mojom.ManagedProperties|undefined}
   *    managedProperties
   * @return {string} The battery percentage integer value converted to a
   *     string. Note that this will not return a string with a "%" suffix.
   * @private
   */
  getBatteryPercentageAsString_(managedProperties) {
    return managedProperties ?
        managedProperties.typeProperties.tether.batteryPercentage.toString() :
        '0';
  },

  /**
   * Retrieves an image that corresponds to signal strength of the tether host.
   * Custom icons are used here instead of a <network-icon> because this
   * dialog uses a special color scheme.
   * @param {!chromeos.networkConfig.mojom.ManagedProperties|undefined}
   *    managedProperties
   * @return {string} The name of the icon to be used to represent the network's
   *     signal strength.
   */
  getSignalStrengthIconName_(managedProperties) {
    const signalStrength = managedProperties ?
        managedProperties.typeProperties.tether.signalStrength :
        0;
    return 'os-settings:signal-cellular-' +
        signalStrengthToBarCount(signalStrength) + '-bar';
  },

  /**
   * Retrieves a localized accessibility label for the signal strength.
   * @param {!chromeos.networkConfig.mojom.ManagedProperties|undefined}
   *    managedProperties
   * @return {string} The localized signal strength label.
   */
  getSignalStrengthLabel_(managedProperties) {
    const signalStrength = managedProperties ?
        managedProperties.typeProperties.tether.signalStrength :
        0;
    const networkTypeString = this.i18n('OncTypeTether');
    return this.i18n(
        'networkIconLabelSignalStrength', networkTypeString, signalStrength);
  },

  /**
   * @param {!chromeos.networkConfig.mojom.ManagedProperties|undefined}
   *    managedProperties
   * @return {string}
   * @private
   */
  getDeviceName_(managedProperties) {
    return managedProperties ? OncMojo.getNetworkName(managedProperties) : '';
  },

  /**
   * @param {!chromeos.networkConfig.mojom.ManagedProperties|undefined}
   *    managedProperties
   * @return {string}
   * @private
   */
  getBatteryPercentageString_(managedProperties) {
    return managedProperties ?
        this.i18n(
            'tetherConnectionBatteryPercentage',
            this.getBatteryPercentageAsString_(managedProperties)) :
        '';
  },

  /**
   * @param {!chromeos.networkConfig.mojom.ManagedProperties|undefined}
   *    managedProperties
   * @return {string}
   * @private
   */
  getExplanation_(managedProperties) {
    return managedProperties ?
        this.i18n(
            'tetherConnectionExplanation',
            HTMLEscape(OncMojo.getNetworkName(managedProperties))) :
        '';
  },

  /**
   * @param {!chromeos.networkConfig.mojom.ManagedProperties|undefined}
   *    managedProperties
   * @return {string}
   * @private
   */
  getDescriptionTitle_(managedProperties) {
    return managedProperties ?
        this.i18n(
            'tetherConnectionDescriptionTitle',
            HTMLEscape(OncMojo.getNetworkName(managedProperties))) :
        '';
  },

  /**
   * @param {!chromeos.networkConfig.mojom.ManagedProperties|undefined}
   *    managedProperties
   * @return {string}
   * @private
   */
  getBatteryDescription_(managedProperties) {
    return managedProperties ?
        this.i18n(
            'tetherConnectionDescriptionBattery',
            this.getBatteryPercentageAsString_(managedProperties)) :
        '';
  },
});
