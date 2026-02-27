// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import type {DataCollectorsElement} from './data_collectors.js';

export function getHtml(this: DataCollectorsElement) {
  // clang-format off
  return html`<!--html_template_start_-->
<h1 tabindex="0">${this.i18n('dataSelectionPageTitle')}</h1>

<cr-checkbox class="select-all-checkbox" id="selectAllCheckbox"
    ?checked="${this.allSelected_}"
    @checked-changed="${this.onAllSelectedCheckedChanged_}" tabindex="0">
  ${this.i18n('selectAll')}
</cr-checkbox>

<div class="data-collector-list">
  ${this.dataCollectors_.map((item, index) => html`
    <cr-checkbox class="data-collector-checkbox"
        ?checked="${item.isIncluded}" data-index="${index}"
        @checked-changed="${this.onDataCollectorCheckedChanged_}" tabindex="0">
      ${item.name}
    </cr-checkbox>
  `)}
</div>
  <!--html_template_end_-->`;
  // clang-format on
}
