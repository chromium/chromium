// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {ToggleRowElement} from './toggle_row.js';

export function getHtml(this: ToggleRowElement) {
  return html`<!--_html_template_start_-->
<div id="left-content" aria-hidden="true">
  <div class="horizontal-align">
    ${this.icon ? html`
      <cr-icon id="icon" .icon="${this.icon}"></cr-icon>
    ` : ''}
    <div class="vertical-align">
      <div id="label">${this.label}</div>
        <div id="description" class="secondary-text">
          ${this.description}
          <slot name="description"></slot>
        </div>
    </div>
  </div>
</div>
<div id="right-content" class="horizontal-align">
  ${this.managed ? html`
    <cr-policy-indicator id="policyIndicator" indicator-type="devicePolicy">
    </cr-policy-indicator>
  ` : ''}
  <cr-toggle id="toggle"
      ?checked="${this.value}"
      ?disabled="${this.isDisabled_()}"
      role="button"
      tabindex="0"
      .ariaLabel="${this.label}">
  </cr-toggle>
</div>
<!--_html_template_end_-->`;
}
