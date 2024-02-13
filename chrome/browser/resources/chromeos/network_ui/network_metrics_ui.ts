// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/ash/common/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/ash/common/cr_elements/cr_radio_button/cr_radio_button.js';
import 'chrome://resources/ash/common/cr_elements/cr_shared_style.css.js';

import {I18nMixin} from 'chrome://resources/ash/common/cr_elements/i18n_mixin.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './network_metrics_ui.html.js';
import {NetworkUiBrowserProxy, NetworkUiBrowserProxyImpl} from './network_ui_browser_proxy.js';
import {uPlot} from './third_party/uPlot.iife.min.js';

/**
 * @fileoverview
 * Polymer element for UI controlling the WiFi performance
 * metrics and their values.
 */

const NetworkMetricsUiElementBase = I18nMixin(PolymerElement);

class NetworkMetricsUiElement extends NetworkMetricsUiElementBase {
  static get is() {
    return 'network-metrics-ui' as const;
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      /**
       * Circular buffer of WiFi.SignalStrengthRssi
       * values from shill used for rendering graph.
       */
      rssiValues_: {
        type: Array,
        value: () => [],
      },

      minRssi_: {
        type: Number,
        value: -100,
      },

      maxRssi_: {
        type: Number,
        value: -25,
      },

      /**
       * Circular buffer of data extraction times used for rendering graph.
       */
      timeValues_: {
        type: Array,
        value: () => [],
      },

      running_: {
        type: Boolean,
        value: false,
      },

      graphRendered_: {
        type: Boolean,
        value: false,
      },

      /**
       * Milliseconds delay between extraction of data.
       */
      delay_: {
        type: Number,
        value: 500,
      },

      /**
       * Max data points to track in circular buffer.
       */
      dataCap_: {
        type: Number,
        value: 100,
      },
    };
  }

  private rssiValues_: number[];
  private minRssi_: number;
  private maxRssi_: number;
  private timeValues_: number[];
  private running_: boolean;
  private graphRendered_: boolean;
  private delay_: number;
  private dataCap_: number;

  private browserProxy_: NetworkUiBrowserProxy =
      NetworkUiBrowserProxyImpl.getInstance();

  private start_() {
    this.running_ = true;
  }

  private stop_() {
    this.running_ = false;
  }

  private decreaseDelay_() {
    const minDelay = 1000 / 8;  // 8Hz
    if (this.delay_ > minDelay) {
      this.delay_ /= 2;
    }
  }

  private increaseDelay_() {
    this.delay_ *= 2;
  }

  /**
   * Requests first WiFi's properties and updates metric arrays
   * when response contains the network information.
   */
  private async updateMetrics_() {
    const response = await this.browserProxy_.getFirstWifiNetworkProperties();
    if (response.length <= 0) {
      return;
    }
    const properties = response[0];
    this.updateRssi_(properties['WiFi.SignalStrengthRssi']);
    this.updateTime_();
  }

  /**
   * Updates Rssi array with extracted signal value.
   */
  private updateRssi_(data: number) {
    if (this.rssiValues_.length >= this.dataCap_) {
      this.rssiValues_.shift();
    }
    this.rssiValues_.push(data);
  }

  /**
   * Updates time array with current time value.
   */
  private updateTime_() {
    const currDate = new Date();
    if (this.timeValues_.length > this.dataCap_) {
      this.timeValues_.shift();
    }
    this.timeValues_.push(currDate.getTime() / 1000);
  }

  /**
   * Updates metrics and creates nested array series required
   * as input for the uPlot graph.
   */
  private getMetrics_(): uPlot.AlignedData {
    this.updateMetrics_();
    return [
      this.timeValues_,
      this.rssiValues_,
    ];
  }

  /**
   * Renders uPlot graph and initiates asynchronous loop
   * to keep updating with new values.
   */
  private renderGraph_() {
    if (!this.graphRendered_) {
      const graph = this.makeChart_(this.getMetrics_());
      this.loop_(graph);
      this.graphRendered_ = true;
    }
  }

  private wait_(ms: number): Promise<void> {
    return new Promise(resolve => setTimeout(resolve, ms));
  }

  /**
   * Repeatedly updates the uPlot graph with new data while the
   * running state is active. Time between updates is determined by
   * the delay property.
   */
  private async loop_(graph: uPlot) {
    while (true) {
      if (this.running_) {
        const updatedData = this.getMetrics_();
        graph.setData(updatedData);
      }
      await this.wait_(this.delay_);
    }
  }

  /**
   * Handles all uPlot functionality.
   */
  private makeChart_(data: uPlot.AlignedData): uPlot {
    const opts: uPlot.Options = {
      title: 'Rssi vs Time',
      width: window.innerWidth * .9,
      height: window.innerHeight * .667,
      scales: {
        x: {time: true},
        y: {
          auto: false,
          range: [this.minRssi_, this.maxRssi_],
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
        opts, data, this.shadowRoot!.getElementById('metrics-graph')!);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [NetworkMetricsUiElement.is]: NetworkMetricsUiElement;
  }
}

customElements.define(NetworkMetricsUiElement.is, NetworkMetricsUiElement);
