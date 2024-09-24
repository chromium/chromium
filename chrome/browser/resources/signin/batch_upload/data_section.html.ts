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
        <div class="data-section-title">
          ${this.getSectionTitle_()}
        </div>
        <cr-expand-button class="expand-button" no-hover
            @click="${this.onExpandClicked_}">
        </cr-expand-button>
        <div class="separator"></div>
        <cr-toggle class="toggle" checked></cr-toggle>
      </div>
      <cr-collapse id="collapse" class="data-items-collapse"
          .opened="${this.expanded_}">
        <div class="data-items-list">
          ${this.dataContainer.dataItems.map((item) =>
          html`
          <div class="data-item">
            <cr-checkbox class="item-checkbox" checked
                data-id="${item.id}"
                @change="${this.onCheckedChanged_}"/>
            <div class="data-item-content">
              <img class="item-icon" alt="Item icon" src="${item.iconUrl}">
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
