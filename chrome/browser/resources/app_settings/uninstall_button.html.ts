// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {UninstallButtonElement} from './uninstall_button.js';

export function getHtml(this: UninstallButtonElement) {
  return html`<!--_html_template_start_-->
${this.showPolicyIndicator_() ? html`
  <cr-tooltip-icon
      id="policyIndicator"
      icon-class="cr20:domain"
      tooltip-text="${this.policyLabel}"
      icon-aria-label="${this.policyLabel}"
      tooltip-position="bottom">
  </cr-tooltip-icon>
` : ''}

${this.showUninstallButton_() ? html`
  <cr-button id="uninstallButton" @click="${this.onClick_}"
      ?disabled="${this.getDisableState_()}">
    ${this.uninstallLabel}
  </cr-button>
` : ''}
<!--_html_template_end_-->`;
}
