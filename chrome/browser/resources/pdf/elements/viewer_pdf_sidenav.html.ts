// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import type {ViewerPdfSidenavElement} from './viewer_pdf_sidenav.js';

export function getHtml(this: ViewerPdfSidenavElement) {
  return html`
<div id="icons" ?hidden="${this.hideIcons_()}" role="tablist"
    @keydown="${this.onKeydown_}">
  ${this.tabs_.map(item => html`<div class="button-wrapper cr-vertical-tab
        ${this.getTabSelectedClass_(item.id)}">
      <cr-icon-button .ironIcon="${item.icon}" role="tab"
          title="${item.title}" data-tab-id="${item.id}"
          aria-selected="${this.getTabAriaSelected_(item.id)}"
          tabindex="${this.getTabIndex_(item.id)}"
          @click="${this.onTabClick_}">
      </cr-icon-button>
    </div>`)}
</div>
<div id="content">
  <viewer-thumbnail-bar id="thumbnail-bar" tabindex="0"
      ?hidden="${this.hideThumbnailView_()}" .activePage="${this.activePage}"
      .clockwiseRotations="${this.clockwiseRotations}"
      .docLength="${this.docLength}">
  </viewer-thumbnail-bar>
  <viewer-document-outline id="outline"
      ?hidden="${this.hideOutlineView_()}" .bookmarks="${this.bookmarks}">
  </viewer-document-outline>
  <viewer-attachment-bar id="attachment-bar"
      ?hidden="${this.hideAttachmentView_()}"
      .attachments="${this.attachments}">
  </viewer-attachment-bar>
</div>`;
}
