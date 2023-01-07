// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './diagnostics_card.js';
import './diagnostics_network_icon.js';
import './diagnostics_shared.css.js';
import './ip_config_info_drawer.js';
import './network_info.js';
import './network_troubleshooting.js';

import {I18nMixin} from 'chrome://resources/cr_elements/i18n_mixin.js';
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
  static get is() {
    return 'network-card';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      guid: {
        type: String,
        value: '',
      },

      networkType_: {
        type: String,
        value: '',
      },

      networkState_: {
        type: String,
        value: '',
      },

      network: {
        type: Object,
      },

      showNetworkDataPoints_: {
        type: Boolean,
        computed: 'computeShouldShowNetworkDataPoints_(network.state,' +
            ' unableToObtainIpAddress_, isMissingNameServers_)',
      },

      showTroubleshootingCard_: {
        type: Boolean,
        value: false,
      },

      macAddress_: {
        type: String,
        value: '',
      },

      unableToObtainIpAddress_: {
        type: Boolean,
        value: false,
      },

      troubleshootingInfo_: {
        type: Object,
        computed: 'computeTroubleshootingInfo_(network.*,' +
            ' unableToObtainIpAddress_, isMissingNameServers_)',
      },

      timerId_: {
        type: Number,
        value: -1,
      },

      timeoutInMs_: {
        type: Number,
        value: 30000,
      },

      isMissingNameServers_: {
        type: Boolean,
        value: false,
      },

    };
  }

  guid: string;
  network: Network;
  protected showNetworkDataPoints_: boolean;
  protected showTroubleshootingCard_: boolean;
  protected macAddress_: string;
  protected unableToObtainIpAddress_: boolean;
  protected troubleshootingInfo_: TroubleshootingInfo;
  protected isMissingNameServers_: boolean;
  private networkType_: string;
  private networkState_: string;
  private timerId_: number;
  private timeoutInMs_: number;
  private networkHealthProvider_: NetworkHealthProviderInterface =
      getNetworkHealthProvider();
  private networkStateObserverReceiver_: NetworkStateObserverReceiver|null =
      null;

  static get observers() {
    return ['observeNetwork_(guid)'];
  }

  override disconnectedCallback() {
    super.disconnectedCallback();

    this.resetTimer_();
  }

  private observeNetwork_(): void {
    // If necessary, clear setTimeout and reset the timerId.
    this.resetTimer_();

    // Reset this flag in case we were unable to obtain an IP Address for the
    // previous network.
    this.unableToObtainIpAddress_ = false;

    if (!this.guid) {
      return;
    }

    if (this.networkStateObserverReceiver_) {
      this.networkStateObserverReceiver_.$.close();
      this.networkStateObserverReceiver_ = null;
    }

    this.networkStateObserverReceiver_ = new NetworkStateObserverReceiver(this);

    this.networkHealthProvider_.observeNetwork(
        this.networkStateObserverReceiver_.$.bindNewPipeAndPassRemote(),
        this.guid);
  }

  /**
   * Implements NetworkStateObserver.onNetworkStateChanged
   */
  onNetworkStateChanged(network: Network): void {
    this.networkType_ = getNetworkType(network.type);
    this.networkState_ = getNetworkState(network.state);
    this.macAddress_ = network.macAddress || '';

    // Remove '0.0.0.0' (if present) from list of name servers.
    filterNameServers(network);
    this.set('network', network);
    const isIpAddressMissing = !network.ipConfig || !network.ipConfig.ipAddress;
    const isTimerInProgress = this.timerId_ !== -1;
    const isConnecting = network.state === NetworkState.kConnecting;

    if (!isIpAddressMissing) {
      this.isMissingNameServers_ = isNetworkMissingNameServers(network);
      // Reset this flag if the current network now has a valid IP Address.
      this.unableToObtainIpAddress_ = false;
    }

    if ((isIpAddressMissing && isConnecting) && !isTimerInProgress) {
      // Wait 30 seconds before displaying the troubleshooting banner.
      this.timerId_ = setTimeout(() => {
        this.resetTimer_();
        this.unableToObtainIpAddress_ = true;
      }, this.timeoutInMs_);
    }
  }

  protected getNetworkCardTitle_(): string {
    return getNetworkCardTitle(this.networkType_, this.networkState_);
  }

  protected computeShouldShowNetworkDataPoints_(): boolean {
    // Wait until the network is present before deciding.
    if (!this.network) {
      return false;
    }

    if (this.unableToObtainIpAddress_ || this.isMissingNameServers_) {
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

  protected isNetworkDisabled_(): boolean {
    return this.network.state === NetworkState.kDisabled;
  }

  protected getMacAddress_(): string {
    if (!this.macAddress_) {
      return '';
    }
    return formatMacAddress(this.macAddress_);
  }

  private getDisabledTroubleshootingInfo_(): TroubleshootingInfo {
    const linkText =
        this.network && this.network.type === NetworkType.kCellular ?
        this.i18n('reconnectLinkText') :
        this.i18n('joinNetworkLinkText', this.networkType_);
    return {
      header: this.i18n('disabledText', this.networkType_),
      linkText,
      url: SETTINGS_URL,
    };
  }

  private getNotConnectedTroubleshootingInfo_(): TroubleshootingInfo {
    return {
      header: this.i18n('troubleshootingText', this.networkType_),
      linkText: this.i18n('troubleConnecting'),
      url: BASE_SUPPORT_URL,
    };
  }

  private computeTroubleshootingInfo_(): TroubleshootingInfo {
    let troubleshootingState: TroubleshootingState|null = null;
    if (!this.network || isConnectedOrOnline(this.network.state)) {
      // Hide the troubleshooting banner if we're in an active state
      // unlesss the network's IP Address has been missing for >= 30
      // seconds or we're missing name servers in which case we'd like
      // to display the bannner to the user.
      if (this.unableToObtainIpAddress_) {
        troubleshootingState = TroubleshootingState.MISSING_IP_ADDRESS;
      }

      if (this.isMissingNameServers_) {
        troubleshootingState = TroubleshootingState.MISSING_NAME_SERVERS;
      }

      if (troubleshootingState == null) {
        this.showTroubleshootingCard_ = false;
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
    this.showTroubleshootingCard_ = true;

    return this.getInfoProperties_(troubleshootingState);
  }

  private getMissingIpAddressInfo_(): TroubleshootingInfo {
    return {
      header: this.i18n('noIpAddressText'),
      linkText: this.i18n('visitSettingsToConfigureLinkText'),
      url: SETTINGS_URL,
    };
  }

  private getMissingNameServersInfo_(): TroubleshootingInfo {
    return {
      header: this.i18n('missingNameServersText'),
      linkText: this.i18n('visitSettingsToConfigureLinkText'),
      url: SETTINGS_URL,
    };
  }

  private getInfoProperties_(state: TroubleshootingState|
                             null): TroubleshootingInfo {
    switch (state) {
      case TroubleshootingState.DISABLED:
        return this.getDisabledTroubleshootingInfo_();
      case TroubleshootingState.NOT_CONNECTED:
        return this.getNotConnectedTroubleshootingInfo_();
      case TroubleshootingState.MISSING_IP_ADDRESS:
        return this.getMissingIpAddressInfo_();
      case TroubleshootingState.MISSING_NAME_SERVERS:
        return this.getMissingNameServersInfo_();
      default:
        return {
          header: '',
          linkText: '',
          url: '',
        };
    }
  }

  private resetTimer_(): void {
    if (this.timerId_ !== -1) {
      clearTimeout(this.timerId_);
      this.timerId_ = -1;
    }
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'network-card': NetworkCardElement;
  }
}

customElements.define(NetworkCardElement.is, NetworkCardElement);
