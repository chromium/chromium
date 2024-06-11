// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Polymer element for rendering network icons based on ONC
 * state properties.
 */

import './network_icons.html.js';
import '//resources/ash/common/cr_elements/cr_hidden_style.css.js';
import '//resources/polymer/v3_0/iron-icon/iron-icon.js';

import {HotspotState} from '//resources/ash/common/hotspot/cros_hotspot_config.mojom-webui.js';
import {I18nBehavior, I18nBehaviorInterface} from '//resources/ash/common/i18n_behavior.js';
import {loadTimeData} from '//resources/ash/common/load_time_data.m.js';
import {ActivationStateType, SecurityType} from '//resources/mojo/chromeos/services/network_config/public/mojom/cros_network_config.mojom-webui.js';
import {ConnectionStateType, DeviceStateType, NetworkType} from '//resources/mojo/chromeos/services/network_config/public/mojom/network_types.mojom-webui.js';
import {mixinBehaviors, PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './network_icon.html.js';
import {OncMojo} from './onc_mojo.js';

/**
 * @constructor
 * @extends {PolymerElement}
 * @implements {I18nBehaviorInterface}
 */
const NetworkIconElementBase = mixinBehaviors([I18nBehavior], PolymerElement);

/** @polymer */
export class NetworkIconElement extends NetworkIconElementBase {
  static get is() {
    return 'network-icon';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      /**
       * If set, the ONC properties will be used to display the icon. This may
       * either be the complete set of NetworkProperties or the subset of
       * NetworkStateProperties.
       * @type {!OncMojo.NetworkStateProperties|undefined}
       */
      networkState: Object,

      /**
       * If set, hotspot state within this object will be used to update the
       * hotspot icon.
       */
      hotspotInfo: Object,

      /**
       * If set, the device state for the network type. Otherwise it defaults to
       * null rather than undefined so that it does not block computed bindings.
       * @type {?OncMojo.DeviceStateProperties}
       */
      deviceState: {
        type: Object,
        value: null,
      },

      /**
       * If true, the icon is part of a list of networks and may be displayed
       * differently, e.g. the disconnected image will never be shown for
       * list items.
       */
      isListItem: {
        type: Boolean,
        value: false,
      },

      /**
       * If true, cellular technology badge is displayed in the network icon.
       */
      showTechnologyBadge: {
        type: Boolean,
        value: true,
      },

      /**
       * This provides an accessibility label that describes the connection
       * state and signal level. This can be used by other components in a
       * aria-describedby by referencing this elements id.
       */
      ariaLabel: {
        type: String,
        reflectToAttribute: true,
        computed: 'computeAriaLabel_(locale, networkState, hotspotInfo)',
      },

      /** @private {boolean} */
      isUserLoggedIn_: {
        type: Boolean,
        value() {
          return loadTimeData.valueExists('isUserLoggedIn') &&
              loadTimeData.getBoolean('isUserLoggedIn');
        },
      },
    };
  }

  constructor() {
    super();

    /**
     * Number of network icons for different cellular or wifi network signal
     * strengths.
     * @private @const {number}
     */
    this.networkIconCount_ = 5;
  }

  /**
   * @return {string} The name of the svg icon image to show.
   * @private
   */
  getIconClass_() {
    // NOTE: computeAriaLabel_() follows a very similar logic structure and both
    // functions should be updated together.

    if (!this.networkState && !this.hotspotInfo) {
      return '';
    }

    if (this.hotspotInfo) {
      if (this.hotspotInfo.state === HotspotState.kEnabled) {
        return 'hotspot-on';
      }
      if (this.hotspotInfo.state === HotspotState.kEnabling) {
        return 'hotspot-connecting';
      }
      return 'hotspot-off';
    }

    const type = this.networkState.type;
    if (type === NetworkType.kEthernet) {
      return 'ethernet';
    }
    if (type === NetworkType.kVPN) {
      return 'vpn';
    }

    const prefix = OncMojo.networkTypeIsMobile(type) ? 'cellular-' : 'wifi-';

    if (this.isPSimPendingActivationWhileLoggedOut_()) {
      return prefix + 'not-activated';
    }

    if (this.networkState.type === NetworkType.kCellular &&
        this.networkState.typeState.cellular.simLocked) {
      if (this.networkState.typeState.cellular.simLockType === 'network-pin') {
        return prefix + 'carrier-locked';
      }
      return prefix + 'locked';
    }

    if (!this.isListItem && !this.networkState.guid) {
      const device = this.deviceState;
      if (!device || device.deviceState === DeviceStateType.kEnabled ||
          device.deviceState === DeviceStateType.kEnabling) {
        return prefix + 'no-network';
      }
      return prefix + 'off';
    }

    const connectionState = this.networkState.connectionState;
    if (connectionState === ConnectionStateType.kConnecting) {
      return prefix + 'connecting';
    }

    if (!this.isListItem &&
        connectionState === ConnectionStateType.kNotConnected) {
      return prefix + 'not-connected';
    }

    const strength = OncMojo.getSignalStrength(this.networkState);
    return prefix + this.strengthToIndex_(strength).toString(10);
  }

  /**
   * @param {string} locale The current local which is passed only to ensure
   * the aria label is updated when the locale changes.
   * @param {!OncMojo.NetworkStateProperties|undefined} networkState The current
   * networkState.
   * @return {string} A localized accessibility label for the icon.
   * @private
   */
  computeAriaLabel_(locale, networkState) {
    // NOTE: getIconClass_() follows a very similar logic structure and both
    // functions should be updated together.

    if (this.hotspotInfo) {
      // TODO(b/284324373): Finalize aria labels for hotspot and update them
      // here.
      return 'hotspot';
    }

    if (!this.networkState) {
      return '';
    }

    const type = this.networkState.type;

    // Ethernet and VPN connection labels don't attempt to describe the network
    // state like the icons, so there is only one string for each.
    if (type === NetworkType.kEthernet) {
      return this.i18nDynamic(locale, 'networkIconLabelEthernet');
    }
    if (type === NetworkType.kVPN) {
      return this.i18nDynamic(locale, 'networkIconLabelVpn');
    }

    // networkTypeString will hold a localized, generic network type name:
    // 'Instant Tether', 'Cellular', 'Wi-Fi' which will be using to form the
    // full localized connection state string.
    let networkTypeString = '';
    if (type === NetworkType.kTether) {
      networkTypeString = this.i18nDynamic(locale, 'OncTypeTether');
    } else if (OncMojo.networkTypeIsMobile(type)) {
      networkTypeString = this.i18nDynamic(locale, 'OncTypeCellular');
    } else {
      networkTypeString = this.i18nDynamic(locale, 'OncTypeWiFi');
    }

    // When isListItem === true, we want to describe the network and signal
    // strength regardless of connection state (i.e. when picking a Wi-Fi
    // network to connect to. If isListItem === false we try to describe the
    // current connection state and describe signal strength only if connected.

    if (!this.isListItem && !this.networkState.guid) {
      const device = this.deviceState;
      // Networks with no guid are generally UI placeholders.
      if (!device || device.deviceState === DeviceStateType.kEnabled ||
          device.deviceState === DeviceStateType.kEnabling) {
        return this.i18nDynamic(
            locale, 'networkIconLabelNoNetwork', networkTypeString);
      }
      return this.i18nDynamic(locale, 'networkIconLabelOff', networkTypeString);
    }

    const connectionState = this.networkState.connectionState;
    if (connectionState === ConnectionStateType.kConnecting) {
      return this.i18nDynamic(
          locale, 'networkIconLabelConnecting', networkTypeString);
    }

    if (!this.isListItem &&
        connectionState === ConnectionStateType.kNotConnected) {
      // We only show 'Not Connected' when we are not in a list.
      return this.i18nDynamic(
          locale, 'networkIconLabelNotConnected', networkTypeString);
    }

    // Here we have a Cellular, Instant Tether, or Wi-Fi network with signal
    // strength available.
    const strength = OncMojo.getSignalStrength(this.networkState);
    return this.i18nDynamic(
        locale, 'networkIconLabelSignalStrength', networkTypeString,
        strength.toString(10));
  }

  /**
   * @param {number} strength The signal strength from [0 - 100].
   * @return {number} An index from 0 to |this.networkIconCount_ - 1|
   * corresponding to |strength|.
   * @private
   */
  strengthToIndex_(strength) {
    if (strength <= 0) {
      return 0;
    }

    if (strength >= 100) {
      return this.networkIconCount_ - 1;
    }

    const zeroBasedIndex =
        Math.trunc((strength - 1) * (this.networkIconCount_ - 1) / 100);
    return zeroBasedIndex + 1;
  }

  /**
   * @return {boolean}
   * @private
   */
  showTechnology_() {
    if (!this.networkState || this.hotspotInfo) {
      return false;
    }
    return !this.showRoaming_() &&
        OncMojo.connectionStateIsConnected(this.networkState.connectionState) &&
        this.getTechnology_() !== '' && this.showTechnologyBadge;
  }

  /**
   * @return {string}
   * @private
   */
  getTechnology_() {
    if (!this.networkState || this.hotspotInfo) {
      return '';
    }
    if (this.networkState.type === NetworkType.kCellular) {
      const technology = this.getTechnologyId_(
          this.networkState.typeState.cellular.networkTechnology);
      if (technology !== '') {
        return 'network:' + technology;
      }
    }
    return '';
  }

  /**
   * @param {string|undefined} networkTechnology
   * @return {string}
   * @private
   */
  getTechnologyId_(networkTechnology) {
    switch (networkTechnology) {
      case 'CDMA1XRTT':
        return 'badge-1x';
      case 'EDGE':
        return 'badge-edge';
      case 'EVDO':
        return 'badge-evdo';
      case 'GPRS':
      case 'GSM':
        return 'badge-gsm';
      case 'HSPA':
        return 'badge-hspa';
      case 'HSPAPlus':
        return 'badge-hspa-plus';
      case 'LTE':
        return 'badge-lte';
      case 'LTEAdvanced':
        return 'badge-lte-advanced';
      case 'UMTS':
        return 'badge-3g';
      case '5GNR':
        return 'badge-5g';
    }
    return '';
  }

  /**
   * @return {boolean}
   * @private
   */
  showSecure_() {
    if (!this.networkState || this.hotspotInfo) {
      return false;
    }
    if (!this.isListItem &&
        this.networkState.connectionState ===
            ConnectionStateType.kNotConnected) {
      return false;
    }
    return this.networkState.type === NetworkType.kWiFi &&
        this.networkState.typeState.wifi.security !== SecurityType.kNone;
  }

  /**
   * @return {boolean}
   * @private
   */
  showRoaming_() {
    if (!this.networkState) {
      return false;
    }
    return this.networkState.type === NetworkType.kCellular &&
        this.networkState.typeState.cellular.roaming;
  }

  /**
   * @return {boolean}
   * @private
   */
  showIcon_() {
    return !!this.networkState || !!this.hotspotInfo;
  }

  /**
   * Return true if current network is pSIM, requires activation and user is
   * not logged in or gone through device setup (OOBE).
   * @return {boolean}
   * @private
   */
  isPSimPendingActivationWhileLoggedOut_() {
    const cellularProperties = this.networkState.typeState.cellular;

    if (!cellularProperties || cellularProperties.eid || this.isUserLoggedIn_) {
      return false;
    }

    return cellularProperties.activationState ==
        ActivationStateType.kNotActivated;
  }
}

customElements.define(NetworkIconElement.is, NetworkIconElement);
