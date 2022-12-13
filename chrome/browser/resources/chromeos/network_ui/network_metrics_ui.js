// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_radio_button/cr_radio_button.js';
import 'chrome://resources/cr_elements/cr_shared_style.css.js';

import {I18nBehavior} from 'chrome://resources/ash/common/i18n_behavior.js';
import {Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './network_metrics_ui.html.js';
import {NetworkUIBrowserProxy, NetworkUIBrowserProxyImpl} from './network_ui_browser_proxy.js';
import {uPlot} from './third_party/uPlot.iife.min.js';

/**
 * @fileoverview
 * Polymer element for UI controlling the WiFi performance
 * metrics and their values.
 */

Polymer({
  is: 'network-metrics-ui',

  _template: getTemplate(),

  behaviors: [I18nBehavior],

  properties: {
    /**
     * Circular buffer of WiFi.SignalStrengthRssi
     * values from shill used for rendering graph.
     * @type {!Array<Number>}
     * @private
     */
    rssiValues_: {
      type: Array,
      value: [],
    },

    /** @private */
    minRssi_: {
      type: Number,
      value: -100,
    },

    /** @private */
    maxRssi_: {
      type: Number,
      value: -25,
    },

    /**
     * Circular buffer of data extraction times used for rendering graph.
     * @type {!Array<number>}
     * @private
     */
    timeValues_: {
      type: Array,
      value: [],
    },

    /** @private */
    running_: {
      type: Boolean,
      value: false,
    },

    /** @private */
    graphRendered_: {
      type: Boolean,
      value: false,
    },

    /**
     * Milliseconds delay between extraction of data.
     * @private
     */
    delay_: {
      type: Number,
      value: 500,
    },

    /**
     * Max data points to track in circular buffer.
     * @private
     */
    dataCap_: {
      type: Number,
      value: 100,
    },
  },

  /** @type {!NetworkUIBrowserProxy} */
  browserProxy_: NetworkUIBrowserProxyImpl.getInstance(),

  /** @private */
  start_() {
    this.running_ = true;
  },

  /** @private */
  stop_() {
    this.running_ = false;
  },

  /** @private */
  decreaseDelay_() {
    const minDelay = 1000 / 8; //8Hz
    if (this.delay_ > minDelay) {
      this.delay_ /= 2;
    }
  },

  /** @private */
  increaseDelay_() {
    this.delay_ *= 2;
  },

  /**
   * Requests first WiFi's properties and updates metric arrays
   * when response contains the network information.
   * @private
   */
  updateMetrics_() {
    this.browserProxy_.getFirstWifiNetworkProperties().then((response) => {
      if (response.length <= 0) {
        return;
      }
      const properties = response[0];
      this.updateRssi_(properties['WiFi.SignalStrengthRssi']);
      this.updateTime_();
    });
  },

  /**
   * Updates Rssi array with extracted signal value.
   * @param {Number} data: The new Rssi data point
   * @private
   */
  updateRssi_(data) {
    if (this.rssiValues_.length >= this.dataCap_) {
      this.rssiValues_.shift();
    }
    this.rssiValues_.push(data);
  },

  /**
   * Updates time array with current time value.
   * @private
   */
  updateTime_() {
    const currDate = new Date();
    if (this.timeValues_.length > this.dataCap_) {
      this.timeValues_.shift();
    }
    this.timeValues_.push(currDate.getTime() / 1000);
  },

  /**
   * Updates metrics and creates nested array series required
   * as input for the uPlot graph.
   * @return {!Array<Array<Number>>} A data nested array.
   * @private
   */
  getMetrics_() {
    this.updateMetrics_();
    const data = [];
    data.push(this.timeValues_);
    data.push(this.rssiValues_);
    return data;
  },

  /**
   * Renders uPlot graph and initiates asynchronous loop
   * to keep updating with new values.
   * @private
   */
  renderGraph_() {
    if (!this.graphRendered_) {
      const self = this;
      const graph = this.makeChart_(self, this.getMetrics_());
      this.loop_(self, graph);
      this.graphRendered_ = true;
    }
  },

  /**
   * @param {number} ms: The time in milliseconds for timeout
   * @return {!Promise} A promise to wait a set time
   * @private
   */
  wait_(ms) {
    return new Promise(resolve => setTimeout(resolve, ms));
  },

  /**
   * Repeatedly updates the uPlot graph with new data while the
   * running state is active. Time between updates is determined by
   * the delay property.
   * @param {!Object} polymerObj: The polymer parent object
   * @param {!Object} graph: The uPlot object
   * @private
   */
  async loop_(polymerObj, graph) {
    while (true) {
      if (polymerObj.running_) {
        const updatedData = polymerObj.getMetrics_();
        graph.setData(updatedData);
      }
      await this.wait_(polymerObj.delay_);
    }
  },

  /**
   * Handles all uPlot functionality.
   * @param {!Object} polymerObj: The polymer parent object
   * @param {!Array<Array<Number>>} data: The values to be rendered
   * @return {!Object} The uPlot object
   * @private
   */
  makeChart_(polymerObj, data) {
    const opts = {
      title: 'Rssi vs Time',
      width: window.innerWidth * .9,
      height: window.innerHeight * .667,
      scales: {
        x: {time: true},
        y: {
          auto: false,
          range: [polymerObj.minRssi_, polymerObj.maxRssi_],
        },
      },
      series: [
        {},
        {
          label: 'Rssi',
          stroke: 'red',
        },
      ],
    };
    return new uPlot(
        opts, data, polymerObj.shadowRoot.getElementById('metrics-graph'));
  },
});
