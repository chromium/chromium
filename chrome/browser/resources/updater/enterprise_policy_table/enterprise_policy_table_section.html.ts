// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {EnterprisePolicyTableSectionElement} from './enterprise_policy_table_section.js';

export function getHtml(this: EnterprisePolicyTableSectionElement) {
  // clang-format off
  return html`
<!--_html_template_start_-->
  <div class="policy-table">
    <div class="header-row">
      <div class="header-content">
        <div class="column-name">$i18n{policyName}</div>
        <div class="column-source">$i18n{policySource}</div>
        <div class="column-value">$i18n{policyValue}</div>
        <div class="column-warning">$i18n{policyStatus}</div>
      </div>
      <div class="header-spacer"></div>
    </div>
    ${this.rowData.map((item, index) => html`
      <cr-expand-button class="policy-row"
          data-index="${index}"
          ?expanded="${item.isExpanded}"
          @expanded-changed="${this.onRowExpandedChanged}">
        <div class="row-content">
          <div class="column-name" title="${item.name}">${item.name}</div>
          <div class="column-source">${item.policy.prevailingSource}</div>
          <div class="column-value">
            <enterprise-policy-value .policyName="${item.name}"
                .value="${this.prevailingValue(item)}">
            </enterprise-policy-value>
          </div>
          <div class="column-warning">
            ${item.hasConflict ? html`
              <cr-icon class="warning-icon" icon="cr:warning"
                  title="$i18n{policyConflictWarning}">
              </cr-icon>
            ` : html`
              <cr-icon class="check-icon" icon="cr:check"
                  title="$i18n{policyOk}">
              </cr-icon>
            `}
          </div>
        </div>
      </cr-expand-button>
      <cr-collapse class="expanded-section" ?opened="${item.isExpanded}"
          no-animation>
        ${Object.entries(item.policy.valuesBySource)
            .filter(([source, _]) => source !== item.policy.prevailingSource)
            .map(([source, value]) => html`
          <div class="expanded-row">
            <div class="expanded-row-content">
              <div class="column-name">$i18n{policyIgnored}</div>
              <div class="column-source">${source}</div>
              <div class="column-value">
                <enterprise-policy-value .policyName="${item.name}"
                    .value="${value}">
                </enterprise-policy-value>
              </div>
              <div class="column-warning"></div>
            </div>
            <div class="header-spacer"></div>
          </div>
        `)}
      </cr-collapse>
    `)}
  </div>
<!--_html_template_end_-->`;
  // clang-format on
}
