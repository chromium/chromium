// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {DestinationListItemElement} from './destination_list_item.js';

export function getHtml(this: DestinationListItemElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
${this.destination !== null ? html`
  <cr-icon icon="${this.destination.icon}"></cr-icon>
  <span class="name searchable">${this.destination.displayName}</span>
  <span class="search-hint searchable" ?hidden="${!this.searchHint_}">
    ${this.searchHint_}
  </span>
  <span class="extension-controlled-indicator"
      ?hidden="${!this.destination.isExtension}">
    <span class="extension-name searchable">
      ${this.destination.extensionName}
    </span>
    <span class="extension-icon" role="button" tabindex="0"
        title="${this.getExtensionPrinterTooltip_()}"></span>
  </span>
` : ''}
<!--_html_template_end_-->`;
  // clang-format on
}
