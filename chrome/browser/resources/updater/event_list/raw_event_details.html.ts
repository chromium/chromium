// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '/strings.m.js';

import {html, nothing} from '//resources/lit/v3_0/lit.rollup.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';

import type {RawEventDetailsElement} from './raw_event_details.js';

export function getHtml(this: RawEventDetailsElement) {
  if (this.events.length === 0) {
    return nothing;
  }
  const renderedEvents = this.events.map(
      (event) => html`<pre>${JSON.stringify(event, null, 2)}</pre>`);
  return html`
    <cr-expand-button
        ?expanded="${this.expanded}"
        @expanded-changed="${this.onExpandedChanged}"
        expand-title="${loadTimeData.getString('viewRawDetails')}">
      ${loadTimeData.getString('viewRawDetails')}
    </cr-expand-button>
    <cr-collapse ?opened="${this.expanded}">
      ${renderedEvents}
    </cr-collapse>`;
}
