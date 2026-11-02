// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {SpeedPageElement} from './speed_page.js';

export function getHtml(this: SpeedPageElement) {
  return html`<!--_html_template_start_-->
<div class="cr-row">
  <div class="cpu-performance-override-header cr-padded-text">
    <div>$i18n{cpuPerformanceOverrideTitle}</div>
    <div class="cr-secondary-text">
      $i18n{cpuPerformanceOverrideDescription}
    </div>
  </div>
</div>
<div class="cr-row continuation">
  <div id="cpuPerformanceInfo" class="cr-padded-text">
    ${this.cpuPerformanceInfo_ ? html`
      <div class="cr-secondary-text">${this.cpuPerformanceModelLabel_}</div>
      <div class="cr-secondary-text">
        ${this.getCpuPerformanceNominalTierLabel_()}
      </div>
    ` : ''}
  </div>
  <settings-dropdown-menu id="cpuPerformanceOverrideDropdown"
      pref-key="cpu_performance_tier_override"
      .menuOptions="${this.cpuPerformanceTierOptions_}">
  </settings-dropdown-menu>
</div>
<!--_html_template_end_-->`;
}
