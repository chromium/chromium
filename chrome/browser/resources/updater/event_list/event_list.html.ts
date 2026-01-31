// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {EventEntry, EventListElement} from './event_list.js';

export function getHtml(this: EventListElement) {
  // clang-format off
  return html`
<!--_html_template_start_-->
<div class="filter-bar-container">
  <filter-bar .filterSettings="${this.filterSettings}"
      @filters-changed="${this.onFiltersChanged}">
  </filter-bar>
  <div class="separator"></div>
  <div class="expand-collapse-controls">
    <cr-button id="expand-all" @click="${this.onExpandCollapseAllClick}">
      ${this.anyExpanded ? '$i18n{collapseAll}' : '$i18n{expandAll}'}
    </cr-button>
  </div>
</div>
<div class="event-count">
  ${this.numDisplayedEventsLabel}
</div>
<cr-infinite-list class="event-list" .items="${this.events}" item-size="36"
    chunk-size="100" role="grid" aria-rowcount="${this.events.length}"
    ?hidden="${this.events.length > 0}" .scrollTarget="${this.scrollTarget}"
    .template="${(item: EventEntry) => html`
      <div>
        <event-list-item .event="${item.event}" .eventDate="${item.eventDate}"
            .processMap="${this.processMap}" .policies="${item.policies}"
            @expanded-changed="${this.onEventItemExpandedChanged}">
        </event-list-item>
      </div>`}">
</cr-infinite-list>
${this.eventsWithoutDates.length > 0 ? html`
  <div>
    <div>${this.eventsWithoutDatesLabel}</div>
    <raw-event-details .events="${this.eventsWithoutDates}"></raw-event-details>
  </div>
` : ''}
${this.eventsWithParseErrors.length > 0 ? html`
  <div>
    <div>${this.eventsWithParseErrorsLabel}</div>
    <raw-event-details .events="${this.eventsWithParseErrors}">
    </raw-event-details>
  </div>
` : ''}
<!--_html_template_end_-->`;
  // clang-format on
}
