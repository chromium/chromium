// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './cellular_info.js';
import './diagnostics_shared.css.js';
import './ethernet_info.js';
import './wifi_info.js';

import {PolymerElementProperties} from 'chrome://resources/polymer/v3_0/polymer/interfaces.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {Network, NetworkType} from './network_health_provider.mojom-webui.js';
import {getTemplate} from './network_info.html.js';

/**
 * @fileoverview
 * 'network-info' is responsible for displaying specialized data points for a
 * supported network type (Ethernet, WiFi, Cellular).
 */

export class NetworkInfoElement extends PolymerElement {
  static get is(): 'network-info' {
    return 'network-info' as const;
  }

  static get template(): HTMLTemplateElement {
    return getTemplate();
  }

  static get properties(): PolymerElementProperties {
    return {
      /** @type {!Network} */
      network: {
        type: Object,
      },

    };
  }

  network: Network;

  protected isWifiNetwork(): boolean {
    return this.network.type === NetworkType.kWiFi;
  }

  protected isCellularNetwork(): boolean {
    return this.network.type === NetworkType.kCellular;
  }

  protected isEthernetNetwork(): boolean {
    return this.network.type === NetworkType.kEthernet;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [NetworkInfoElement.is]: NetworkInfoElement;
  }
}

customElements.define(NetworkInfoElement.is, NetworkInfoElement);
