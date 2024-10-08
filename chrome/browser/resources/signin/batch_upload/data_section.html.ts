// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {DataSectionElement} from './data_section.js';

export function getHtml(this: DataSectionElement) {
  // clang-format off
  return html`
    <div class="data-section">
      <div class="data-section-header">
        <div id="sectionTitle">
          ${this.getSectionTitle_()}
        </div>
        <cr-expand-button id="expandButton" no-hover
            ?hidden="${this.disabled_}"
            ?expanded="${this.expanded_}"
            @expanded-changed="${this.onExpandChanged_}">
        </cr-expand-button>
        <div id="separator" ?hidden="${this.disabled_}"></div>
        <cr-toggle id="toggle"
            @checked-changed=${this.onToggleChanged_}
            ?checked="${!this.disabled_}">
        </cr-toggle>
      </div>
      <cr-collapse id="collapse" .opened="${this.expanded_}">
        <div id="data-items-list">
          ${this.dataContainer.dataItems.map((item) =>
          html`
          <div class="data-item">
            <cr-checkbox class="item-checkbox"
                data-id="${item.id}"
                ?checked="${this.isCheckboxChecked_(item.id)}"
                @change="${this.onCheckedChanged_}"/>
            <div class="data-item-content">
              <img class="item-icon"
                  ?hidden="${this.isStrEmpty_(item.iconUrl)}"
                  alt="Item icon" src="${item.iconUrl}">
              <div class="item-title">${item.title}</div>
              <div class="item-subtitle">${item.subtitle}</div>
            </div>
          </div>
          `)}
        </div>
      </cr-collapse>
    </div>`;
  // clang-format on
}
