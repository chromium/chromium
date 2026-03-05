// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {EnterprisePolicyTableSectionElement} from './enterprise_policy_table_section.js';

export function getHtml(this: EnterprisePolicyTableSectionElement) {
  // clang-format off
  return html`
<!--_html_template_start_-->
  <div class="policy-table" role="table">
    <div class="header-row" role="row">
      <div class="column-name" role="columnheader">$i18n{policyName}</div>
      <div class="column-source" role="columnheader">$i18n{policySource}</div>
      <div class="column-value" role="columnheader">$i18n{policyValue}</div>
      <div class="column-warning" role="columnheader">$i18n{policyStatus}</div>
      <div class="column-expand" role="columnheader"></div>
    </div>
    ${this.rowData.map((item, index) => html`
      <div class="policy-row" role="row" ?expanded="${item.isExpanded}">
        <div class="column-name" role="cell" title="${item.name}">
          ${item.name}
        </div>
        <div class="column-source" role="cell">
          ${item.policy.prevailingSource}
        </div>
        <div class="column-value" role="cell">
          <enterprise-policy-value .policyName="${item.name}"
              .value="${this.prevailingValue(item)}">
          </enterprise-policy-value>
        </div>
        <div class="column-warning" role="cell">
          ${item.hasConflict ? html`
            <cr-icon class="warning-icon" icon="cr:warning" role="img"
                aria-label="$i18n{policyConflictWarning}"
                title="$i18n{policyConflictWarning}">
            </cr-icon>
          ` : html`
            <cr-icon class="check-icon" icon="cr:check" role="img"
                aria-label="$i18n{policyOk}" title="$i18n{policyOk}">
            </cr-icon>
          `}
        </div>
        <div class="column-expand" role="cell">
          ${this.canExpand(item) ? html`
            <cr-icon-button class="expand-icon"
                data-index="${index}"
                iron-icon="${item.isExpanded ? 'cr:expand-less' :
                    'cr:expand-more'}"
                aria-expanded="${item.isExpanded}"
                aria-controls="expanded-section-${index}"
                @click="${this.onExpandButtonClick}">
            </cr-icon-button>
          ` : ''}
        </div>
      </div>
      <cr-collapse id="expanded-section-${index}" class="expanded-section"
          ?opened="${item.isExpanded}" no-animation>
        ${Object.entries(item.policy.valuesBySource)
            .filter(([source, _]) => source !== item.policy.prevailingSource)
            .map(([source, value]) => html`
          <div class="expanded-row" role="row">
            <div class="column-name" role="cell">
              $i18n{policyIgnored}
            </div>
            <div class="column-source" role="cell">${source}</div>
            <div class="column-value" role="cell">
              <enterprise-policy-value .policyName="${item.name}"
                  .value="${value}">
              </enterprise-policy-value>
            </div>
            <div class="column-warning" role="cell"></div>
            <div class="column-expand" role="cell"></div>
          </div>
        `)}
      </cr-collapse>
    `)}
  </div>
<!--_html_template_end_-->`;
  // clang-format on
}
