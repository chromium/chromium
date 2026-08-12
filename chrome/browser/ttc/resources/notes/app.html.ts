// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {loadTimeData} from '//resources/js/load_time_data.js';
import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {NotesAppElement} from './app.js';

export function getHtml(this: NotesAppElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
    <h1>Remembered Notes</h1>
    <div id="add-note-section">
      <cr-input id="newNoteKey"
          label="Key"
          placeholder="e.g. favorite_color">
      </cr-input>
      <cr-input id="newNoteValue"
          label="Value"
          placeholder="e.g. blue">
      </cr-input>
      <cr-button id="addNoteButton"
          class="action-button"
          @click="${this.onAddNoteClick_}">
        ${loadTimeData.getString('add')}
      </cr-button>
    </div>
    <div id="notes-list">
      ${this.notes.length === 0 ? html`
        <p id="empty-state">No notes stored yet.</p>
      ` : ''}
      ${this.notes.map(item => html`
        <div class="note-row">
          <span class="note-key">${item.key}</span>
          <cr-input class="note-value-input"
              data-key="${item.key}"
              .value="${item.value}"
              @change="${this.onNoteValueChange_}">
          </cr-input>
          <cr-button class="delete-button"
              data-key="${item.key}"
              aria-label="${loadTimeData.getString('delete')}"
              @click="${this.onDeleteNoteClick_}">
            ${loadTimeData.getString('delete')}
          </cr-button>
        </div>
      `)}
    </div>
  <!--_html_template_end_-->`;
  // clang-format on
}
