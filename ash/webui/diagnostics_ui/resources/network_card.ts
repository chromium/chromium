// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './diagnostics_card.js';
import './diagnostics_network_icon.js';
import './diagnostics_shared.css.js';
import './ip_config_info_drawer.js';
import './network_info.js';
import './network_troubleshooting.js';

import {I18nMixin} from 'chrome://resources/ash/common/cr_elements/i18n_mixin.js';
import {PolymerElementProperties} from 'chrome://resources/polymer/v3_0/polymer/interfaces.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {TroubleshootingInfo} from './diagnostics_types.js';
import {filterNameServers, formatMacAddress, getNetworkCardTitle, getNetworkState, getNetworkType, isConnectedOrOnline, isNetworkMissingNameServers} from './diagnostics_utils.js';
import {getNetworkHealthProvider} from './mojo_interface_provider.js';
import {getTemplate} from './network_card.html.js';
import {Network, NetworkHealthProviderInterface, NetworkState, NetworkStateObserverReceiver, NetworkType} from './network_health_provider.mojom-webui.js';

const BASE_SUPPORT_URL = 'https://support.google.com/chromebook?p=diagnostics_';
const SETTINGS_URL = 'chrome://os-settings/';

/**
 * Represents the state of the network troubleshooting banner.
 */
export enum TroubleshootingState {
  DISABLED,
  NOT_CONNECTED,
  MISSING_IP_ADDRESS,
  MISSING_NAME_SERVERS,
}

/**
 * @fileoverview
 * 'network-card' is a styling wrapper for a network-info element.
 */

const NetworkCardElementBase = I18nMixin(PolymerElement);

export class NetworkCardElement extends NetworkCardElementBase {
  static get is(): 'network-card' {
    return 'network-card' as const;
  }

  static get template(): HTMLTemplateElement {
    return getTemplate();
  }

  static get properties(): PolymerElementProperties {
    return {
      guid: {
        type: String,
        value: '',
      },

      networkType: {
        type: String,
        value: '',
      },

      networkState: {
        type: String,
        value: '',
      },

      network: {
        type: Object,
      },

      showNetworkDataPoints: {
        type: Boolean,
        computed: 'computeShouldShowNetworkDataPoints(network.state,' +
            ' unableToObtainIpAddress, isMissingNameServers)',
      },

      showTroubleshootingCard: {
        type: Boolean,
        value: false,
      },

      macAddress: {
        type: String,
        value: '',
      },

      unableToObtainIpAddress: {
        type: Boolean,
        value: false,
      },

      troubleshootingInfo: {
        type: Object,
        computed: 'computeTroubleshootingInfo(network.*,' +
            ' unableToObtainIpAddress, isMissingNameServers)',
      },

      timerId: {
        type: Number,
        value: -1,
      },

      timeoutInMs: {
        type: Number,
        value: 30000,
      },

      isMissingNameServers: {
        type: Boolean,
        value: false,
      },

    };
  }

  guid: string;
  network: Network;
  protected showNetworkDataPoints: boolean;
  protected showTroubleshootingCard: boolean;
  protected macAddress: string;
  protected unableToObtainIpAddress: boolean;
  protected troubleshootingInfo: TroubleshootingInfo;
  protected isMissingNameServers: boolean;
  private networkType: string;
  private networkState: string;
  private timerId: number;
  private timeoutInMs: number;
  private networkHealthProvider: NetworkHealthProviderInterface =
      getNetworkHealthProvider();
  private networkStateObserverReceiver: NetworkStateObserverReceiver|null =
      null;

  static get observers(): string[] {
    return ['observeNetwork(guid)'];
  }

  override disconnectedCallback(): void {
    super.disconnectedCallback();

    this.resetTimer();
  }

  private observeNetwork(): void {
    // If necessary, clear setTimeout and reset the timerId.
    this.resetTimer();

    // Reset this flag in case we were unable to obtain an IP Address for the
    // previous network.
    this.unableToObtainIpAddress = false;

    if (!this.guid) {
      return;
    }

    if (this.networkStateObserverReceiver) {
      this.networkStateObserverReceiver.$.close();
      this.networkStateObserverReceiver = null;
    }

    this.networkStateObserverReceiver = new NetworkStateObserverReceiver(this);

    this.networkHealthProvider.observeNetwork(
        this.networkStateObserverReceiver.$.bindNewPipeAndPassRemote(),
        this.guid);
  }

  /**
   * Implements NetworkStateObserver.onNetworkStateChanged
   */
  onNetworkStateChanged(network: Network): void {
    this.networkType = getNetworkType(network.type);
    this.networkState = getNetworkState(network.state);
    this.macAddress = network.macAddress || '';

    // Remove '0.0.0.0' (if present) from list of name servers.
    filterNameServers(network);
    this.set('network', network);
    const isIpAddressMissing = !network.ipConfig || !network.ipConfig.ipAddress;
    const isTimerInProgress = this.timerId !== -1;
    const isConnecting = network.state === NetworkState.kConnecting;

    if (!isIpAddressMissing) {
      this.isMissingNameServers = isNetworkMissingNameServers(network);
      // Reset this flag if the current network now has a valid IP Address.
      this.unableToObtainIpAddress = false;
    }

    if ((isIpAddressMissing && isConnecting) && !isTimerInProgress) {
      // Wait 30 seconds before displaying the troubleshooting banner.
      this.timerId = setTimeout(() => {
        this.resetTimer();
        this.unableToObtainIpAddress = true;
      }, this.timeoutInMs);
    }
  }

