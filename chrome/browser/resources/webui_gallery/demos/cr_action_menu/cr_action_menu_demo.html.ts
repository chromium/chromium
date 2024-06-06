// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {CrActionMenuDemoElement} from './cr_action_menu_demo.js';

export function getHtml(this: CrActionMenuDemoElement) {
  // clang-format off
  return html`
<h1>cr-action-menu</h1>

<h2>Typical action menu</h2>
<div class="demos">
  <cr-icon-button
      title="Show menu"
      aria-haspopup="menu"
      iron-icon="cr:more-vert"
      @click="${this.onShowAnchoredMenuClick_}">
  </cr-icon-button>
</div>

<h2>As context menu</h2>
<div class="demos" @contextmenu="${this.onContextMenu_}">
  Right-click anywhere in this area.
</div>

<h2>With custom min/max positioning</h2>
<div id="minMaxContainer" class="demos">
  Action menus opened from this area are automatically confined to this area

  <cr-icon-button class="min-max-anchor top-left"
      iron-icon="cr:add"
      @click="${this.onShowMinMaxMenu_}">
  </cr-icon-button>
  <cr-button class="min-max-anchor top-right"
      @click="${this.onShowMinMaxMenu_}">
    Open menu
  </cr-button>
  <cr-icon-button class="min-max-anchor bottom-left"
      iron-icon="cr20:menu"
      @click="${this.onShowMinMaxMenu_}">
  </cr-icon-button>
  <cr-button class="min-max-anchor bottom-right"
      @click="${this.onShowMinMaxMenu_}">
    Open menu
  </cr-icon-button>
</div>

<h2>Custom anchor alignment</h2>
<div id="anchorAlignmentDemoOptions">
  <div class="anchor-alignment-option">
    <label id="alignmentXLabel">x alignment</label>
    <select aria-labelledby="alignmentXLabel"
        .value="${this.customAlignmentX_}"
        @change="${this.onCustomAlignmentXChanged_}">
      ${this.alignmentOptions_.map(item => html`
        <option .value="${item}"
            ?selected="${this.isSelectedAlignment_(
                this.customAlignmentX_, item)}">
          ${item}
        </option>
      `)}
    </select>
  </div>

  <div class="anchor-alignment-option">
    <label id="alignmentYLabel">y alignment</label>
    <select aria-labelledby="alignmentYLabel"
        .value="${this.customAlignmentY_}"
        @change="${this.onCustomAlignmentYChanged_}">
      ${this.alignmentOptions_.map(item => html`
        <option .value="${item}"
            ?selected="${this.isSelectedAlignment_(
                this.customAlignmentY_, item)}">
          ${item}
        </option>
      `)}
    </select>
  </div>

  <cr-button @click="${this.onAnchorAlignmentDemoClick_}">
    Show at anchor
  </cr-button>
</div>

<div class="demos">
  <div id="anchorAlignmentDemo">
    Anchor box

    <span class="anchor-alignment-label center">center</span>
    <span class="anchor-alignment-label start-y">y start</span>
    <span class="anchor-alignment-label end-y">y end</span>
    <span class="anchor-alignment-label start-x">x start</span>
    <span class="anchor-alignment-label end-x">x end</span>
  </div>
</div>

<cr-action-menu id="menu">
  <button class="dropdown-item">Menu item 1</button>
  <button class="dropdown-item">Menu item 2</button>
  <hr>
  <button class="dropdown-item" disabled>Menu item 3</button>
</cr-action-menu>`;
  // clang-format on
}
