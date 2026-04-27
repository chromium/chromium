// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {RelatedWebsiteSetsListItemElement} from './list_item.js';

export function getHtml(this: RelatedWebsiteSetsListItemElement) {
  // clang-format off
  return html`
  <cr-expand-button id="expandButton" class="cr-row"
      ?expanded="${this.expanded}"
      @expanded-changed="${this.onExpandedChanged_}">
    <div id="container">
      <site-favicon class="favicon" domain="${this.primarySite}"
          url="${this.getIconImageUrl_(this.primarySite)}" aria-hidden="true">
      </site-favicon>
      <div id="borderPart">
        ${this.getBoldedParts_(this.primarySite).map(item =>
            item.isMatch ? html`<b>${item.text}</b>` : item.text)}
      </div>
      <cr-icon class="icon" icon="cr20:domain"
          ?hidden="${this.isEnterpriseIconHidden_()}">
      </cr-icon>
    </div>
  </cr-expand-button>
  <cr-collapse id="expandedContent" ?opened="${this.expanded}">
    ${this.memberSites.map(item => html`
      <div class="cr-padded-text hr">
        <site-favicon class="favicon" domain="${item.site}"
            url="${this.getIconImageUrl_(item.site)}"
            aria-hidden="true">
        </site-favicon>
        <div class="cr-secondary-text">
          ${this.getBoldedParts_(item.site).map(part =>
              part.isMatch ? html`<b>${part.text}</b>` : part.text)} - ${this.getSiteType_(item.type)}
        </div>
      </div>`)}
  </cr-collapse>`;
  // clang-format on
}
