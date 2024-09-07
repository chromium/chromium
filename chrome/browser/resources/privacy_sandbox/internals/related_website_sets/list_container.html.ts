// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {RelatedWebsiteSetsListContainerElement} from './list_container.js';

export function getHtml(this: RelatedWebsiteSetsListContainerElement) {
  return html`
<div class="header">
  <h2 class="flex page-title">All sets</h2>
  <cr-button id="expandCollapseButton"
      @click="${this.onClick_}" ?hidden="${this.errorMessage}">
    ${this.expandCollapseButtonText_()}
  </cr-button>
</div>
<div id="descriptionLabel" class="cr-secondary-text"
    ?hidden="${!this.errorMessage}">
  ${this.getDisplayedError()}
</div>
<div id="descriptionLabel" class="cr-secondary-text"
    ?hidden="${this.errorMessage}">
  <p>RWS explanation string.
    <a href="https://developers.google.com/privacy-sandbox/3pcd/related-website-sets" target="_blank" rel="noopener noreferrer">Learn more.</a>
  </p>
</div>
<div id="related-website-sets" class="card" role="list"
    ?hidden="${this.errorMessage}">
  ${this.filteredItems.map(item => html`
    <related-website-sets-list-item id="${item.primarySite}"
        .primarySite="${item.primarySite}"
        .memberSites="${this.getMemberSites_(item)}"
        .managedByEnterprise="${item.managedByEnterprise}"
        .query="${this.query}"
        @expanded-toggled="${this.onExpandedToggled_}">
    </related-website-sets-list-item>
  `)}
</div>`;
}
