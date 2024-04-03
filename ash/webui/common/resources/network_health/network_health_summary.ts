// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/ash/common/cr_elements/cr_shared_style.css.js';
import '//resources/ash/common/network/network_shared.css.js';

import {I18nMixin} from '//resources/ash/common/cr_elements/i18n_mixin.js';
import {assertNotReached} from '//resources/js/assert.js';
import {NetworkType, PortalState} from '//resources/mojo/chromeos/services/network_config/public/mojom/network_types.mojom-webui.js';
import {NetworkHealthService, NetworkHealthServiceRemote} from '//resources/mojo/chromeos/services/network_health/public/mojom/network_health.mojom-webui.js';
import {Network, NetworkHealthState, NetworkState, UInt32Value} from '//resources/mojo/chromeos/services/network_health/public/mojom/network_health_types.mojom-webui.js';
import {PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {OncMojo} from '../network/onc_mojo.js';

import {NetworkHealthContainerElement} from './network_health_container.js';
import {getTemplate} from './network_health_summary.html.js';

enum TechnologyIcons {
  CELLULAR = 'cellular_0.svg',
  ETHERNET = 'ethernet.svg',
  VPN = 'vpn.svg',
  WIFI = 'wifi_0.svg',
}

/**
 * @fileoverview Polymer element for displaying NetworkHealth properties.
 */

const NetworkHealthSummaryElementBase = I18nMixin(PolymerElement);

export class NetworkHealthSummaryElement extends
    NetworkHealthSummaryElementBase {
  static get is() {
    return 'network-health-summary' as const;
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      /**
       * Network Health State object.
       */
      networkHealthState_: Object,

      /**
       * Network Health mojo remote.
       */
      networkHealth_: Object,

      /**
       * Expanded state per network type.
       */
      typeExpanded_: Array,
    };
  }

  private networkHealthState_: NetworkHealthState|null = null;
  private networkHealth_: NetworkHealthServiceRemote =
      NetworkHealthService.getRemote();
  private typeExpanded_: boolean[] = [];

  override connectedCallback() {
    super.connectedCallback();

    this.requestNetworkHealth_();

    // Automatically refresh Network Health every second.
    window.setInterval(() => {
      this.requestNetworkHealth_();
    }, 1000);
  }

  /**
   * Requests the NetworkHealthState and updates the page.
   */
  private requestNetworkHealth_() {
    this.networkHealth_.getHealthSnapshot().then(result => {
      this.networkHealthState_ = result.state;
    });
  }

  /**
   * Returns a string for the given NetworkState.
   */
  private getNetworkStateString_(state: NetworkState): string {
    switch (state) {
      case NetworkState.kUninitialized:
        return this.i18n('NetworkHealthStateUninitialized');
      case NetworkState.kDisabled:
        return this.i18n('NetworkHealthStateDisabled');
      case NetworkState.kProhibited:
        return this.i18n('NetworkHealthStateProhibited');
      case NetworkState.kNotConnected:
        return this.i18n('NetworkHealthStateNotConnected');
      case NetworkState.kConnecting:
        return this.i18n('NetworkHealthStateConnecting');
      case NetworkState.kPortal:
        return this.i18n('NetworkHealthStatePortal');
      case NetworkState.kConnected:
        return this.i18n('NetworkHealthStateConnected');
      case NetworkState.kOnline:
        return this.i18n('NetworkHealthStateOnline');
      default:
        assertNotReached('Unexpected enum value');
    }
  }

  /**
   * Returns a boolean flag to show the PortalState attribute. The information
   * is not meaningful in all cases and should be hidden to prevent confusion.
   */
  private showPortalState_(network: Network): boolean {
    if (network.state === NetworkState.kOnline &&
        network.portalState === PortalState.kOnline) {
      return false;
    }

    const notApplicableStates = [
      NetworkState.kUninitialized,
      NetworkState.kDisabled,
      NetworkState.kProhibited,
      NetworkState.kConnecting,
      NetworkState.kNotConnected,
    ];
    if (notApplicableStates.includes(network.state)) {
      return false;
    }

    return true;
  }

  /**
   * Returns a string for the given PortalState.
   */
  private getPortalStateString_(state: PortalState): string {
    return this.i18n('OncPortalState' + OncMojo.getPortalStateString(state));
  }

  /**
   * Returns a string for the given NetworkType.
   */
  private getNetworkTypeString_(type: NetworkType): string {
    return this.i18n('OncType' + OncMojo.getNetworkTypeString(type));
  }

  /**
   * Returns a icon for the given NetworkType.
   */
  private getNetworkTypeIcon_(type: NetworkType): string {
    switch (type) {
      case NetworkType.kEthernet:
        return TechnologyIcons.ETHERNET;
      case NetworkType.kWiFi:
        return TechnologyIcons.WIFI;
      case NetworkType.kVPN:
        return TechnologyIcons.VPN;
      case NetworkType.kTether:
      case NetworkType.kMobile:
      case NetworkType.kCellular:
        return TechnologyIcons.CELLULAR;
      default:
        return '';
    }
  }

  /**
   * Returns a string for the given signal strength.
   */
  private getSignalStrengthString_(signalStrength: UInt32Value|null): string {
    return signalStrength ? signalStrength.value.toString() : '';
  }

  /**
   * Returns a boolean flag if the open to settings link should be shown.
   */
  private showSettingsLink_(network: Network): boolean {
    const validStates = [
      NetworkState.kConnected,
      NetworkState.kConnecting,
      NetworkState.kPortal,
      NetworkState.kOnline,
    ];
    return validStates.includes(network.state);
  }

  /**
   * Returns a URL for the network's settings page.
   */
  private getNetworkUrl_(network: Network): string {
    return 'chrome://os-settings/networkDetail?guid=' + network.guid;
  }

  /**
   * Returns a concatenated list of strings.
   */
  private joinAddresses_(addresses: string[]): string {
    return addresses.join(', ');
  }

  /**
   * Returns a boolean flag if the routine type should be expanded.
   */
  private getTypeExpanded_(networkType: NetworkType): boolean {
    if (this.typeExpanded_[Number(networkType)] === undefined) {
      this.set('typeExpanded_.' + networkType, false);
      return false;
    }

    return this.typeExpanded_[Number(networkType)];
  }

  /**
   * Helper function to toggle the expanded properties when the network
   * container is toggled.
   */
  private onToggleExpanded_(event: Event&{model: {network: Network}}) {
    const type = event.model.network.type;
    this.set('typeExpanded_.' + type, !this.typeExpanded_[Number(type)]);
  }
}

declare global {
  interface HTMLElementEventMap {
    'toggle-expanded': NetworkHealthContainerElement;
  }

  interface HTMLElementTagNameMap {
    [NetworkHealthSummaryElement.is]: NetworkHealthSummaryElement;
  }
}

customElements.define(
    NetworkHealthSummaryElement.is, NetworkHealthSummaryElement);
