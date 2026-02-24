// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import {localizeEventType} from '../event_history.js';
import {loadTimeData} from '../i18n_setup.js';

import type {EventListItemElement} from './event_list_item.js';

export function getHtml(this: EventListItemElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
${this.event !== undefined ? html`
  <cr-expand-button ?expanded="${this.expanded}"
      @expanded-changed="${this.onExpandedChanged}">
    <div class="event-header">
      <div class="event-summary">
        <div class="event-date-column">
          ${this.formattedDate ?? ''}
        </div>
        <span class="event-app-column${!this.appId ? ' internal-event' : ''}">
          ${this.appLabel ?? ''}
        </span>
        <div class="event-scope-column">
          ${this.scope ? html`
            <scope-icon .scope="${this.scope}"></scope-icon>
            </scope-icon>
          ` : ''}
        </div>
        <div class="event-type-column">
          <span class="event-type">
            ${localizeEventType(this.event.eventType)}
          </span>
        </div>
        <div class="event-description-icon-column">
          ${this.eventSummaryIcon ? html`
            <cr-icon icon="${this.eventSummaryIcon}">
            </cr-icon>
          ` : ''}
        </div>
        <span class="event-description-column">
          ${this.eventSummary ?? ''}
        </span>
      </div>
    </div>
  </cr-expand-button>
  <cr-collapse class="event-body" ?opened="${this.expanded}">
    ${this.omahaRequest ? html`
      <raw-event-details .events="${[this.omahaRequest]}"
          label="$i18n{omahaRequest}">
      </raw-event-details>
    ` : ''}
    ${this.omahaResponse ? html`
      <raw-event-details .events="${[this.omahaResponse]}"
          label="$i18n{omahaResponse}">
      </raw-event-details>
    ` : ''}
    ${this.nextVersion ? html`
      <div>
        ${loadTimeData.getStringF('nextVersion', this.nextVersion)}
      </div>
    ` : ''}
    ${this.commandLine ? html`
      <div>
        $i18n{commandLine}
        <code>${this.commandLine}</code>
      </div>
    ` : ''}
    ${this.errors.length > 0 ? html`
      <ul class="event-error-details">
        ${this.errors.map(item => html`
          <li>
            ${item}
          </li>
        `)}
      </ul>
    ` : ''}
    ${this.updaterVersion ? html`
      <div>
        ${loadTimeData.getStringF('updaterVersion', this.updaterVersion)}
      </div>
    ` : ''}
    ${this.formattedDuration ? html`
      <span class="event-duration">${this.formattedDuration}</span>
    ` : ''}
    ${this.policies !== undefined ? html`
      <div>
      <h3>Enterprise Policies</h3>
      <enterprise-policy-table .policies="${this.policies}"
          .appId="${this.appId}">
      </enterprise-policy-table>
      </div>
    ` : ''}
    <raw-event-details .events="${[this.event]}">
    </raw-event-details>
  </cr-collapse>
` : ''}
<!--_html_template_end_-->`;
  // clang-format on
}