  protected getNetworkCardTitle(): string {
    return getNetworkCardTitle(this.networkType, this.networkState);
  }

  protected computeShouldShowNetworkDataPoints(): boolean {
    // Wait until the network is present before deciding.
    if (!this.network) {
      return false;
    }

    if (this.unableToObtainIpAddress || this.isMissingNameServers) {
      return false;
    }

    // Show the data-points when portal, online, connected, or connecting.
    switch (this.network.state) {
      case NetworkState.kPortal:
      case NetworkState.kOnline:
      case NetworkState.kConnected:
      case NetworkState.kConnecting:
        return true;
      default:
        return false;
    }
  }

  protected isNetworkDisabled(): boolean {
    return this.network.state === NetworkState.kDisabled;
  }

  protected getMacAddress(): string {
    if (!this.macAddress) {
      return '';
    }
    return formatMacAddress(this.macAddress);
  }

  private getDisabledTroubleshootingInfo(): TroubleshootingInfo {
    const linkText =
        this.network && this.network.type === NetworkType.kCellular ?
        this.i18n('reconnectLinkText') :
        this.i18n('joinNetworkLinkText', this.networkType);
    return {
      header: this.i18n('disabledText', this.networkType),
      linkText,
      url: SETTINGS_URL,
    };
  }

  private getNotConnectedTroubleshootingInfo(): TroubleshootingInfo {
    return {
      header: this.i18n('troubleshootingText', this.networkType),
      linkText: this.i18n('troubleConnecting'),
      url: BASE_SUPPORT_URL,
    };
  }

  private computeTroubleshootingInfo(): TroubleshootingInfo {
    let troubleshootingState: TroubleshootingState|null = null;
    if (!this.network || isConnectedOrOnline(this.network.state)) {
      // Hide the troubleshooting banner if we're in an active state
      // unlesss the network's IP Address has been missing for >= 30
      // seconds or we're missing name servers in which case we'd like
      // to display the bannner to the user.
      if (this.unableToObtainIpAddress) {
        troubleshootingState = TroubleshootingState.MISSING_IP_ADDRESS;
      }

      if (this.isMissingNameServers) {
        troubleshootingState = TroubleshootingState.MISSING_NAME_SERVERS;
      }

      if (troubleshootingState == null) {
        this.showTroubleshootingCard = false;
        return {
          header: '',
          linkText: '',
          url: '',
        };
      }
    }

    const isDisabled = this.network.state === NetworkState.kDisabled;
    // Show the not connected state for the Not Connected/Portal states.
    const isNotConnected = [
      NetworkState.kNotConnected,
      NetworkState.kPortal,
    ].includes(this.network.state);
    // Override the |troubleshootingState| value if necessary since the
    // disabled and not connected states take precedence.
    if (isNotConnected) {
      troubleshootingState = TroubleshootingState.NOT_CONNECTED;
    }

    if (isDisabled) {
      troubleshootingState = TroubleshootingState.DISABLED;
    }

    // At this point, |isConnectedOrOnline| is false, which means
    // out network state is either disabled or not connected.
    this.showTroubleshootingCard = true;

    return this.getInfoProperties(troubleshootingState);
  }

  private getMissingIpAddressInfo(): TroubleshootingInfo {
    return {
      header: this.i18n('noIpAddressText'),
      linkText: this.i18n('visitSettingsToConfigureLinkText'),
      url: SETTINGS_URL,
    };
  }

  private getMissingNameServersInfo(): TroubleshootingInfo {
    return {
      header: this.i18n('missingNameServersText'),
      linkText: this.i18n('visitSettingsToConfigureLinkText'),
      url: SETTINGS_URL,
    };
  }

  private getInfoProperties(state: TroubleshootingState|
                            null): TroubleshootingInfo {
    switch (state) {
      case TroubleshootingState.DISABLED:
        return this.getDisabledTroubleshootingInfo();
      case TroubleshootingState.NOT_CONNECTED:
        return this.getNotConnectedTroubleshootingInfo();
      case TroubleshootingState.MISSING_IP_ADDRESS:
        return this.getMissingIpAddressInfo();
      case TroubleshootingState.MISSING_NAME_SERVERS:
        return this.getMissingNameServersInfo();
      default:
        return {
          header: '',
          linkText: '',
          url: '',
        };
    }
  }

  private resetTimer(): void {
    if (this.timerId !== -1) {
      clearTimeout(this.timerId);
      this.timerId = -1;
    }
  }

  getTimerIdForTesting(): number {
    return this.timerId;
  }

  setTimeoutInMsForTesting(timeout: number): void {
    this.timeoutInMs = timeout;
  }

  getTimeoutInMsForTesting(): number {
    return this.timeoutInMs;
  }

  getUnableToObtainIpAddressForTesting(): boolean {
    return this.unableToObtainIpAddress;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [NetworkCardElement.is]: NetworkCardElement;
  }
}

customElements.define(NetworkCardElement.is, NetworkCardElement);
