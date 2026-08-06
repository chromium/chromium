// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';
import type {PerformancePageIndexElement} from './performance_page_index.js';

export function getHtml(this: PerformancePageIndexElement) {
  return html`<!--_html_template_start_-->
<cr-view-manager id="viewManager" class="cr-centered-card-container"
    ?show-all="${this.shouldShowAll}">
  <settings-performance-page slot="view" id="performance">
  </settings-performance-page>

  <settings-memory-page slot="view" id="memory"></settings-memory-page>

  <settings-battery-page slot="view" id="battery"
      ?hidden="${!this.showBatterySettings_}">
  </settings-battery-page>

  <settings-speed-page slot="view" id="speed"></settings-speed-page>
</cr-view-manager>
<!--_html_template_end_-->`;
}
