// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {OrganizerListSectionElement} from './organizer_list_section.js';

export function getHtml(this: OrganizerListSectionElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
<h2 id="header">${this.delegate?.getHeader() || ''}</h2>
${this.hasNoSearchResults_() ? html`
  <div id="noResults">$i18n{noResults}</div>
` : html`
  <div id="items" role="list">
    ${this.getInitialItems_().map(item => html`
      <organizer-list-section-item .item="${item}" role="listitem"
          @click="${this.onItemClick_}"
          @action-button-click="${this.onItemActionButtonClick_}">
      </organizer-list-section-item>
    `)}
    ${this.hasMoreItems_() ? html`
      <div id="collapse" @transitionend="${this.onCollapseTransitionend_}">
        ${this.getRemainingItems_().map(item => html`
          <organizer-list-section-item .item="${item}" role="listitem"
              @click="${this.onItemClick_}"
              @action-button-click="${this.onItemActionButtonClick_}">
          </organizer-list-section-item>
        `)}
      </div>
      <cr-url-list-item id="expandButton" size="compact"
          .title="${this.getExpandButtonLabel_()}"
          @click="${this.onExpandButtonClick_}">
        <cr-icon id="expandButtonIcon" slot="customIcon"
            .icon="${this.getExpandButtonIcon_()}">
        </cr-icon>
      </cr-url-list-item>
    ` : ''}
  </div>
`}
<!--_html_template_end_-->`;
  // clang-format on
}
