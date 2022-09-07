// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './cellular_info.js';
import './diagnostics_shared_css.js';
import './ethernet_info.js';
import './wifi_info.js';

import {html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {Network, NetworkType} from './diagnostics_types.js';

/**
 * @fileoverview
 * 'network-info' is responsible for displaying specialized data points for a
 * supported network type (Ethernet, WiFi, Cellular).
 */
Polymer({
  is: 'network-info',

  _template: html`{__html_template__}`,

  properties: {
    /** @type {!Network} */
    network: {
      type: Object,
    },
  },

  /**
   * @protected
   * @return {boolean}
   */
  isWifiNetwork_() {
    return this.network.type === NetworkType.kWiFi;
  },

  /**
   * @protected
   * @return {boolean}
   */
  isCellularNetwork_() {
    return this.network.type === NetworkType.kCellular;
  },

  /**
   * @protected
   * @return {boolean}
   */
  isEthernetNetwork_() {
    return this.network.type === NetworkType.kEthernet;
  },
});
