// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {CrToggleDemoElement} from './cr_toggle_demo.js';

export function getHtml(this: CrToggleDemoElement) {
  return html`
<h1>cr-toggle</h1>
<div class="demos">
  <div class="row">
    <cr-toggle
        aria-label="Label for toggle"
        ?checked="${this.checked_}"
        @checked-changed="${this.onCheckedChanged_}">
    </cr-toggle>
    <span aria-hidden="true">Label for toggle</span>
    <span>
      Checked? ${this.checked_}
    </span>
  </div>

  <div class="row">
    <cr-toggle aria-label="Disabled checked toggle" disabled checked>
    </cr-toggle>
    <span aria-hidden="true">Disabled checked toggle</span>
  </div>

  <div class="row">
    <cr-toggle aria-label="Disabled unchecked toggle" disabled>
    </cr-toggle>
    <span aria-hidden="true">Disabled unchecked toggle</span>
  </div>
</div>`;
}
