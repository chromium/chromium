// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {CrTabsDemoElement} from './cr_tabs_demo.js';

export function getHtml(this: CrTabsDemoElement) {
  return html`
<h1>cr-tabs</h1>
<div class="demos">
  <cr-tabs id="tabs" .tabNames="${this.tabNames_}"
      .selected="${this.selectedTabIndex_}"
      @selected-changed="${this.onSelectedTabIndexChanged_}">
  </cr-tabs>
  <cr-page-selector .selected="${this.selectedTabIndex_}">
    ${this.tabNames_.map(tabName => html`
      <div class="tab-contents">${tabName} contents</div>
    `)}
  </cr-page-selector>
</div>

<div class="demos">
  <div class="row">
    <cr-button @click="${this.onAddClick_}">Add</cr-button>
    <cr-button @click="${this.onAddAt1Click_}">Add at 1</cr-button>
    <cr-button @click="${this.onSelectAt1Click_}">Select at 1</cr-button>
  </div>
</div>

<div>
  Tab Count: ${this.tabNames_.length},
  Selected Tab: ${this.selectedTabIndex_}
</div>`;
}
