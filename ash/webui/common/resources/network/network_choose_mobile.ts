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

import {assert} from '//resources/js/assert.js';
import type {CrosNetworkConfigInterface, FoundNetworkProperties, ManagedProperties} from '//resources/mojo/chromeos/services/network_config/public/mojom/cros_network_config.mojom-webui.js';
import {ConnectionStateType, NetworkType} from '//resources/mojo/chromeos/services/network_config/public/mojom/network_types.mojom-webui.js';
import {microTask, PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {I18nMixin} from 'chrome://resources/ash/common/cr_elements/i18n_mixin.js';

import {MojoInterfaceProvider, MojoInterfaceProviderImpl} from './mojo_interface_provider.js';
import {getTemplate} from './network_choose_mobile.html.js';
import type {OncMojo} from './onc_mojo.js';

const NetworkChooseMobileElementBase = I18nMixin(PolymerElement);

export class NetworkChooseMobileElement extends NetworkChooseMobileElementBase {
  static get is() {
    return 'network-choose-mobile' as const;
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      deviceState: {
        type: Object,
        value: null,
      },

      disabled: {
        type: Boolean,
        value: false,
      },

      managedProperties: {
        type: Object,
        observer: 'managedPropertiesChanged_',
      },

      /**
       * The mojom.FoundNetworkProperties.networkId of the selected mobile
       * network.
       */
      selectedMobileNetworkId_: {
        type: String,
        value: '',
      },

      /**
       * Selectable list of mojom.FoundNetworkProperties dictionaries for the
       * UI.
       */
      mobileNetworkList_: {
        type: Array,
        value() {
          return [];
        },
      },
    };
  }

  deviceState: OncMojo.DeviceStateProperties|null;
  disabled: boolean;
  managedProperties: ManagedProperties|undefined;
  private mobileNetworkList_: FoundNetworkProperties[];
  private networkConfig_: CrosNetworkConfigInterface|null = null;
  private scanRequested_: boolean = false;
  private selectedMobileNetworkId_: string;

  override connectedCallback() {
    super.connectedCallback();

    this.scanRequested_ = false;
  }

  private getNetworkConfig_(): CrosNetworkConfigInterface {
    if (!this.networkConfig_) {
      this.networkConfig_ =
          MojoInterfaceProviderImpl.getInstance().getMojoServiceRemote();
    }
    return this.networkConfig_!;
  }

  private managedPropertiesChanged_(): void {
    assert(this.managedProperties);
    const cellular = this.managedProperties.typeProperties.cellular;
    assert(cellular);
    this.mobileNetworkList_ = cellular.foundNetworks || [];
    if (!this.mobileNetworkList_.length) {
      this.mobileNetworkList_ = [{
        status: '',
        networkId: 'none',
        technology: '',
        longName: this.i18n('networkCellularNoNetworks'),
        shortName: null,
      }];
    }
    // Set selectedMobileNetworkId_ after the dom-repeat has been stamped.
    microTask.run(() => {
      let selected = this.mobileNetworkList_.find((mobileNetwork) => {
        return mobileNetwork.status === 'current';
      });
      if (!selected) {
        selected = this.mobileNetworkList_[0];
      }
      this.selectedMobileNetworkId_ = selected.networkId;
    });
  }

  private getMobileNetworkIsDisabled_(foundNetwork: FoundNetworkProperties):
      boolean {
    return foundNetwork.status !== 'available' &&
        foundNetwork.status !== 'current';
  }

  private getEnableScanButton_(properties: ManagedProperties): boolean {
    return !this.disabled &&
        properties.connectionState === ConnectionStateType.kNotConnected &&
        !!this.deviceState && !this.deviceState.scanning;
  }

  private getEnableSelectNetwork_(properties: ManagedProperties): boolean {
    assert(properties.typeProperties.cellular);
    return (
        !this.disabled && !!this.deviceState && !this.deviceState.scanning &&
        properties.connectionState === ConnectionStateType.kNotConnected &&
        !!properties.typeProperties.cellular.foundNetworks &&
        properties.typeProperties.cellular.foundNetworks.length > 0);
  }

  private getSecondaryText_(properties: ManagedProperties): string {
    if (!properties) {
      return '';
    }
    if (this.deviceState?.scanning) {
      return this.i18n('networkCellularScanning');
    }
    if (this.scanRequested_) {
      return this.i18n('networkCellularScanCompleted');
    }
    if (properties.connectionState !== ConnectionStateType.kNotConnected) {
      return this.i18n('networkCellularScanConnectedHelp');
    }
    return '';
  }

  private getName_(foundNetwork: FoundNetworkProperties): string {
    return foundNetwork.longName || foundNetwork.shortName ||
        foundNetwork.networkId;
  }

  /**
   * Request a Cellular scan to populate the list of networks. This will trigger
   * a change to managedProperties when completed (if Cellular.FoundNetworks
   * changes).
   */
  private onScanTap_(): void {
    this.scanRequested_ = true;

    this.getNetworkConfig_().requestNetworkScan(NetworkType.kCellular);
  }

  private onChange_(event: Event): void {
    const target = event.target;
    assert(target instanceof HTMLSelectElement);
    if (!target.value || target.value === 'none') {
      return;
    }

    assert(this.managedProperties);
    this.getNetworkConfig_().selectCellularMobileNetwork(
        this.managedProperties.guid, target.value);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [NetworkChooseMobileElement.is]: NetworkChooseMobileElement;
  }
}

customElements.define(
    NetworkChooseMobileElement.is, NetworkChooseMobileElement);
