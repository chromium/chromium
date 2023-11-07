// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '../strings.m.js';
import './ml_calculator.js';
import './ml_table.js';
import './ml_chart.js';

import {CustomElement} from 'chrome://resources/js/custom_element.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';

import {Signals} from '../omnibox.mojom-webui.js';

import {MlBrowserProxy} from './ml_browser_proxy.js';
import {MlCalculatorElement} from './ml_calculator.js';
import {MlChartElement} from './ml_chart.js';
import {MlTableElement} from './ml_table.js';
// @ts-ignore:next-line
import sheet from './ml_ui.css' assert {type : 'css'};
import {getTemplate} from './ml_ui.html.js';

declare global {
  interface HTMLElementEventMap {
    'match-selected': CustomEvent<Signals>;
    'copied': CustomEvent<Promise<void>>;
  }
}

export class MlUiElement extends CustomElement {
  private readonly mlBrowserProxy = new MlBrowserProxy();

  static override get template() {
    return getTemplate();
  }

  constructor() {
    super();
    this.shadowRoot!.adoptedStyleSheets = [sheet];
  }

  connectedCallback() {
    const mlCalculator =
        this.getRequiredElement<MlCalculatorElement>('ml-calculator');
    const mlTable = this.getRequiredElement<MlTableElement>('ml-table');
    const mlChart = this.getRequiredElement<MlChartElement>('ml-chart');

    this.getRequiredElement('#ml-sync-batch-url-scoring-disabled-warning')
        .hidden = loadTimeData.getBoolean('isMlSyncBatchUrlScoringEnabled');
    [mlCalculator, mlTable].forEach(
        el => el.addEventListener('copied', ({detail}) => {
          detail.then(() => 'Copied!')
              .catch(e => {
                console.error('Failed to copy to clipboard:', e);
                return 'Failed to copy :(';
              })
              .then(text => {
                const notification =
                    this.getRequiredElement('#copied-notification');
                notification.textContent = text;
                notification.classList.remove('fade-out');
                // Querying `offsetHeight` forces a page reflow; otherwise,
                // the classList changes above and below would be deduped.
                notification.offsetHeight;
                notification.classList.add('fade-out');
              });
        }));
    mlCalculator.addEventListener(
        'updated', () => mlChart.signals = mlCalculator.signals);
    mlTable.addEventListener(
        'match-selected', ({detail}) => mlCalculator.signals = detail);

    this.mlBrowserProxy.modelVersion.then(() => {
      // ML model was loaded.
      mlCalculator.mlBrowserProxy = this.mlBrowserProxy;
      mlTable.mlBrowserProxy = this.mlBrowserProxy;
      mlChart.mlBrowserProxy = this.mlBrowserProxy;
    });
  }
}

customElements.define('ml-ui', MlUiElement);
