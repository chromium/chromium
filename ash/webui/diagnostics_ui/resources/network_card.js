// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './diagnostics_card.js';
import './diagnostics_network_icon.js';
import './diagnostics_shared.css.js';
import './ip_config_info_drawer.js';
import './network_info.js';
import './network_troubleshooting.js';

import {I18nBehavior, I18nBehaviorInterface} from 'chrome://resources/cr_elements/i18n_behavior.js';
import {mixinBehaviors, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {TroubleshootingInfo} from './diagnostics_types.js';
import {filterNameServers, formatMacAddress, getNetworkCardTitle, getNetworkState, getNetworkType, isConnectedOrOnline, isNetworkMissingNameServers} from './diagnostics_utils.js';
import {getNetworkHealthProvider} from './mojo_interface_provider.js';
import {Network, NetworkHealthProviderInterface, NetworkState, NetworkStateObserverInterface, NetworkStateObserverReceiver, NetworkType} from './network_health_provider.mojom-webui.js';
import {getTemplate} from './network_card.html.js';

const BASE_SUPPORT_URL = 'https://support.google.com/chromebook?p=diagnostics_';
const SETTINGS_URL = 'chrome://os-settings/';

/**
 * Represents the state of the network troubleshooting banner.
 * @enum {number}
 */
export const TroubleshootingState = {
  DISABLED: 0,
  NOT_CONNECTED: 1,
  MISSING_IP_ADDRESS: 2,
  MISSING_NAME_SERVERS: 3,
};

/**
 * @fileoverview
 * 'network-card' is a styling wrapper for a network-info element.
 */

/**
 * @constructor
 * @extends {PolymerElement}
 * @implements {I18nBehaviorInterface}
 */
const NetworkCardElementBase = mixinBehaviors([I18nBehavior], PolymerElement);

/** @polymer */
export class NetworkCardElement extends NetworkCardElementBase {
  static get is() {
    return 'network-card';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      /** @type {string} */
      guid: {
        type: String,
        value: '',
      },

      /** @private {string} */
      networkType_: {
        type: String,
        value: '',
      },

      /** @private {string} */
      networkState_: {
        type: String,
        value: '',
      },

      /** @type {!Network} */
      network: {
        type: Object,
      },

      /** @protected {boolean} */
      showNetworkDataPoints_: {
        type: Boolean,
        computed: 'computeShouldShowNetworkDataPoints_(network.state,' +
            ' unableToObtainIpAddress_, isMissingNameServers_)',
      },

      /** @protected {boolean} */
      showTroubleshootingCard_: {
        type: Boolean,
        value: false,
      },

      /** @protected {string} */
      macAddress_: {
        type: String,
        value: '',
      },

      /** @protected {boolean} */
      unableToObtainIpAddress_: {
        type: Boolean,
        value: false,
      },

      /** @protected {TroubleshootingInfo} */
      troubleshootingInfo_: {
        type: Object,
        computed: 'computeTroubleshootingInfo_(network.*,' +
            ' unableToObtainIpAddress_, isMissingNameServers_)',
      },

      /** @private */
      timerId_: {
        type: Number,
        value: -1,
      },

      /** @private */
      timeoutInMs_: {
        type: Number,
        value: 30000,
      },

      /** @protected {boolean} */
      isMissingNameServers_: {
        type: Boolean,
        value: false,
      },

    };
  }

  static get observers() {
    return ['observeNetwork_(guid)'];
  }


  /** @override */
  constructor() {
    super();
    /**
     * @private {?NetworkHealthProviderInterface}
     */
    this.networkHealthProvider_ = null;

    /**
     * Receiver responsible for observing a single active network connection.
     * @private {?NetworkStateObserverReceiver}
     */
    this.networkStateObserverReceiver_ = null;

    this.networkHealthProvider_ = getNetworkHealthProvider();
  }

  /** @override */
  disconnectedCallback() {
    super.disconnectedCallback();

    this.resetTimer_();
  }

  /** @private */
  observeNetwork_() {
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

    this.networkStateObserverReceiver_ = new NetworkStateObserverReceiver(
        /**
         * @type {!NetworkStateObserverInterface}
         */
        (this));

    this.networkHealthProvider_.observeNetwork(
        this.networkStateObserverReceiver_.$.bindNewPipeAndPassRemote(),
        this.guid);
  }

  /**
   * Implements NetworkStateObserver.onNetworkStateChanged
   * @param {!Network} network
   */
  onNetworkStateChanged(network) {
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

  /**
   * @protected
   * @return {string}
   */
  getNetworkCardTitle_() {
    return getNetworkCardTitle(this.networkType_, this.networkState_);
  }

  /**
   * @protected
   * @return {boolean}
   */
  computeShouldShowNetworkDataPoints_() {
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

  /**
   * @protected
   * @return {boolean}
   */
  isNetworkDisabled_() {
    return this.network.state === NetworkState.kDisabled;
  }

  /**
   * @protected
   * @return {string}
   */
  getMacAddress_() {
    if (!this.macAddress_) {
      return '';
    }
    return formatMacAddress(this.macAddress_);
  }

  /**
   * @private
   * @return {!TroubleshootingInfo}
   */
  getDisabledTroubleshootingInfo_() {
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

  /**
   * @private
   * @return {!TroubleshootingInfo}
   */
  getNotConnectedTroubleshootingInfo_() {
    return {
      header: this.i18n('troubleshootingText', this.networkType_),
      linkText: this.i18n('troubleConnecting'),
      url: BASE_SUPPORT_URL,
    };
  }

  /**
   * @private
   * @return {!TroubleshootingInfo}
   */
  computeTroubleshootingInfo_() {
    let troubleshootingState = null;
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

    // At this point, |isConnectedOrOnline| was falsy which means
    // out network state is either disabled or not connected.
    this.showTroubleshootingCard_ = true;
    return this.getInfoProperties_(
        /** @type {!TroubleshootingState} */ (troubleshootingState));
  }

  /**
   * @private
   * @return {!TroubleshootingInfo}
   */
  getMissingIpAddressInfo_() {
    return {
      header: this.i18n('noIpAddressText'),
      linkText: this.i18n('visitSettingsToConfigureLinkText'),
      url: SETTINGS_URL,
    };
  }

  /**
   * @private
   * @return {!TroubleshootingInfo}
   */
  getMissingNameServersInfo_() {
    return {
      header: this.i18n('missingNameServersText'),
      linkText: this.i18n('visitSettingsToConfigureLinkText'),
      url: SETTINGS_URL,
    };
  }

  /**
   * @private
   * @param {!TroubleshootingState} state
   * @return {!TroubleshootingInfo}
   */
  getInfoProperties_(state) {
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

  /** @private */
  resetTimer_() {
    if (this.timerId_ !== -1) {
      clearTimeout(this.timerId_);
      this.timerId_ = -1;
    }
  }
}

customElements.define(NetworkCardElement.is, NetworkCardElement);
