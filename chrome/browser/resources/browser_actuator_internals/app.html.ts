// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import {TRUNCATED_LABEL, TRUNCATED_NOTE} from './app.js';
import type {BrowserActuatorInternalsAppElement} from './app.js';

export function getHtml(this: BrowserActuatorInternalsAppElement) {
  return html`
<header class="page-header">
  <h1 class="page-title">chrome://browser-actuator-internals</h1>
  <p class="subtitle session-count">
    Total Sessions: ${this.sessions_.length}
  </p>
  <p class="subtitle hint">Reload the page to see new data.</p>
</header>

${this.sessions_.length === 0 ? html`
  <div id="empty-state">
    No sessions recorded yet. A session is recorded when it receives its first
    downstream message.
  </div>
` : html`
  <div id="sessions-list">
    ${this.sessions_.map(session => html`
      <div class="card session-card" data-session-id="${session.sessionId}">
        <div class="session-header">
          <span class="session-id">Session: ${session.sessionId}</span>
          <span
              class="badge ${
                  session.endWallTime === null ? 'active' : 'closed'}">
            ${session.endWallTime === null ? 'Active' : 'Closed'}
          </span>
        </div>
        <div class="stat-grid">
          <div class="stat-tile">
            <div class="stat-label">Downstream</div>
            <div class="stat-value downstream">
              <span class="downstream-count">
                ${session.totalDownstreamMessages}
              </span>
            </div>
          </div>
          <div class="stat-tile">
            <div class="stat-label">Upstream</div>
            <div class="stat-value upstream">
              <span class="upstream-count">
                ${session.totalUpstreamMessages}
              </span>
            </div>
          </div>
          <div class="stat-tile">
            <div class="stat-label">Start</div>
            <div class="stat-value">
              <span class="start-time">
                ${this.formatTime_(session.startWallTime)}
              </span>
            </div>
          </div>
          <div class="stat-tile">
            <div class="stat-label">End</div>
            <div class="stat-value">
              ${session.endWallTime !== null ? html`
                <span class="end-time">
                  ${this.formatTime_(session.endWallTime)}
                </span>
              ` : html`
                &mdash;
              `}
            </div>
          </div>
        </div>

        ${session.events.length > 0 ? html`
          <details class="events">
            <summary class="events-summary">
              ${this.getEventsSummary_(session)}
            </summary>
            <table class="events-table">
              <thead>
                <tr>
                  <th>Time</th>
                  <th>Direction</th>
                  <th>Payload Types</th>
                  <th>Message</th>
                </tr>
              </thead>
              <tbody>
                ${session.events.map(event => html`
                  <tr class="event-row">
                    <td class="event-time">
                      ${this.formatTime_(event.timestamp)}
                    </td>
                    <td class="event-direction">
                      <span
                          class="direction-pill ${
                              this.getDirectionClass_(event.direction)}">
                        ${this.getDirectionLabel_(event.direction)}
                      </span>
                    </td>
                    <td class="event-payload-types">
                      ${event.payloadTypes.join(', ')}
                    </td>
                    <td class="event-message">
                      ${event.message.length > 80 ? html`
                        <details class="message-details">
                          <summary>
                            <span class="message-text">
                              ${event.message.slice(0, 80)}...
                            </span>
                            ${event.messageTruncated ? html`
                              <span class="truncated-marker">
                                ${TRUNCATED_LABEL}
                              </span>
                            ` : ''}
                          </summary>
                          <pre class="message-full">${event.message}</pre>
                          ${event.messageTruncated ? html`
                            <div class="truncated-note">
                              ${TRUNCATED_NOTE}
                            </div>
                          ` : ''}
                        </details>
                      ` : html`
                        <span class="message-text">${event.message}</span>
                        ${event.messageTruncated ? html`
                          <span class="truncated-marker">
                            ${TRUNCATED_LABEL}
                          </span>
                        ` : ''}
                      `}
                    </td>
                  </tr>
                `)}
              </tbody>
            </table>
          </details>
        ` : ''}
      </div>
    `)}
  </div>
`}`;
}
