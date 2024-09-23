// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {MdSelectDemoElement} from './md_select_demo.js';

export function getHtml(this: MdSelectDemoElement) {
  return html`
<h1>Select menus</h1>
<div class="demos">
  <div class="row center">
    <label id="label">Select an option</label>
    <select id="select" class="md-select" value="${this.selectedOption_}"
        @change="${this.onSelectValueChanged_}"
        aria-labelledby="label">
      <option value="one">Option 1</option>
      <option value="two">Option 2</option>
      <option value="three">Option 3</option>
      <option value="four">Option 4</option>
      <option value="five">Option 5</option>
    </select>
  </div>

  <div>Selected value: ${this.selectedOption_}</div>

  <div class="row center">
    <label id="disabled-label">Select an option</label>
    <select class="md-select" disabled aria-labelledby="disabled-label">
      <option>Disabled option 1</option>
      <option>Disabled option 2</option>
      <option selected>Disabled option 3</option>
    </select>
  </div>
</div>`;
}
