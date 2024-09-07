// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import type {ViewerBookmarkElement} from './viewer_bookmark.js';

export function getHtml(this: ViewerBookmarkElement) {
  return html`<!--_html_template_start_-->
<div id="item" @click="${this.onClick_}"
    .style="${this.getItemStartPaddingStyle_()}">
  <div id="expand-container">
    <cr-icon-button id="expand" iron-icon="cr:chevron-right"
        ?hidden="${this.getExpandHidden_()}"
        aria-label="$i18n{bookmarkExpandIconAriaLabel}"
        aria-expanded="${this.childrenShown_}"
        @click="${this.toggleChildren_}"></cr-icon-button>
  </div>
  <span id="title" tabindex="0">${this.bookmark.title}</span>
</div>
${this.childrenShown_ ? html`
  ${this.bookmark.children.map(item => html`
    <viewer-bookmark .bookmark="${item}" .depth="${this.getChildDepth_()}">
    </viewer-bookmark>`)}` : ''}
<!--_html_template_end_-->`;
}
