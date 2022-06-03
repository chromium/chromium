// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * Polymer element lock screen network selection UI.
 */

import 'chrome://resources/cr_elements/cr_dialog/cr_dialog.m.js';
import 'chrome://resources/cr_elements/icons.m.js';
import 'chrome://resources/cr_components/chromeos/network/network_select.m.js';
import 'chrome://resources/cr_elements/cr_button/cr_button.m.js';
import './strings.m.js';

import {OncMojo} from 'chrome://resources/cr_components/chromeos/network/onc_mojo.m.js';
import {html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

Polymer({
  is: 'lock-screen-network-ui',

  /** @type {?chromeos.networkConfig.mojom.CrosNetworkConfigRemote} */
  networkConfig_: null,

  /** @override */
  attached() {
    this.networkConfig_ =
        chromeos.networkConfig.mojom.CrosNetworkConfig.getRemote();

    const select = this.$$('network-select');
    select.customItems = [
      {
        customItemName: 'addWiFiListItemName',
        polymerIcon: 'cr:add',
        customData: 'WiFi'
      },
    ];
  },

  /**
   * Handles clicks on network items in the <network-select> element by
   * attempting a connection to the selected network or requesting a password
   * if the network requires a password.
   * @param {!Event<!OncMojo.NetworkStateProperties>} event
   * @private
   */
  onNetworkItemSelected_(event) {
    const networkState = event.detail;

    // If the network is already connected, show network details.
    if (OncMojo.connectionStateIsConnected(networkState.connectionState)) {
      chrome.send('showNetworkDetails', [networkState.guid]);
      return;
    }

    // If the network is not connectable, show a configuration dialog.
    if (networkState.connectable === false || networkState.errorState) {
      chrome.send('showNetworkConfig', [networkState.guid]);
      return;
    }

    // Otherwise, connect.
    this.networkConfig_.startConnect(networkState.guid).then(response => {
      if (response.result ==
          chromeos.networkConfig.mojom.StartConnectResult.kSuccess) {
        return;
      }
      chrome.send('showNetworkConfig', [networkState.guid]);
    });
  },

  /**
   * @param {!Event<!{detail:{customData: string}}>} event
   * @private
   */
  onCustomItemSelected_(event) {
    chrome.send('addNetwork', [event.detail.customData]);
  },

  /** @private */
  onCloseTap_() {
    chrome.send('dialogClose');
  },

});
