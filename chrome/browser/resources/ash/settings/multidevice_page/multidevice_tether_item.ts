// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * This element provides a layer between the settings-multidevice-subpage
 * element and the internet_page folder's network-summary-item. It is
 * responsible for loading initial tethering network data from the
 * networkConfig mojo API as well as updating the data in real time. It
 * serves a role comparable to the internet_page's network-summary element.
 */

import 'chrome://resources/ash/common/network/network_icon.js';
import '../settings_shared.css.js';
import '../settings_vars.css.js';
import './multidevice_feature_item.js';

import {MojoInterfaceProviderImpl} from 'chrome://resources/ash/common/network/mojo_interface_provider.js';
import {NetworkListenerBehavior, NetworkListenerBehaviorInterface} from 'chrome://resources/ash/common/network/network_listener_behavior.js';
import {OncMojo} from 'chrome://resources/ash/common/network/onc_mojo.js';
import {assert} from 'chrome://resources/js/assert.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {CrosNetworkConfigInterface, FilterType, InhibitReason, NetworkStateProperties} from 'chrome://resources/mojo/chromeos/services/network_config/public/mojom/cros_network_config.mojom-webui.js';
import {ConnectionStateType, DeviceStateType, NetworkType} from 'chrome://resources/mojo/chromeos/services/network_config/public/mojom/network_types.mojom-webui.js';
import {mixinBehaviors, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {castExists} from '../assert_extras.js';
import {isRevampWayfindingEnabled} from '../common/load_time_booleans.js';
import {Constructor} from '../common/types.js';
import {routes} from '../router.js';

import {MultiDeviceFeatureMixin, MultiDeviceFeatureMixinInterface} from './multidevice_feature_mixin.js';
import {getTemplate} from './multidevice_tether_item.html.js';

const SettingsMultideviceTetherItemElementBase =
    mixinBehaviors(
        [
          NetworkListenerBehavior,
        ],
        MultiDeviceFeatureMixin(PolymerElement)) as
    Constructor<PolymerElement&MultiDeviceFeatureMixinInterface&
                NetworkListenerBehaviorInterface>;

class SettingsMultideviceTetherItemElement extends
    SettingsMultideviceTetherItemElementBase {
  static get is() {
    return 'settings-multidevice-tether-item' as const;
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      /**
       * The device state for tethering.
       */
      deviceState_: Object,

      /**
       * The network state for a potential tethering host phone. Note that there
       * is at most one because only one MultiDevice host phone is allowed on an
       * account at a given time.
       */
      activeNetworkState_: Object,

      /**
       * Alias for allowing Polymer bindings to routes.
       */
      routes: {
        type: Object,
        value: routes,
        readonly: true,
      },

      isRevampWayfindingEnabled_: {
        type: Boolean,
        value: () => {
          return isRevampWayfindingEnabled();
        },
      },

      /**
       * Whether to show technology badge on mobile network icon.
       */
      showTechnologyBadge_: {
        type: Boolean,
        value() {
          return loadTimeData.valueExists('showTechnologyBadge') &&
              loadTimeData.getBoolean('showTechnologyBadge');
        },
      },
    };
  }

  private activeNetworkState_: OncMojo.NetworkStateProperties|undefined;
  private deviceState_: OncMojo.DeviceStateProperties|undefined;
  private isRevampWayfindingEnabled_: boolean;
  private networkConfig_: CrosNetworkConfigInterface;
  private showTechnologyBadge_: boolean;

  constructor() {
    super();

    this.networkConfig_ =
        MojoInterfaceProviderImpl.getInstance().getMojoServiceRemote();
  }

  override connectedCallback(): void {
    super.connectedCallback();

    this.updateTetherDeviceState_();
    this.updateTetherNetworkState_();
  }

  override focus(): void {
    this.shadowRoot!.querySelector(
                        'settings-multidevice-feature-item')!.focus();
  }

  /**
   * CrosNetworkConfigObserver impl
   * Note that any change to leading to a new active network will also trigger
   * onNetworkStateListChanged, triggering updateTetherNetworkState_ and
   * rendering this callback redundant. As a result, we return early if the
   * active network is not changed.
   */
  override onActiveNetworksChanged(networks: NetworkStateProperties[]): void {
    const guid = this.activeNetworkState_!.guid;
    if (!networks.find(network => network.guid === guid)) {
      return;
    }
    this.networkConfig_.getNetworkState(guid).then(response => {
      if (response.result) {
        this.activeNetworkState_ = response.result;
      }
    });
  }

  /** CrosNetworkConfigObserver impl */
  override onNetworkStateListChanged(): void {
    this.updateTetherNetworkState_();
  }

  /** CrosNetworkConfigObserver impl */
  override onDeviceStateListChanged(): void {
    this.updateTetherDeviceState_();
  }

  /**
   * Retrieves device states (OncMojo.DeviceStateProperties) and sets
   * this.deviceState_ to the retrieved Tether device state (or undefined if
   * there is none). Note that crosNetworkConfig.getDeviceStateList retrieves at
   * most one device per NetworkType so there will be at most one Tether device
   * state.
   */
  private updateTetherDeviceState_(): void {
    this.networkConfig_.getDeviceStateList().then(response => {
      const kTether = NetworkType.kTether;
      const deviceStates = response.result;
      const deviceState =
          deviceStates.find(deviceState => deviceState.type === kTether);
      this.deviceState_ = deviceState || {
        deviceState: DeviceStateType.kDisabled,
        inhibitReason: InhibitReason.kNotInhibited,
        managedNetworkAvailable: false,
        scanning: false,
        simAbsent: false,
        type: kTether,
      } as OncMojo.DeviceStateProperties;
    });
  }

  /**
   * Retrieves all Instant Tethering network states
   * (OncMojo.NetworkStateProperties). Note that there is at most one because
   * only one host is allowed on an account at a given time. Then it sets
   * this.activeNetworkState_ to that network if there is one or a dummy object
   * with an empty string for a GUID otherwise.
   */
  private updateTetherNetworkState_(): void {
    const kTether = NetworkType.kTether;
    const filter = {
      filter: FilterType.kVisible,
      limit: 1,
      networkType: kTether,
    };
    this.networkConfig_.getNetworkStateList(filter).then(response => {
      const networks = response.result;
      this.activeNetworkState_ =
          networks[0] || OncMojo.getDefaultNetworkState(kTether);
    });
  }

  /**
   * Returns an array containing the active network state if there is one
   * (note that if there is not GUID will be falsy).  Returns an empty array
   * otherwise.
   */
  private getNetworkStateList_(): NetworkStateProperties[] {
    return this.activeNetworkState_!.guid ?
        [castExists(this.activeNetworkState_)] :
        [];
  }

  private getTetherNetworkUrlSearchParams_(): URLSearchParams {
    return new URLSearchParams('type=Tether');
  }

  private getInstantTetheringDescription_(): string {
    const deviceState = this.deviceState_;
    // If the `deviceState` is enabled, the description depends on the
    // `connectionState`, otherwise return the disabled description directly.
    if (deviceState && deviceState.deviceState === DeviceStateType.kEnabled) {
      assert(deviceState.type === NetworkType.kTether);
      if (this.activeNetworkState_) {
        const connectionState = this.activeNetworkState_.connectionState;
        const deviceName = this.pageContentData.hostDeviceName || '';
        if (OncMojo.connectionStateIsConnected(connectionState)) {
          return this.i18n(
              'multideviceInstantTetheringItemConnectedDescription',
              deviceName);
        }
        if (connectionState === ConnectionStateType.kConnecting) {
          return this.i18n(
              'multideviceInstantTetheringItemConnectingDescription',
              deviceName);
        }
        if (connectionState === ConnectionStateType.kNotConnected) {
          return this.i18n(
              'multideviceInstantTetheringItemNoNetworkDescription',
              deviceName);
        }
      }
    }

    return this.i18n('multideviceInstantTetheringItemDisabledDescription');
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [SettingsMultideviceTetherItemElement.is]:
        SettingsMultideviceTetherItemElement;
  }
}

customElements.define(
    SettingsMultideviceTetherItemElement.is,
    SettingsMultideviceTetherItemElement);
