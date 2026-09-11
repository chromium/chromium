// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {OrganizerListSectionItemDescriptionElement} from './organizer_list_section_item_description.js';

export function getHtml(this: OrganizerListSectionItemDescriptionElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
<div id="descriptionParts" class="description-parts">
  ${this.descriptionParts.map((descriptionPart, index) => html`
    ${index > 0 ? html`<div class="separator">•</div>` : ''}
    <div class="description-part" title="${descriptionPart.text}">
      ${descriptionPart.prefixElement ? html`
        <div class="prefix">${descriptionPart.prefixElement}</div>
      ` : ''}
      <div class="description-text ${
          descriptionPart.elideFromStart ? 'elide-from-start' : ''}">
        <bdi>${descriptionPart.text}</bdi>
      </div>
    </div>
  `)}
</div>
<!--_html_template_end_-->`;
  // clang-format on
}
