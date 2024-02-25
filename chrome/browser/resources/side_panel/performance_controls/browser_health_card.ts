// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://performance-side-panel.top-chrome/shared/sp_heading.js';
import 'chrome://performance-side-panel.top-chrome/shared/sp_shared_style.css.js';
import '../strings.m.js';

import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './browser_health_card.html.js';

export interface BrowserHealthCardElement {
  $: {};
}

export class BrowserHealthCardElement extends PolymerElement {
  static get is() {
    return 'browser-health-card';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      isPerformanceCPUInterventionEnabled: {
        readOnly: true,
        type: Boolean,
        value: () =>
            loadTimeData.getBoolean('isPerformanceCPUInterventionEnabled'),
      },
      isPerformanceMemoryInterventionEnabled: {
        readOnly: true,
        type: Boolean,
        value: () =>
            loadTimeData.getBoolean('isPerformanceMemoryInterventionEnabled'),
      },
      hidden: {
        type: Boolean,
        reflectToAttribute: true,
        computed: 'computeHidden()',
      },
    };
  }

  private isPerformanceCPUInterventionEnabled: boolean;
  private isPerformanceMemoryInterventionEnabled: boolean;

  private computeHidden() {
    return !this.isPerformanceCPUInterventionEnabled &&
        !this.isPerformanceMemoryInterventionEnabled;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'browser-health-card': BrowserHealthCardElement;
  }
}
customElements.define(BrowserHealthCardElement.is, BrowserHealthCardElement);
