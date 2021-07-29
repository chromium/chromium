// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './data_point.js';
import './diagnostics_fonts_css.js';
import './diagnostics_shared_css.js';

import {I18nBehavior} from 'chrome://resources/js/i18n_behavior.m.js';
import {html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {Network} from './diagnostics_types.js';
import {convertFrequencyToChannel, getSubnetMaskFromRoutingPrefix} from './diagnostics_utils.js';

/**
 * @fileoverview
 * 'wifi-info' is responsible for displaying data points related
 * to a WiFi network.
 */
Polymer({
  is: 'wifi-info',

  _template: html`{__html_template__}`,

  behaviors: [I18nBehavior],

  properties: {
    /** @type {!Network} */
    network: {
      type: Object,
    },

    /**
     * @protected
     * @type {string}
     */
    signalStrength_: {
      type: String,
      computed:
          'computeSignalStrength_(network.typeProperties.wifi.signalStrength)'
    }
  },

  /**
   * Builds channel text based frequency conversion. If value of frequency is
   * undefined or zero, then an empty string is returned. Otherwise, if value
   * returned by conversion function is null, then we display a question mark
   * for channel value. Frequency used to calculate converted from MHz to  GHz
   * for display.
   * @protected
   * @param {number} frequency Given in MHz.
   * @return {string}
   */
  getChannelDescription_(frequency) {
    if (!frequency || frequency === 0) {
      return '';
    }
    const channel = convertFrequencyToChannel(frequency);
    const ghz = (frequency / 1000).toFixed(3);
    return `${channel || '?'} (${ghz} GHz)`;
  },

  /**
   * @return {string}
   */
  computeSignalStrength_() {
    if (this.network.typeProperties && this.network.typeProperties.wifi &&
        this.network.typeProperties.wifi.signalStrength > 0) {
      return `${this.network.typeProperties.wifi.signalStrength}`;
    }
    return '';
  }
});
