// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {RelatedWebsiteSetsAppElement} from './app.js';

export function getHtml(this: RelatedWebsiteSetsAppElement) {
  return html`
<related-website-sets-toolbar
    id="toolbar"
    .pageName="${this.pageTitle_}"
    .narrow="${this.narrow_}"
    @narrowChanged="${(e: CustomEvent) => this.handleNarrowChange(e)}"
    @menuClicked="${() => this.handleMenuButtonClick()}">
</related-website-sets-toolbar>
<div id="container" role="group">
  <related-website-sets-sidebar
      id="sidebar"
      ?hidden="${this.narrow_}">
  </related-website-sets-sidebar>
  <cr-drawer
      id="drawer"
      heading="Related Website Sets"
      @close="${this.onDrawerClose_}">
    <div slot="body">
      <related-website-sets-sidebar></related-website-sets-sidebar>
    </div>
  </cr-drawer>
</div>`;
}