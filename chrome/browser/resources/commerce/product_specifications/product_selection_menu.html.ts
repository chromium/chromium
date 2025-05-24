// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {ProductSelectionMenuElement} from './product_selection_menu.js';

// clang-format off
export function getHtml(this: ProductSelectionMenuElement) {
  return html`<!--_html_template_start_-->
  <cr-lazy-render-lit id="menu" .template="${() => html`
    <cr-action-menu @close="${this.onClose_}" auto-reposition="true">
      <div id="selectionContainer">
        ${!this.showTableFullMessage_() ? html`
            ${this.sections.map((section, sectionIndex) => html`
              <cr-expand-button ?expanded="${section.expanded}" no-hover
                  @expanded-changed="${
                      (e: CustomEvent<{value: boolean}>) => {
                          this.expandedChanged_(e, section);
                      }}"
                  expand-icon="cr:arrow-drop-down"
                  collapse-icon="cr:arrow-drop-up">
                <div class="section-title">${section.title}</div>
              </cr-expand-button>
              <cr-collapse ?opened="${section.expanded}">
                ${section.entries.map((item, index) => html`
                  <cr-url-list-item class="dropdown-item" size="medium"
                      url="${item.url}" title="${item.title}"
                      description="${this.getUrl_(item)}" no-hover
                      data-item-index="${index}"
                      data-section-index="${sectionIndex}"
                      data-section-type="${section.sectionType}"
                      @click="${this.onSelect_}">
                  </cr-url-list-item>
                `)}
                <!-- Add spacer instead of adding margin or padding to the -->
                <!-- parent for smooth expand/collapse transitions for the -->
                <!-- last section. -->
                ${this.isLastSection_(sectionIndex) ? html`
                  <div class="dropdown-spacer"></div>
                ` : ''}
              </cr-collapse>
            `)}
          ` : ''}
        <div id="empty" class="message"
            ?hidden="${!this.showEmptySuggestionsMessage_()}">
          $i18n{emptyMenu}
        </div>
        <div id="tableFullMessage" class="message"
            ?hidden="${!this.showTableFullMessage_()}">
          $i18n{tableFullMessage}
        </div>
      </div>
      ${this.selectedUrl ? html`
        <hr>
        <button id="remove" class="dropdown-item" role="menuitem"
            @click="${this.onRemoveClick_}">
          <cr-icon icon="product-specifications:remove"></cr-icon>
          $i18n{removeColumn}
        </button>
      ` : ''}
    </cr-action-menu>
  `}">
  </cr-lazy-render-lit>
  <!--_html_template_end_-->`;
}
// clang-format on
