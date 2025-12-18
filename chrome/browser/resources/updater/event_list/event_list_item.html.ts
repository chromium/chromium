// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import {localizeEventType} from '../event_history.js';
import {loadTimeData} from '../i18n_setup.js';

import type {EventListItemElement} from './event_list_item.js';

export function getHtml(this: EventListItemElement) {
  if (!this.event) {
    return '';
  }
  // clang-format off
  return html`
<!--_html_template_start_-->
<cr-expand-button ?expanded="${this.expanded}"
    @expanded-changed="${this.onExpandedChanged}">
  <div class="event-header">
    <div class="event-summary">
      <span class="event-app${!this.appId ? ' internal-event' : ''}">
        ${this.appLabel ?? ''}
      </span>
      <div class="event-type-column">
        <span class="event-type">
          ${localizeEventType(this.event.eventType)}
        </span>
      </div>
      <span class="event-description">
        ${this.shouldShowOmahaRequestChip() ? html`
          <span class="event-type omaha-request">
            $i18n{omahaRequest}
          </span>
        ` : ''}
        ${this.eventSummary ?? ''}
      </span>
    </div>
    <div class="event-timestamp">
      ${this.formattedDate ? html`
        <span class="event-date">
          ${this.formattedDate}
        </span>
      ` : ''}
      ${this.formattedDuration ? html`
        <span class="event-duration">${this.formattedDuration}</span>
      ` : ''}
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
  ${this.error ? html`
    <div class="event-error-details">
      ${this.errors.map(item => html`
        <div>
          ${item}
        </div>
      `)}
    </div>
  ` : ''}
  ${this.updaterVersion ? html`
    <div>
      ${loadTimeData.getStringF('updaterVersion', this.updaterVersion)}
    </div>
  ` : ''}
  <raw-event-details .events="${[this.event]}">
  </raw-event-details>
</cr-collapse>
<!--_html_template_end_-->`;
  // clang-format on
}
