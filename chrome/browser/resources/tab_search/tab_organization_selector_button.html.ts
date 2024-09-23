// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {TabOrganizationSelectorButtonElement} from './tab_organization_selector_button.js';

export function getHtml(this: TabOrganizationSelectorButtonElement) {
  // clang-format off
  return html`
<!--_html_template_start_-->
<cr-button ?disabled="${this.disabled}">
  <div id="buttonContent">
    <div id="iconTextRow">
      <div id="prefixIcon">
        <cr-icon icon="${this.icon}"></cr-icon>
      </div>
      <div id="textColumn">
        <div id="heading">${this.heading}</div>
        <div>${this.subheading}</div>
      </div>
    </div>
    ${this.disabled ? '' : html`<cr-icon icon="cr:chevron-right"></cr-icon>`}
  </div>
</cr-button>
<!--_html_template_end_-->`;
  // clang-format on
}
