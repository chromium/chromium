// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {CrTooltipDemoElement} from './cr_tooltip_demo.js';

export function getHtml(this: CrTooltipDemoElement) {
  return html`
<h1>Automatic Tooltip (shows when mouse over target)</h1>
<div class="demos">
  <div class="target" id="target">This is a tooltip target</div>
  <cr-tooltip for="target" .position="${this.tooltipPosition_}"
      .offset="${this.tooltipOffset_}">
    <span>Tooltip text</span>
  </cr-tooltip>
</div>

<h1>Manual Mode Tooltip</h1>
<div class="demos">
  <div class="target" id="target1">Target 1</div>
  <div class="target" id="target2">Target 2</div>
  <cr-tooltip id="manualTooltip" manual-mode
      .position="${this.tooltipPosition_}"
      .offset="${this.tooltipOffset_}">
    <span>Tooltip text</span>
  </cr-tooltip>
  <button @click="${this.showAtTarget1_}">Show at Target 1</button>
  <button @click="${this.showAtTarget2_}">Show at Target 2</button>
  <button @click="${this.hide_}">Hide</button>
</div>

<h1>Customize tooltips</h1>
<div class="demos">
  <cr-input type="number" min="0" max="24" .value="${this.tooltipOffset_}"
      @input="${this.onTooltipOffsetInput_}" label="Tooltip offset (px)">
  </cr-input>
  <label>Tooltip position</label>
  <select .value="${this.tooltipPosition_}" class="md-select"
      @change="${this.onTooltipPositionChange_}">
    <option value="top">top</option>
    <option value="bottom">bottom</option>
    <option value="left">left</option>
    <option value="right">right</option>
  </select>
</div>`;
}
