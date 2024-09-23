// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {CrRadioDemoElement} from './cr_radio_demo.js';

export function getHtml(this: CrRadioDemoElement) {
  return html`
<h1>cr-radio-group and cr-radio-button</h1>
<div class="demos">
  <cr-radio-group selected="${this.selectedRadioOption_}"
      @selected-changed="${this.onSelectedRadioOptionChanged_}">
    <cr-radio-button name="option1" label="Option 1"></cr-radio-button>
    <cr-radio-button name="option2" label="Option 2"></cr-radio-button>
    <cr-radio-button name="option3" label="Option 3">
      <div>With slotted content</div>
    </cr-radio-button>
    <cr-radio-button name="option4" label="Option 4" class="label-first">
      <div>Radio button with the label showing first</div>
    </cr-radio-button>
    <cr-radio-button name="option5" label="Option 5" disabled>
      <div>Disabled</div>
    </cr-radio-button>
  </cr-radio-group>

  Selected option: ${this.selectedRadioOption_}
  <cr-radio-group selected="option5">
    <cr-radio-button name="option5" label="Disabled selected"
        disabled></cr-radio-button>
  </cr-radio-group>
</div>`;
}
