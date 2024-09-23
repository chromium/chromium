// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * Polymer element lock screen network selection UI.
 */

import 'chrome://resources/ash/common/cr_elements/cr_dialog/cr_dialog.js';
import 'chrome://resources/ash/common/cr_elements/icons.html.js';
import 'chrome://resources/ash/common/network/network_select.js';
import 'chrome://resources/ash/common/cr_elements/cr_button/cr_button.js';
import './strings.m.js';

import {OncMojo} from 'chrome://resources/ash/common/network/onc_mojo.js';
import {CrosNetworkConfig, CrosNetworkConfigRemote, StartConnectResult} from 'chrome://resources/mojo/chromeos/services/network_config/public/mojom/cros_network_config.mojom-webui.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

class LockScreenNetworkUi extends PolymerElement {
  static get is() {
    return 'lock-screen-network-ui' as const;
  }

  private networkConfig: CrosNetworkConfigRemote =
      CrosNetworkConfig.getRemote();

  override connectedCallback() {
    super.connectedCallback();
    const select = this.shadowRoot!.querySelector('network-select');
    select!.customItems = [
      {
        customItemName: 'addWiFiListItemName',
        polymerIcon: 'cr:add',
        customData: 'WiFi',
      },
    ];
  }

  override ready() {
    super.ready();
    chrome.send('initialize');
  }

  /**
   * Handles clicks on network items in the <network-select> element by
   * attempting a connection to the selected network or requesting a password
   * if the network requires a password.
   */
  private onNetworkItemSelected(
      event: CustomEvent<OncMojo.NetworkStateProperties>) {
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
    this.networkConfig.startConnect(networkState.guid).then(response => {
      if (response.result === StartConnectResult.kSuccess) {
        return;
      }
      chrome.send('showNetworkConfig', [networkState.guid]);
    });
  }

  private onCustomItemSelected(event: CustomEvent<{customData: string}>) {
    chrome.send('addNetwork', [event.detail.customData]);
  }

  private onCloseClick() {
    chrome.send('dialogClose');
  }

}

customElements.define(LockScreenNetworkUi.is, LockScreenNetworkUi);
