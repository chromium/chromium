// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {ControlledRadioButtonElement} from './controlled_radio_button.js';

export function getHtml(this: ControlledRadioButtonElement) {
  return html`<!--_html_template_start_-->
<div
    aria-checked="${this.getAriaChecked()}"
    aria-describedby="slotted-content"
    aria-disabled="${this.getAriaDisabled()}"
    aria-labelledby="label"
    class="disc-wrapper"
    id="button"
    part="disc"
    role="radio"
    tabindex="${this.getButtonTabIndex()}"
    @keydown="${this.onInputKeydown}">
  <div class="disc-border"></div>
  <div class="disc"></div>
  <div id="overlay"></div>
</div>

<div id="labelWrapper" part="labelWrapper">
  <span id="label" ?hidden="${!this.label}">${this.label}</span>
  <span id="slotted-content">
    <slot></slot>
  </span>
</div>

<slot name="additional-content"></slot>

${this.showIndicator_() ? html`
  <cr-policy-pref-indicator part="policy-indicator" .pref="${this.pref}"
      @click="${this.onIndicatorClick_}" icon-aria-label="${this.label}">
  </cr-policy-pref-indicator>
` : ''}
<!--_html_template_end_-->`;
}
