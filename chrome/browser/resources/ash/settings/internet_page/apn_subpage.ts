// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * Settings subpage for managing APNs.
 */

import './internet_shared.css.js';
import 'chrome://resources/ash/common/network/apn_list.js';

import {ApnListElement} from 'chrome://resources/ash/common/network/apn_list.js';
import {processDeviceState} from 'chrome://resources/ash/common/network/cellular_utils.js';
import {MojoInterfaceProviderImpl} from 'chrome://resources/ash/common/network/mojo_interface_provider.js';
import {NetworkListenerBehavior, NetworkListenerBehaviorInterface} from 'chrome://resources/ash/common/network/network_listener_behavior.js';
import {OncMojo} from 'chrome://resources/ash/common/network/onc_mojo.js';
import {assert} from 'chrome://resources/js/assert.js';
import {CrosNetworkConfigInterface, ManagedProperties, MAX_NUM_CUSTOM_APNS, NetworkStateProperties} from 'chrome://resources/mojo/chromeos/services/network_config/public/mojom/cros_network_config.mojom-webui.js';
import {NetworkType} from 'chrome://resources/mojo/chromeos/services/network_config/public/mojom/network_types.mojom-webui.js';
import {mixinBehaviors, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {RouteObserverMixin, RouteObserverMixinInterface} from '../common/route_observer_mixin.js';
import {Constructor} from '../common/types.js';
import {Route, Router, routes} from '../router.js';

import {getTemplate} from './apn_subpage.html.js';

export interface ApnSubpageElement {
  $: {
    apnList: ApnListElement,
  };
}

const ApnSubpageElementBase = mixinBehaviors(
                                  [
                                    NetworkListenerBehavior,
                                  ],
                                  RouteObserverMixin(PolymerElement)) as
    Constructor<PolymerElement&RouteObserverMixinInterface&
                NetworkListenerBehaviorInterface>;

export class ApnSubpageElement extends ApnSubpageElementBase {
  static get is() {
    return 'apn-subpage' as const;
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      isNumCustomApnsLimitReached: {
        type: Boolean,
        notify: true,
        value: false,
        computed: 'computeIsNumCustomApnsLimitReached_(managedProperties_)',
      },

      shouldDisallowApnModification: {
        type: Boolean,
        value: false,
      },

      /** The GUID of the network to display details for. */
      guid_: String,

      managedProperties_: {
        type: Object,
      },

      deviceState_: {
        type: Object,
        value: null,
      },
    };
  }

  isNumCustomApnsLimitReached: boolean;
  shouldDisallowApnModification: boolean;
  private deviceState_: OncMojo.DeviceStateProperties|null;
  private guid_: string;
  private managedProperties_: ManagedProperties|undefined;
  private networkConfig_: CrosNetworkConfigInterface;

  constructor() {
    super();
    this.networkConfig_ =
        MojoInterfaceProviderImpl.getInstance().getMojoServiceRemote();
  }

  close(): void {
    // If the page is already closed, return early to avoid navigating backward
    // erroneously.
    if (!this.guid_) {
      return;
    }

    this.guid_ = '';
    this.managedProperties_ = undefined;
    this.deviceState_ = null;

    // Only navigate backwards if this is page is the current route.
    if (Router.getInstance().currentRoute === routes.APN) {
      Router.getInstance().navigateToPreviousRoute();
    }
  }

  override currentRouteChanged(route: Route): void {
    if (route !== routes.APN) {
      return;
    }

    const queryParams = Router.getInstance().getQueryParameters();
    const guid = queryParams.get('guid') || '';
    if (!guid) {
      console.warn('No guid specified for page:' + route);
      Router.getInstance().navigateToPreviousRoute();
      return;
    }

    this.guid_ = guid;
    // Set default properties until they are loaded.
    this.deviceState_ = null;
    this.managedProperties_ = OncMojo.getDefaultManagedProperties(
        NetworkType.kCellular, this.guid_,
        OncMojo.getNetworkTypeString(NetworkType.kCellular));
    this.getNetworkDetails_();
  }

  override onNetworkStateChanged(network: NetworkStateProperties): void {
    if (!this.guid_ || !this.managedProperties_) {
      return;
    }
    if (network.guid === this.guid_) {
      this.getNetworkDetails_();
    }
  }

  override onDeviceStateListChanged(): void {
    if (!this.guid_ || !this.managedProperties_) {
      return;
    }
    this.getDeviceState_();
  }

  /**
   * Helper method that can be used by parent elements to open the APN
   * creation dialog.
   */
  openApnDetailDialogInCreateMode(): void {
    assert(this.guid_);
    this.$.apnList.openApnDetailDialogInCreateMode();
  }

  /**
   * Helper method that can be used by parent elements to open the APN
   * selection dialog.
   */
  openApnSelectionDialog(): void {
    assert(this.guid_);
    this.$.apnList.openApnSelectionDialog();
  }

  private async getNetworkDetails_(): Promise<void> {
    assert(this.guid_);

    const response = await this.networkConfig_.getManagedProperties(this.guid_);
    // Details page was closed while request was in progress, ignore the
    // result.
    if (!this.guid_) {
      return;
    }

    if (!response.result) {
      // Close the page if the network was removed and no longer exists.
      this.close();
      return;
    }

    if (!this.isCellular_(response.result)) {
      // Close the page if there are no cellular properties.
      this.close();
      return;
    }

    this.managedProperties_ = response.result;

    if (!this.deviceState_) {
      this.getDeviceState_();
    }
  }

  private async getDeviceState_(): Promise<void> {
    if (!this.isCellular_(this.managedProperties_)) {
      return;
    }
    const type = this.managedProperties_!.type;
    const response = await this.networkConfig_.getDeviceStateList();
    // If there is no GUID, the page was closed between requesting the device
    // state and receiving it. If this occurs, there is no need to process the
    // response. Note that if this subpage is reopened later, we'll request
    // this data again.
    if (!this.guid_) {
      return;
    }

    const {deviceState, shouldGetNetworkDetails} =
        processDeviceState(type, response.result, this.deviceState_);

    this.deviceState_ = deviceState;
    if (shouldGetNetworkDetails) {
      this.getNetworkDetails_();
    }
  }

  private isCellular_(managedProperties: ManagedProperties|undefined): boolean {
    return !!managedProperties &&
        managedProperties.type === NetworkType.kCellular;
  }

  private computeIsNumCustomApnsLimitReached_(): boolean {
    return this.isCellular_(this.managedProperties_) &&
        !!this.managedProperties_!.typeProperties.cellular!.customApnList &&
        this.managedProperties_!.typeProperties.cellular!.customApnList
            .length >= MAX_NUM_CUSTOM_APNS;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [ApnSubpageElement.is]: ApnSubpageElement;
  }
}

customElements.define(ApnSubpageElement.is, ApnSubpageElement);
