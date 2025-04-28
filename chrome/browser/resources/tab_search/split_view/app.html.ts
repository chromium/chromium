// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {SplitNewTabPageAppElement} from './app.js';

export function getHtml(this: SplitNewTabPageAppElement) {
  return html`<!--_html_template_start_-->
<cr-icon-button id="closeButton"
    iron-icon="tab-search:close"
    @click="${this.onClose_}">
</cr-icon-button>
${
      this.allInvisibleTabs_.length === 0 ? html`
  <div class="title">$i18n{splitViewEmptyTitle}</div>
  <div class="body">$i18n{splitViewEmptyBody}</div>
  ` :
                                            html`
  <div class="title">$i18n{splitViewTitle}</div>
  <div class="contents">
  <div class="heading">$i18n{splitViewOpenHeading}</div>
  <div class="tab-list">
    ${this.allInvisibleTabs_.map(data => html`
      <tab-search-item class="mwb-list-item"
          hide-close-button
          hide-timestamp
          size="large"
          .data="${data}"
          @click="${this.onTabClick_}">
      </tab-search-item>
    `)}
  </div>`}
<!--_html_template_end_-->`;
}
