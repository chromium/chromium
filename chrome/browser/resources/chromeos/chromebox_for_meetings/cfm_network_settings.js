// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_components/chromeos/network/network_select.js';
import 'chrome://resources/cr_components/chromeos/network/network_list.js';
import 'chrome://resources/js/load_time_data.m.js';
import 'chrome://resources/polymer/v3_0/iron-iconset-svg/iron-iconset-svg.js';
import './strings.m.js';

import {MojoInterfaceProviderImpl} from 'chrome://resources/cr_components/chromeos/network/mojo_interface_provider.js';
import {NetworkList} from 'chrome://resources/cr_components/chromeos/network/network_list_types.js';
import {OncMojo} from 'chrome://resources/cr_components/chromeos/network/onc_mojo.js';
import {$} from 'chrome://resources/js/util.m.js';
import {html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {CfmNetworkSettingsBrowserProxy, CfmNetworkSettingsBrowserProxyImpl} from './cfm_network_settings_browser_proxy.js';

/**
 * Data sent with event when custom item is selected.
 * @typedef {{onTap: !function(CfmNetworkSettingsBrowserProxy)}}
 */
let networkCustomItemCustomData;

/**
 * @param {!OncMojo.NetworkStateProperties} networkState
 */
function shouldShowNetworkDetails(networkState) {
  const mojom = chromeos.networkConfig.mojom;
  return OncMojo.connectionStateIsConnected(networkState.connectionState) ||
      networkState.connectionState == mojom.ConnectionStateType.kConnecting ||
      (networkState.type == mojom.NetworkType.kCellular);
}

/**
 * Customized network settings menu for display in a dialog on CFM.
 * @polymer
 */
export class CfmNetworkSettings extends PolymerElement {
  static get is() {
    return 'cfm-network-settings';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      customItems_: {
        type: Array,
        notify: false,
        readOnly: true,
      },
    };
  }

  constructor() {
    super();

    /** @private {?CfmNetworkSettingsBrowserProxy} */
    this.browserProxy_ = null;
  }

  /** @override */
  connectedCallback() {
    super.connectedCallback();
    this.browserProxy_ = CfmNetworkSettingsBrowserProxyImpl.getInstance();
  }

  /**
   * Handles tap on a network entry in networks list.
   * @param {!CustomEvent<!OncMojo.NetworkStateProperties>} e
   * @private
   */
  onNetworkItemSelected_(e) {
    const mojom = chromeos.networkConfig.mojom;
    const networkState = e.detail;
    const guid = networkState.guid;

    if (shouldShowNetworkDetails(networkState)) {
      this.browserProxy_.showNetworkDetails(guid);
      return;
    }

    if (!networkState.connectable || networkState.errorState) {
      this.browserProxy_.showNetworkConfig(guid);
      return;
    }

    const networkConfig =
        MojoInterfaceProviderImpl.getInstance().getMojoServiceRemote();
    networkConfig.startConnect(guid).then(response => {
      switch (response.result) {
        case mojom.StartConnectResult.kSuccess:
          return;
        case mojom.StartConnectResult.kInvalidGuid:
        case mojom.StartConnectResult.kInvalidState:
        case mojom.StartConnectResult.kCanceled:
          return;
        case mojom.StartConnectResult.kNotConfigured:
          if (!OncMojo.networkTypeIsMobile(networkState.type)) {
            this.browserProxy_.showNetworkConfig(guid);
          } else {
            console.error('Cellular network is not configured: ' + guid);
          }
          return;
        case mojom.StartConnectResult.kBlocked:
        case mojom.StartConnectResult.kUnknown:
          console.error(
              'startConnect failed for: ' + guid + ': ' + response.message);
          return;
      }
    });
  }

  /**
   * The list of custom items to display after the list of networks.
   * See NetworkList for details.
   * @return {!Array<NetworkList.CustomItemState>}
   * @private
   */
  get customItems_() {
    return [
      {
        customItemType: NetworkList.CustomItemType.OOBE,
        customItemName: 'addWiFiListItemName',
        polymerIcon: 'cr:add',
        showBeforeNetworksList: false,
        customData: {onTap: proxy => proxy.showAddWifi()},
      },
      {
        customItemType: NetworkList.CustomItemType.OOBE,
        customItemName: 'proxySettingsListItemName',
        polymerIcon: 'cr:info-outline',
        showBeforeNetworksList: false,
        customData: {onTap: proxy => proxy.showNetworkDetails('')},
      },
      {
        customItemType: NetworkList.CustomItemType.OOBE,
        customItemName: 'manageCertsListItemName',
        polymerIcon: 'cfm-custom:manage-certs',
        showBeforeNetworksList: false,
        customData: {onTap: proxy => proxy.showManageCerts()},
      },
    ];
  }

  /**
   * Handles tap on a custom item from the networks list.
   * @param {!CustomEvent<{customData:!networkCustomItemCustomData}>} event
   * @private
   */
  onCustomItemSelected_(event) {
    const itemState = event.detail;
    itemState.customData.onTap(this.browserProxy_);
  }
}

customElements.define(CfmNetworkSettings.is, CfmNetworkSettings);
