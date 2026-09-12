// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html, nothing} from '//resources/lit/v3_0/lit.rollup.js';

import type {OrganizerListSectionItemElement} from './organizer_list_section_item.js';

export function getHtml(this: OrganizerListSectionItemElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
<cr-url-list-item id="crUrlListItem"
    .itemAriaLabel="${this.getAriaLabel_()}"
    .itemAriaDescription="${this.getAriaDescription_() || nothing}"
    .url="${this.item.prefixIcon?.url || nothing}"
    ?always-show-suffix="${this.hasSuffix_()}"
    .size="${this.item.size || nothing}">
  <div id="content" slot="content">
    <organizer-list-section-item-title id="title"
        .titleParts="${this.item.title}">
    </organizer-list-section-item-title>
    <organizer-list-section-item-description id="description"
        ?hidden="${!this.hasDescription_()}"
        .descriptionParts="${this.item.description || []}">
    </organizer-list-section-item-description>
  </div>
  ${this.item.prefixIcon?.element ? html`
    <div slot="customIcon">
      ${this.item.prefixIcon.element}
    </div>
  ` : this.item.prefixIcon?.stackedFavicons ? html`
    <stacked-favicons id="stackedFavicons" slot="customIcon"
        .url="${this.item.prefixIcon.stackedFavicons.urls[0]}"
        .secondaryUrl="${this.item.prefixIcon.stackedFavicons.urls[1]}"
        ?stack-vertically="${
            this.item.prefixIcon.stackedFavicons.stackVertically}">
    </stacked-favicons>
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
