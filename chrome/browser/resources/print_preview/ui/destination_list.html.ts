// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import type {Destination} from '../data/destination.js';

import type {PrintPreviewDestinationListElement} from './destination_list.js';

export function getHtml(this: PrintPreviewDestinationListElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
<div class="no-destinations-message" ?hidden="${this.hasDestinations_}">
  $i18n{noDestinationsMessage}
</div>

<cr-infinite-list id="list" .items="${this.matchingDestinations_}" role="grid"
    aria-rowcount="${this.matchingDestinations_.length}"
    aria-label="$i18n{printDestinationsTitle}" ?hidden="${this.hideList_}"
    item-size="32" chunk-size="30"
    .template="${(item: Destination, index: number, tabIndex: number) => html`
      <div role="row" id="destination_${index}"
          aria-rowindex="${this.getAriaRowindex_(index)}" tabindex="${tabIndex}"
          @focus="${this.onDestinationRowFocus_}">
        <print-preview-destination-list-item class="list-item"
            .searchQuery="${this.searchQuery}" .destination="${item}"
            @click="${this.onDestinationSelected_}"
            @keydown="${this.onKeydown_}" tabindex="-1"
            role="gridcell">
        </print-preview-destination-list-item>
      </div>
    `}"></cr-infinite-list>
<div class="throbber-container" ?hidden="${this.throbberHidden_}">
  <div class="throbber"></div>
</div>
<!--_html_template_end_-->`;
  // clang-format on
}
