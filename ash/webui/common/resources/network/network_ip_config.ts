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

import type {CrToggleElement} from '//resources/ash/common/cr_elements/cr_toggle/cr_toggle.js';
import type {IPConfigProperties, ManagedProperties} from '//resources/mojo/chromeos/services/network_config/public/mojom/cros_network_config.mojom-webui.js';
import {NO_ROUTING_PREFIX} from '//resources/mojo/chromeos/services/network_config/public/mojom/cros_network_config.mojom-webui.js';
import {IPConfigType, NetworkType} from '//resources/mojo/chromeos/services/network_config/public/mojom/network_types.mojom-webui.js';
import {mixinBehaviors, PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import type {I18nMixinInterface} from 'chrome://resources/ash/common/cr_elements/i18n_mixin.js';
import {I18nMixin} from 'chrome://resources/ash/common/cr_elements/i18n_mixin.js';

import type {CrPolicyNetworkBehaviorMojoInterface} from './cr_policy_network_behavior_mojo.js';
import {CrPolicyNetworkBehaviorMojo} from './cr_policy_network_behavior_mojo.js';
import {getTemplate} from './network_ip_config.html.js';
import {OncMojo} from './onc_mojo.js';

/**
 * Returns the routing prefix as a string for a given prefix length. If
 * |prefixLength| is invalid, returns undefined.
 * @param prefixLength The ONC routing prefix length.
 */
const getRoutingPrefixAsNetmask = function(prefixLength: number): string|null {
  'use strict';
  // Return the empty string for invalid inputs.
  if (prefixLength <= 0 || prefixLength > 32) {
    return null;
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
 * @param netmask The netmask string, e.g. 255.255.255.0.
 */
const getRoutingPrefixAsLength = function(netmask: string|null): number {
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

const NetworkIpConfigElementBase =
    mixinBehaviors([CrPolicyNetworkBehaviorMojo], I18nMixin(PolymerElement)) as
    {
      new (): PolymerElement & I18nMixinInterface &
          CrPolicyNetworkBehaviorMojoInterface,
    };

export class NetworkIpConfigElement extends NetworkIpConfigElementBase {
  static get is() {
    return 'network-ip-config' as const;
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      disabled: {
        type: Boolean,
        value: false,
      },

      managedProperties: {
        type: Object,
        observer: 'managedPropertiesChanged_',
      },

      /**
       * State of 'Configure IP Addresses Automatically'.
       */
      automatic_: {
        type: Boolean,
        value: true,
      },

      /**
       * The currently visible IP Config property dictionary.
       */
      ipConfig_: Object,

      /**
       * Array of properties to pass to the property list.
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
       */
      shouldShowAutoIpConfigToggle_: {
        type: Boolean,
        value: true,
        computed: 'computeShouldShowAutoIpConfigToggle_(managedProperties)',
      },
    };
  }

  disabled: boolean;
  managedProperties: ManagedProperties|undefined;
  private automatic_: boolean;
  private ipConfig_: ({
    ipv4: (OncMojo.IPConfigUIProperties | undefined),
    ipv6: (OncMojo.IPConfigUIProperties|undefined),
  }|undefined);
  private ipConfigFields_: string[];
  private shouldShowAutoIpConfigToggle_: boolean;
  private savedStaticIp_: OncMojo.IPConfigUIProperties|undefined;

  constructor() {
    super();

    /**
     * Saved static IP configuration properties when switching to 'automatic'.
     */
    this.savedStaticIp_ = undefined;
  }

  /**
   * Returns the automatically configure IP CrToggleElement.
   */
  getAutoConfigIpToggle(): CrToggleElement|null {
    return this.shadowRoot!.querySelector('#autoConfigIpToggle');
  }

  private managedPropertiesChanged_(
      newValue: ManagedProperties|undefined,
      oldValue: ManagedProperties|undefined): void {
    if (!this.managedProperties) {
      return;
    }

    const properties = this.managedProperties;
    if (newValue && newValue.guid !== (oldValue && oldValue.guid)) {
      this.savedStaticIp_ = undefined;
    }

    // Update the 'automatic' property.
    const ipConfigType = OncMojo.getActiveValue(properties.ipAddressConfigType);
    this.automatic_ = ipConfigType !== 'Static';

    if (properties.ipConfigs || properties.staticIpConfig) {
      if (this.automatic_ || !oldValue ||
          (newValue && newValue.guid !== (oldValue && oldValue.guid)) ||
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
          ipv6 = ipv6 || {
            type: IPConfigType.kIPv6,
            gateway: null,
            ipAddress: null,
            nameServers: null,
            netmask: null,
            webProxyAutoDiscoveryUrl: null,
          };
          if (ipv6) {
            ipv6.ipAddress =
                ipv6.ipAddress || this.i18n('ipAddressNotAvailable');
          }
        }
        this.ipConfig_ = {ipv4: ipv4, ipv6: ipv6};
      }
    } else {
      this.ipConfig_ = undefined;
    }
  }

  /**
   * Checks whether IP address config type can be changed.
   */
  private canChangeIPConfigType_(managedProperties: ManagedProperties|
                                 null): boolean {
    if (this.disabled || !managedProperties) {
      return false;
    }
    if (managedProperties.type === NetworkType.kCellular) {
      // Cellular IP config properties can not be changed.
      return false;
    }
    const ipConfigType = managedProperties.ipAddressConfigType;
    return !ipConfigType || !this.isNetworkPolicyEnforced(ipConfigType);
  }

  /**
   * Overrides null values of this.ipConfig_.ipv4 with defaults so that
   * this.ipConfig_.ipv4 passes validation after being converted to ONC
   * StaticIPConfig.
   * TODO(https://crbug.com/1148841): Setting defaults here is strange, find
   * some better way.
   */
  private setIpv4Defaults_(): void {
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
  }

  private onAutomaticChange_(): void {
    if (!this.automatic_) {
      if (!this.ipConfig_) {
        this.ipConfig_ = {ipv4: undefined, ipv6: undefined};
      }
      if (this.savedStaticIp_) {
        this.ipConfig_.ipv4 = this.savedStaticIp_;
      }
      if (!this.ipConfig_.ipv4) {
        this.ipConfig_.ipv4 = {
          type: IPConfigType.kIPv4,
          gateway: null,
          ipAddress: null,
          nameServers: null,
          netmask: null,
          webProxyAutoDiscoveryUrl: null,
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
    this.dispatchEvent(new CustomEvent('ip-change', {
      bubbles: true,
      composed: true,
      detail: {field: 'ipAddressConfigType', value: 'DHCP'},
    }));
  }

  /**
   * Returns a new IPConfigUIProperties object with routingPrefix expressed as a
   * netmask string instead of a prefix length. Returns undefined if
   * |ipconfig| is not defined.
   *
   */
  private getIPConfigUIProperties_(ipconfig: IPConfigProperties|undefined):
      OncMojo.IPConfigUIProperties|undefined {
    if (!ipconfig) {
      return undefined;
    }

    // Copy |ipconfig| properties into |ipconfigUI|.
    const ipconfigUI: OncMojo.IPConfigUIProperties = {
      gateway: ipconfig.gateway,
      ipAddress: ipconfig.ipAddress,
      nameServers: ipconfig.nameServers,
      type: ipconfig.type,
      webProxyAutoDiscoveryUrl: ipconfig.webProxyAutoDiscoveryUrl,
      netmask: ipconfig.routingPrefix !== NO_ROUTING_PREFIX ?
          getRoutingPrefixAsNetmask(ipconfig.routingPrefix) :
          null,
    };

    return ipconfigUI;
  }

  /**
   * Returns a new IPConfigProperties object with netmask expressed as a
   * prefix length.
   */
  private getIPConfigProperties_(ipconfigUI: OncMojo.IPConfigUIProperties):
      IPConfigProperties {
    const ipconfig: IPConfigProperties = {
      gateway: ipconfigUI.gateway,
      ipAddress: ipconfigUI.ipAddress,
      nameServers: ipconfigUI.nameServers,
      routingPrefix: getRoutingPrefixAsLength(ipconfigUI.netmask),
      type: ipconfigUI.type,
      webProxyAutoDiscoveryUrl: ipconfigUI.webProxyAutoDiscoveryUrl,
      excludedRoutes: null,
      includedRoutes: null,
      searchDomains: null,
    };

    return ipconfig;
  }

  private hasIpConfigFields_(): boolean {
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
  }

  /**
   * Returns an edit type to be used in network-property-list.
   */
  private getIPFieldEditType_(property: OncMojo.ManagedProperty|
                              undefined): string|undefined {
    return this.isNetworkPolicyEnforced(property) ? undefined : 'String';
  }

  /**
   * Returns an object with the edit type for each editable field.
   */
  private getIPEditFields_(): Object {
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
  }

  /**
   * Event triggered when the network property list changes.
   */
  private onIPChange_(event: CustomEvent<{field: string, value: string}>):
      void {
    if (!this.ipConfig_) {
      return;
    }
    const field = event.detail.field;
    const value = event.detail.value;
    // Note: |field| includes the 'ipv4.' prefix.
    this.set('ipConfig_.' + field, value);
    this.sendStaticIpConfig_();
  }

  private sendStaticIpConfig_(): void {
    // This will also set IPAddressConfigType to STATIC.
    this.dispatchEvent(new CustomEvent('ip-change', {
      bubbles: true,
      composed: true,
      detail: {
        field: 'staticIpConfig',
        value: this.ipConfig_ && this.ipConfig_.ipv4 ?
            this.getIPConfigProperties_(this.ipConfig_.ipv4) :
            {},
      },
    }));
  }

  private computeShouldShowAutoIpConfigToggle_(): boolean {
    if (this.managedProperties &&
        this.managedProperties.type === NetworkType.kCellular) {
      return false;
    }
    return true;
  }

  private getFieldsClassList_(): string {
    let classes = 'property-box single-column stretch';
    if (this.shouldShowAutoIpConfigToggle_) {
      classes += ' indented';
    }
    return classes;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'network-ip-config': NetworkIpConfigElement;
  }
}

customElements.define(NetworkIpConfigElement.is, NetworkIpConfigElement);
