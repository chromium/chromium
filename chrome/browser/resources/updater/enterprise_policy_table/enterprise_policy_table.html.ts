// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {EnterprisePolicyTableElement} from './enterprise_policy_table.js';

export function getHtml(this: EnterprisePolicyTableElement) {
  // clang-format off
  return html`
<!--_html_template_start_-->
${this.hasOnlyDefaultValues ? html`
  <div class="no-policies">$i18n{noPolicies}</div>
` : html`
  ${this.updaterPolicies.length > 0 ? html`
    <div class="section">
      <h3>$i18n{updaterPolicies}</h3>
      <enterprise-policy-table-section .rowData="${this.updaterPolicies}">
      </enterprise-policy-table-section>
    </div>
  ` : ''}
  ${Object.entries(this.appPolicies).map(([appLabel, policies]) => html`
    <div class="section">
      <h3>${this.getAppPoliciesLabel(appLabel)}</h3>
      <enterprise-policy-table-section .rowData="${policies}">
      </enterprise-policy-table-section>
    </div>
  `)}
`}
<!--_html_template_end_-->`;
  // clang-format on
}
