// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html, nothing} from '//resources/lit/v3_0/lit.rollup.js';

import type {OrganizerListSectionItemElement} from './organizer_list_section_item.js';

export function getHtml(this: OrganizerListSectionItemElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
<!-- TODO(b/549796273): Use custom title and description element. -->
<cr-url-list-item id="crUrlListItem" .title="${this.item.title}"
    .description="${this.getDescription_()}"
    .url="${this.getUrl_() || nothing}"
    ?always-show-suffix="${this.hasSuffix_()}"
    .size="${this.item.size || nothing}">
  ${this.item.prefixIcon?.element ? html`
    <div slot="customIcon">
      ${this.item.prefixIcon.element}
    </div>
  ` : ''}
  ${this.item.trailingIcon ? html`
    <cr-icon id="trailingIcon" slot="suffix" .icon="${this.item.trailingIcon}"
        class="${this.hasActionButton_() ? 'has-action-button' : ''}">
    </cr-icon>
  ` : ''}
  ${this.item.hoveredActionButton ? html`
    <cr-icon-button id="actionButton" slot="suffix"
        iron-icon="${this.item.hoveredActionButton.icon}"
        aria-label="${this.item.hoveredActionButton.ariaLabel}"
        @click="${this.onActionButtonClick_}">
    </cr-icon-button>
  ` : ''}
</cr-url-list-item>
<!--_html_template_end_-->`;
  // clang-format on
}
