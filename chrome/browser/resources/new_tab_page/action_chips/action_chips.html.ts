// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
import {html, nothing} from '//resources/lit/v3_0/lit.rollup.js';

import {type ActionChip} from '../action_chips.mojom-webui.js';

import {type ActionChipsElement} from './action_chips.js';

export function getHtml(this: ActionChipsElement) {
  return html`
  <div class="action-chips-wrapper">
    <div class="action-chips-container">
    ${
      this.actionChips_.map(
          (chip: ActionChip) => html`
      <button id="${this.getId(chip) || nothing}"
        class="action-chip"
        @click="${() => this.handleClick_(chip)}">
        <div class="action-chip-icon-container ${
              this.getAdditionalIconClasses_(chip)}"></div>
        <div class="action-chip-text-container">
          <span class="chip-title">${chip.title}</span>
          <span class="chip-body">${chip.suggestion}</span>
        </div>
      </button>`)}
    </div>
  </div>`;
}
