// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {LogsAppElement} from './logs_app.js';

export function getHtml(this: LogsAppElement) {
  // clang-format off
  return html`
<h1>Omnibox Debug Logs</h1>
<div>
  <div>Log Messages:</div>
  <table>
    <thead>
      <tr>
        <th class="time">Time</th>
        <th class="tag">Tag</th>
        <th class="source-location">Source Location</th>
        <th class="message">Log Message</th>
      </tr>
    </thead>
    <tbody>
      ${this.eventLogMessages_.map(item => html`
        <tr>
          <td class="time">${item.eventTime.toLocaleTimeString()}</td>
          <td class="tag">${item.tag}</td>
          <td class="source-location">
            <a target="_blank" href="${item.sourceLinkUrl}">
              ${item.sourceLinkText}
            </a>
          </td>
          <td class="message">${item.message}</td>
        </tr>
      `)}
    </tbody>
  </table>
</div>
`;
  // clang-format on
}
