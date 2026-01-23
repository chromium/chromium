/* Copyright 2026 The Chromium Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file. */

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {HistoryFilterChipsElement} from './filter_chips.js';

export function getHtml(this: HistoryFilterChipsElement) {
  // TODO(b/472479874): Translate strings.
  return html`
    <div class="filter-chip-container">
      <cr-chip
          ?selected="${this.isUserSelected}"
          @click="${this.onUserVisitsClick_}">
        ${this.isUserSelected ? html`<cr-icon icon="cr:check"></cr-icon>` : ''}
        You
      </cr-chip>
      <cr-chip
          ?selected="${this.isActorSelected}"
          @click="${this.onActorVisitsClick_}">
        ${this.isActorSelected ? html`<cr-icon icon="cr:check"></cr-icon>` : ''}
        Auto browse
      </cr-chip>
    </div>
  `;
}
