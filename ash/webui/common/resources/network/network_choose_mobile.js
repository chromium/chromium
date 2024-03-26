// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Polymer element for displaying and modifying a list of cellular
 * mobile networks.
 */

import '//resources/ash/common/cr_elements/cr_button/cr_button.js';
import '//resources/ash/common/cr_elements/md_select.css.js';
import '//resources/ash/common/cr_elements/cr_shared_style.css.js';
import '//resources/polymer/v3_0/iron-flex-layout/iron-flex-layout-classes.js';
import './network_shared.css.js';

import {I18nBehavior} from '//resources/ash/common/i18n_behavior.js';
import {CrosNetworkConfigInterface, FoundNetworkProperties, ManagedProperties} from '//resources/mojo/chromeos/services/network_config/public/mojom/cros_network_config.mojom-webui.js';
import {ConnectionStateType, NetworkType} from '//resources/mojo/chromeos/services/network_config/public/mojom/network_types.mojom-webui.js';
import {Polymer} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {MojoInterfaceProvider, MojoInterfaceProviderImpl} from './mojo_interface_provider.js';
import {getTemplate} from './network_choose_mobile.html.js';
import {OncMojo} from './onc_mojo.js';

Polymer({
  _template: getTemplate(),
  is: 'network-choose-mobile',

  behaviors: [I18nBehavior],

  properties: {
    /** @type {?OncMojo.DeviceStateProperties} */
    deviceState: {
      type: Object,
      value: null,
    },

    disabled: {
      type: Boolean,
      value: false,
    },

    /** @type {!ManagedProperties|undefined} */
    managedProperties: {
      type: Object,
      observer: 'managedPropertiesChanged_',
    },

    /**
     * The mojom.FoundNetworkProperties.networkId of the selected mobile
     * network.
     * @private
     */
    selectedMobileNetworkId_: {
      type: String,
      value: '',
    },

    /**
     * Selectable list of mojom.FoundNetworkProperties dictionaries for the UI.
     * @private {!Array<!FoundNetworkProperties>}
     */
    mobileNetworkList_: {
      type: Array,
      value() {
        return [];
      },
    },
  },

  /** @private {boolean} */
  scanRequested_: false,

  /** @private {?CrosNetworkConfigInterface} */
  networkConfig_: null,

  /** @override */
  attached() {
    this.scanRequested_ = false;
  },

  /**
   * @return {?CrosNetworkConfigInterface}
   * @private
   */
  getNetworkConfig_() {
    if (!this.networkConfig_) {
      this.networkConfig_ =
          MojoInterfaceProviderImpl.getInstance().getMojoServiceRemote();
    }
    return this.networkConfig_;
  },

  /**
   * Polymer managedProperties changed method.
   * @private
   */
  managedPropertiesChanged_() {
    const cellular = this.managedProperties.typeProperties.cellular;
    this.mobileNetworkList_ = cellular.foundNetworks || [];
    if (!this.mobileNetworkList_.length) {
      this.mobileNetworkList_ = [
        {networkId: 'none', longName: this.i18n('networkCellularNoNetworks')},
      ];
    }
    // Set selectedMobileNetworkId_ after the dom-repeat has been stamped.
    this.async(() => {
      let selected = this.mobileNetworkList_.find(function(mobileNetwork) {
        return mobileNetwork.status === 'current';
      });
      if (!selected) {
        selected = this.mobileNetworkList_[0];
      }
      this.selectedMobileNetworkId_ = selected.networkId;
    });
  },

  /**
   * @param {!FoundNetworkProperties} foundNetwork
   * @return {boolean}
   * @private
   */
  getMobileNetworkIsDisabled_(foundNetwork) {
    return foundNetwork.status !== 'available' &&
        foundNetwork.status !== 'current';
  },

  /**
   * @param {!ManagedProperties} properties
   * @return {boolean}
   * @private
   */
  getEnableScanButton_(properties) {
    return !this.disabled &&
        properties.connectionState === ConnectionStateType.kNotConnected &&
        !!this.deviceState && !this.deviceState.scanning;
  },

  /**
   * @param {!ManagedProperties} properties
   * @return {boolean}
   * @private
   */
  getEnableSelectNetwork_(properties) {
    return (
        !this.disabled && !!this.deviceState && !this.deviceState.scanning &&
        properties.connectionState === ConnectionStateType.kNotConnected &&
        !!properties.typeProperties.cellular.foundNetworks &&
        properties.typeProperties.cellular.foundNetworks.length > 0);
  },

  /**
   * @param {!ManagedProperties} properties
   * @return {string}
   * @private
   */
  getSecondaryText_(properties) {
    if (!properties) {
      return '';
    }
    if (!!this.deviceState && this.deviceState.scanning) {
      return this.i18n('networkCellularScanning');
    }
    if (this.scanRequested_) {
      return this.i18n('networkCellularScanCompleted');
    }
    if (properties.connectionState !== ConnectionStateType.kNotConnected) {
      return this.i18n('networkCellularScanConnectedHelp');
    }
    return '';
  },

  /**
   * @param {!FoundNetworkProperties} foundNetwork
   * @return {string}
   * @private
   */
  getName_(foundNetwork) {
    return foundNetwork.longName || foundNetwork.shortName ||
        foundNetwork.networkId;
  },

  /**
   * Request a Cellular scan to populate the list of networks. This will
   * triger a change to managedProperties when completed (if
   * Cellular.FoundNetworks changes).
   * @private
   */
  onScanTap_() {
    this.scanRequested_ = true;

    this.getNetworkConfig_().requestNetworkScan(NetworkType.kCellular);
  },

  /**
   * @param {!Event} event
   * @private
   */
  onChange_(event) {
    const target = /** @type {!HTMLSelectElement} */ (event.target);
    if (!target.value || target.value === 'none') {
      return;
    }

    this.getNetworkConfig_().selectCellularMobileNetwork(
        this.managedProperties.guid, target.value);
    this.fire('user-action-setting-change');
  },
});
