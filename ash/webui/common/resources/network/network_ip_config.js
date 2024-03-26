// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Polymer element for displaying the IP Config properties for
 * a network state.
 */

import '//resources/ash/common/cr_elements/cr_toggle/cr_toggle.js';
import '//resources/ash/common/cr_elements/policy/cr_policy_indicator.js';
import './network_property_list_mojo.js';
import './network_shared.css.js';

import {I18nBehavior} from '//resources/ash/common/i18n_behavior.js';
import {IPConfigProperties, ManagedProperties, NO_ROUTING_PREFIX} from '//resources/mojo/chromeos/services/network_config/public/mojom/cros_network_config.mojom-webui.js';
import {IPConfigType, NetworkType} from '//resources/mojo/chromeos/services/network_config/public/mojom/network_types.mojom-webui.js';
import {Polymer} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {CrPolicyNetworkBehaviorMojo} from './cr_policy_network_behavior_mojo.js';
import {getTemplate} from './network_ip_config.html.js';
import {OncMojo} from './onc_mojo.js';

/**
 * Returns the routing prefix as a string for a given prefix length. If
 * |prefixLength| is invalid, returns undefined.
 * @param {number} prefixLength The ONC routing prefix length.
 * @return {string|undefined}
 */
const getRoutingPrefixAsNetmask = function(prefixLength) {
  'use strict';
  // Return the empty string for invalid inputs.
  if (prefixLength <= 0 || prefixLength > 32) {
    return undefined;
  }
  let netmask = '';
  for (let i = 0; i < 4; ++i) {
    let remainder = 8;
    if (prefixLength >= 8) {
      prefixLength -= 8;
    } else {
      remainder = prefixLength;
      prefixLength = 0;
    }
    if (i > 0) {
      netmask += '.';
    }
    let value = 0;
    if (remainder !== 0) {
      value = ((2 << (remainder - 1)) - 1) << (8 - remainder);
    }
    netmask += value.toString();
  }
  return netmask;
};

/**
 * Returns the routing prefix length as a number from the netmask string.
 * @param {string|undefined} netmask The netmask string, e.g. 255.255.255.0.
 * @return {number} The corresponding netmask or NO_ROUTING_PREFIX if invalid.
 */
const getRoutingPrefixAsLength = function(netmask) {
  'use strict';
  if (!netmask) {
    return NO_ROUTING_PREFIX;
  }
  const tokens = netmask.split('.');
  if (tokens.length !== 4) {
    return NO_ROUTING_PREFIX;
  }
  let prefixLength = 0;
  for (let i = 0; i < tokens.length; ++i) {
    const token = tokens[i];
    // If we already found the last mask and the current one is not
    // '0' then the netmask is invalid. For example, 255.224.255.0
    if (prefixLength / 8 !== i) {
      if (token !== '0') {
        return NO_ROUTING_PREFIX;
      }
    } else if (token === '255') {
      prefixLength += 8;
    } else if (token === '254') {
      prefixLength += 7;
    } else if (token === '252') {
      prefixLength += 6;
    } else if (token === '248') {
      prefixLength += 5;
    } else if (token === '240') {
      prefixLength += 4;
    } else if (token === '224') {
      prefixLength += 3;
    } else if (token === '192') {
      prefixLength += 2;
    } else if (token === '128') {
      prefixLength += 1;
    } else if (token === '0') {
      prefixLength += 0;
    } else {
      // mask is not a valid number.
      return NO_ROUTING_PREFIX;
    }
  }
  return prefixLength;
};

