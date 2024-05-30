// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {CrCheckboxDemoElement} from './cr_checkbox_demo.js';

export function getHtml(this: CrCheckboxDemoElement) {
  return html`
<h1>cr-checkbox</h1>
<div class="demos">
  <cr-checkbox ?checked="${this.myValue_}"
      @checked-changed="${this.onCheckedChanged_}">Checkbox</cr-checkbox>
  <div>Above checkbox is checked? ${this.myValue_}</div>

  <cr-checkbox checked>Checkbox</cr-checkbox>
  <cr-checkbox checked class="label-first">
    Checkbox with the label showing first
  </cr-checkbox>
  <cr-checkbox disabled>Disabled checkbox</cr-checkbox>
  <cr-checkbox checked disabled>Disabled checked checkbox</cr-checkbox>
</div>`;
}
