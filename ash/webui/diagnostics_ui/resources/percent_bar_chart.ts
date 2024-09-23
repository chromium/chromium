// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/ash/common/cr_elements/icons.html.js';
import 'chrome://resources/polymer/v3_0/iron-icon/iron-icon.js';
import 'chrome://resources/polymer/v3_0/paper-progress/paper-progress.js';
import './diagnostics_shared.css.js';
import './strings.m.js';

import {PolymerElementProperties} from 'chrome://resources/polymer/v3_0/polymer/interfaces.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './percent_bar_chart.html.js';

/**
 * @fileoverview
 * 'percent-bar-chart' is a styling wrapper for paper-progress used to display a
 * percentage based bar chart.
 */

export class PercentBarChartElement extends PolymerElement {
  static get is(): 'percent-bar-chart' {
    return 'percent-bar-chart' as const;
  }

  static get template(): HTMLTemplateElement {
    return getTemplate();
  }

  static get properties(): PolymerElementProperties {
    return {
      header: {
        type: String,
      },

      value: {
        type: Number,
        value: 0,
      },

      max: {
        type: Number,
        value: 100,
      },

    };
  }

  header: string;
  value: number;
  max: number;

  /**
   * Get adjusted value clamped to max value. paper-progress breaks for a while
   * when value is set higher than max in certain cases (e.g. due to fetching of
   * max being resolved later).
   */
  protected getAdjustedValue(): number {
    return this.value <= this.max ? this.value : this.max;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [PercentBarChartElement.is]: PercentBarChartElement;
  }
}

customElements.define(PercentBarChartElement.is, PercentBarChartElement);
