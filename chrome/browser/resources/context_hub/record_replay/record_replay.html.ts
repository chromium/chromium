// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {RecordReplayElement} from './record_replay.js';

export function getHtml(this: RecordReplayElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
    ${this.recordReplayEnabled_ ? html`
      <div id="container">
        <h2>Record and Replay</h2>
        <p>Skeleton code for Record and Replay feature.</p>

        <div class="controls">
          <cr-button class="action-button" ?disabled="${this.isRecording_}"
              @click="${this.onStartRecordingClick_}">
            Start Recording
          </cr-button>
          <cr-button class="action-button" ?disabled="${!this.isRecording_}"
              @click="${this.onStopRecordingClick_}">
            Stop Recording
          </cr-button>
        </div>

        <div class="status">
          ${this.isRecording_ ? html`
            <p class="recording-status">
              Recording in progress...
            </p>
          ` : ''}
        </div>

        ${this.sessionData_ && this.sessionData_.actions.length > 0 ? html`
          <div class="results">
            <h3>Recorded Actions</h3>
            <ul>
              ${this.sessionData_.actions.map(item => html`
                <li class="action-item">
                  <div><strong>Action:</strong> ${item.description}</div>
                  <small class="action-timestamp">
                    Timestamp: ${this.formatTimestamp_(item.timestamp)}
                  </small>
                  ${item.screenshotDataUrl ? html`
                    <img class="action-screenshot"
                        src="${item.screenshotDataUrl}"
                        alt="${item.description}">
                  ` : ''}
                </li>
              `)}
            </ul>
          </div>
        ` : ''}
      </div>
    ` : ''}
  <!--_html_template_end_-->`;
  // clang-format on
}
