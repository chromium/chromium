// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '../strings.m.js';
import './ml_calculator.js';
import './ml_table.js';

import {CustomElement} from 'chrome://resources/js/custom_element.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';

import {Signals} from '../omnibox.mojom-webui.js';

import {MlBrowserProxy} from './ml_browser_proxy.js';
import {MlCalculatorElement} from './ml_calculator.js';
import {MlTableElement} from './ml_table.js';
// @ts-ignore:next-line
import sheet from './ml_ui.css' assert {type : 'css'};
import {getTemplate} from './ml_ui.html.js';

declare global {
  interface HTMLElementEventMap {
    'match-selected': CustomEvent<Signals>;
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

    this.getRequiredElement('#ml-sync-batch-url-scoring-disabled-warning')
        .hidden = loadTimeData.getBoolean('isMlSyncBatchUrlScoringEnabled');
    mlCalculator.mlBrowserProxy = this.mlBrowserProxy;
    this.mlBrowserProxy.getModelVersion().then(
        version => mlCalculator.version = version);
    this.mlBrowserProxy.addResponseListener(
        (...args) => mlTable.onNewResponse(...args));
    mlTable.addEventListener(
        'match-selected', ({detail}) => mlCalculator.signals = detail);
  }
}

customElements.define('ml-ui', MlUiElement);