Polymer({
  _template: getTemplate(),
  is: 'network-ip-config',

  behaviors: [I18nBehavior, CrPolicyNetworkBehaviorMojo],

  properties: {
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
     * State of 'Configure IP Addresses Automatically'.
     * @private
     */
    automatic_: {
      type: Boolean,
      value: true,
    },

    /**
     * The currently visible IP Config property dictionary.
     * @private {{
     *   ipv4: (OncMojo.IPConfigUIProperties|undefined),
     *   ipv6: (OncMojo.IPConfigUIProperties|undefined)
     * }|undefined}
     */
    ipConfig_: Object,

    /**
     * Array of properties to pass to the property list.
     * @private {!Array<string>}
     */
    ipConfigFields_: {
      type: Array,
      value() {
        return [
          'ipv4.ipAddress',
          'ipv4.netmask',
          'ipv4.gateway',
          'ipv6.ipAddress',
        ];
      },
      readOnly: true,
    },

    /**
     * True if automatically-configured IP address toggle should be visible.
     * @private
     */
    shouldShowAutoIpConfigToggle_: {
      type: Boolean,
      value: true,
      computed: 'computeShouldShowAutoIpConfigToggle_(managedProperties)',
    },
  },

  /**
   * Returns the automatically configure IP CrToggleElement.
   * @return {?CrToggleElement}
   */
  getAutoConfigIpToggle() {
    return /** @type {?CrToggleElement} */ (this.$$('#autoConfigIpToggle'));
  },

  /**
   * Saved static IP configuration properties when switching to 'automatic'.
   * @private {!OncMojo.IPConfigUIProperties|undefined}
   */
  savedStaticIp_: undefined,

  /** @private */
  managedPropertiesChanged_(newValue, oldValue) {
    if (!this.managedProperties) {
      return;
    }

    const properties = this.managedProperties;
    if (newValue.guid !== (oldValue && oldValue.guid)) {
      this.savedStaticIp_ = undefined;
    }

    // Update the 'automatic' property.
    const ipConfigType = OncMojo.getActiveValue(properties.ipAddressConfigType);
    this.automatic_ = ipConfigType !== 'Static';

    if (properties.ipConfigs || properties.staticIpConfig) {
      if (this.automatic_ || !oldValue ||
          newValue.guid !== (oldValue && oldValue.guid) ||
          !OncMojo.connectionStateIsConnected(properties.connectionState)) {
        // Update the 'ipConfig' property.
        const ipv4 = this.getIPConfigUIProperties_(
            OncMojo.getIPConfigForType(properties, IPConfigType.kIPv4));
        let ipv6 = this.getIPConfigUIProperties_(
            OncMojo.getIPConfigForType(properties, IPConfigType.kIPv6));

        // If connected and the IP address is automatic and set, show message if
        // the ipv6 address is not set.
        if (OncMojo.connectionStateIsConnected(properties.connectionState) &&
            this.automatic_ && ipv4 && ipv4.ipAddress) {
          ipv6 = ipv6 || {type: IPConfigType.kIPv6};
          ipv6.ipAddress = ipv6.ipAddress || this.i18n('ipAddressNotAvailable');
        }
        this.ipConfig_ = {ipv4: ipv4, ipv6: ipv6};
      }
    } else {
      this.ipConfig_ = undefined;
    }
  },

  /**
   * Checks whether IP address config type can be changed.
   * @param {?ManagedProperties} managedProperties
   * @return {boolean}
   * @private
   */
  canChangeIPConfigType_(managedProperties) {
    if (this.disabled || !managedProperties) {
      return false;
    }
    if (managedProperties.type === NetworkType.kCellular) {
      // Cellular IP config properties can not be changed.
      return false;
    }
    const ipConfigType = managedProperties.ipAddressConfigType;
    return !ipConfigType || !this.isNetworkPolicyEnforced(ipConfigType);
  },

  /**
   * Overrides null values of this.ipConfig_.ipv4 with defaults so that
   * this.ipConfig_.ipv4 passes validation after being converted to ONC
   * StaticIPConfig.
   * TODO(https://crbug.com/1148841): Setting defaults here is strange, find
   * some better way.
   * @private
   */
  setIpv4Defaults_() {
    if (!this.ipConfig_ || !this.ipConfig_.ipv4) {
      return;
    }
    if (!this.ipConfig_.ipv4.gateway) {
      this.set('ipConfig_.ipv4.gateway', '192.168.1.1');
    }
    if (!this.ipConfig_.ipv4.ipAddress) {
      this.set('ipConfig_.ipv4.ipAddress', '192.168.1.1');
    }
    if (!this.ipConfig_.ipv4.netmask) {
      this.set('ipConfig_.ipv4.netmask', '255.255.255.0');
    }
  },

  /** @private */
  onAutomaticChange_() {
    if (!this.automatic_) {
      if (!this.ipConfig_) {
        this.ipConfig_ = {};
      }
      if (this.savedStaticIp_) {
        this.ipConfig_.ipv4 = this.savedStaticIp_;
      }
      if (!this.ipConfig_.ipv4) {
        this.ipConfig_.ipv4 = {
          type: IPConfigType.kIPv4,
        };
      }
      this.setIpv4Defaults_();
      this.sendStaticIpConfig_();
      return;
    }

    // Save the static IP configuration when switching to automatic.
    if (this.ipConfig_) {
      this.savedStaticIp_ = this.ipConfig_.ipv4;
    }
    this.fire('ip-change', {
      field: 'ipAddressConfigType',
      value: 'DHCP',
    });
  },

  /**
   * @param {!IPConfigProperties|undefined}
   *     ipconfig
   * @return {!OncMojo.IPConfigUIProperties|undefined} A new
   *     IPConfigUIProperties object with routingPrefix expressed as a netmask
   *     string instead of a prefix length. Returns undefined if |ipconfig|
   *     is not defined.
   * @private
   */
  getIPConfigUIProperties_(ipconfig) {
    if (!ipconfig) {
      return undefined;
    }

    // Copy |ipconfig| properties into |ipconfigUI|.
    const ipconfigUI = {};
    ipconfigUI.gateway = ipconfig.gateway;
    ipconfigUI.ipAddress = ipconfig.ipAddress;
    ipconfigUI.nameServers = ipconfig.nameServers;
    ipconfigUI.type = ipconfig.type;
    ipconfigUI.webProxyAutoDiscoveryUrl = ipconfig.webProxyAutoDiscoveryUrl;

    if (ipconfig.routingPrefix !== NO_ROUTING_PREFIX) {
      ipconfigUI.netmask = getRoutingPrefixAsNetmask(ipconfig.routingPrefix);
    }

    return ipconfigUI;
  },

  /**
   * @param {!OncMojo.IPConfigUIProperties} ipconfigUI
   * @return {!IPConfigProperties} A new
   *     IPConfigProperties object with netmask expressed as a a prefix
   *     length.
   * @private
   */
  getIPConfigProperties_(ipconfigUI) {
    const ipconfig = {};
    ipconfig.gateway = ipconfigUI.gateway;
    ipconfig.ipAddress = ipconfigUI.ipAddress;
    ipconfig.nameServers = ipconfigUI.nameServers;
    ipconfig.routingPrefix = getRoutingPrefixAsLength(ipconfigUI.netmask);
    ipconfig.type = ipconfigUI.type;
    ipconfig.webProxyAutoDiscoveryUrl = ipconfigUI.webProxyAutoDiscoveryUrl;

    return ipconfig;
  },

  /**
   * @return {boolean}
   * @private
   */
  hasIpConfigFields_() {
    if (!this.ipConfig_) {
      return false;
    }
    for (let i = 0; i < this.ipConfigFields_.length; ++i) {
      const key = this.ipConfigFields_[i];
      const value = this.get(key, this.ipConfig_);
      if (value !== undefined && value !== '') {
        return true;
      }
    }
    return false;
  },

  /**
   * @param {?OncMojo.ManagedProperty|undefined} property
   * @return {string|undefined} Edit type to be used in network-property-list.
   * @private
   */
  getIPFieldEditType_(property) {
    return this.isNetworkPolicyEnforced(property) ? undefined : 'String';
  },

  /**
   * @return {Object} An object with the edit type for each editable field.
   * @private
   */
  getIPEditFields_() {
    const staticIpConfig =
        this.managedProperties && this.managedProperties.staticIpConfig;
    if (this.automatic_ || !staticIpConfig) {
      return {};
    }
    return {
      'ipv4.ipAddress': this.getIPFieldEditType_(staticIpConfig.ipAddress),
      // Use routingPrefix instead of netmask because getIPFieldEditType_
      // expects a ManagedProperty and routingPrefix has the same type as
      // netmask.
      'ipv4.netmask': this.getIPFieldEditType_(staticIpConfig.routingPrefix),
      'ipv4.gateway': this.getIPFieldEditType_(staticIpConfig.gateway),
    };
  },

  /**
   * Event triggered when the network property list changes.
   * @param {!CustomEvent<!{field: string, value: string}>} event The
   *     network-property-list change event.
   * @private
   */
  onIPChange_(event) {
    if (!this.ipConfig_) {
      return;
    }
    const field = event.detail.field;
    const value = event.detail.value;
    // Note: |field| includes the 'ipv4.' prefix.
    this.set('ipConfig_.' + field, value);
    this.sendStaticIpConfig_();
  },

  /** @private */
  sendStaticIpConfig_() {
    // This will also set IPAddressConfigType to STATIC.
    this.fire('ip-change', {
      field: 'staticIpConfig',
      value: this.ipConfig_.ipv4 ?
          this.getIPConfigProperties_(this.ipConfig_.ipv4) :
          {},
    });
  },

  /**
   * @return {boolean}
   * @private
   */
  computeShouldShowAutoIpConfigToggle_() {
    if (this.managedProperties.type === NetworkType.kCellular) {
      return false;
    }
    return true;
  },

  /**
   * @return {string}
   * @private
   */
  getFieldsClassList_() {
    let classes = 'property-box single-column stretch';
    if (this.shouldShowAutoIpConfigToggle_) {
      classes += ' indented';
    }
    return classes;
  },
});
