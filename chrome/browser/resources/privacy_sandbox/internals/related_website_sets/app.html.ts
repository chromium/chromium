// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {RelatedWebsiteSetsAppElement} from './app.js';

export function getHtml(this: RelatedWebsiteSetsAppElement) {
  // clang-format off
  return html`
<related-website-sets-toolbar id="toolbar" .pageName="${this.pageTitle_}"
    ?narrow="${this.narrow_}" @narrow-changed="${this.onNarrowChanged_}"
    @cr-toolbar-menu-click="${this.onMenuButtonClick_}"
    @search-changed="${this.onSearchChanged_}">
</related-website-sets-toolbar>
<div id="container" role="group">
  <related-website-sets-sidebar id="sidebar" ?hidden="${this.narrow_}">
  </related-website-sets-sidebar>
  <div id="content">
    <related-website-sets-list-container id="rws-list-container"
        class="cr-centered-card-container" .errorMessage="${this.errorMessage_}"
        .query="${this.query_}"
        .relatedWebsiteSets="${this.relatedWebsiteSets_}">
    </related-website-sets-list-container>
  </div>
  <div id="space-holder" ?hidden="${this.narrow_}"></div>
  <cr-drawer id="drawer" heading="Related Website Sets"
      @close="${this.onDrawerClose_}">
    <div slot="body">
      <related-website-sets-sidebar></related-website-sets-sidebar>
    </div>
  </cr-drawer>
</div>`;
  // clang-format on
}
