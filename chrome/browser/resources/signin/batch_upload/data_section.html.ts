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
        <h2 id="sectionTitle">${this.title_}</h2>
        <cr-expand-button id="expandButton" no-hover
            ?hidden="${this.disabled_}"
            ?expanded="${this.expanded_}"
            @expanded-changed="${this.onExpandChanged_}"
            aria-label="${this.titleWithoutCount_}">
        </cr-expand-button>
        <div id="separator" ?hidden="${this.disabled_}"></div>
        <cr-toggle id="toggle"
            @checked-changed=${this.onToggleChanged_}
            ?checked="${!this.disabled_}"
            aria-label="${this.titleWithoutCount_}">
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
                @change="${this.onCheckedChanged_}">
                ${item.title}, ${item.subtitle}
            </cr-checkbox>
            <div class="data-item-content">
              <div class="item-icon-container"
                  ?hidden="${this.isStrEmpty_(item.iconUrl)}">
                <img class="item-icon" alt=""
                    src="${this.getFaviconUrl_(item.iconUrl)}">
              </div>
              <div class="item-info">
                <div class="item-title text-elide" aria-hidden="true">
                  ${item.title}
                </div>
                <div class="item-subtitle text-elide" aria-hidden="true">
                  ${item.subtitle}
                </div>
              </div>
            </div>
          </div>
          `)}
        </div>
      </cr-collapse>
    </div>`;
  // clang-format on
}
