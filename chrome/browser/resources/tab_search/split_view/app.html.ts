// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {SplitNewTabPageAppElement} from './app.js';

export function getHtml(this: SplitNewTabPageAppElement) {
  return html`<!--_html_template_start_-->
<div id="header">
  <cr-icon-button id="closeButton"
      iron-icon="tab-search:close"
      @click="${this.onClose_}">
  </cr-icon-button>
  ${
      this.allInvisibleTabs_.length === 0 ? html`
        <picture>
          <source media="(prefers-color-scheme: dark)"
              srcset="./split_view/images/empty_dark.svg">
          <img id="product-logo" srcset="./split_view/images/empty.svg" alt="">
        </picture>
      ` :
                                            html``}
  <div class="title">${this.title_}</div>
  ${
      this.allInvisibleTabs_.length === 0 ?
          html`<div class="body">$i18n{splitViewEmptyBody}</div>` :
          html``}
</div>
<div class="tab-list" ?hidden="${this.allInvisibleTabs_.length === 0}">
  <cr-lazy-list id="splitTabsList" class="scroller"
      .items="${this.allInvisibleTabs_}"
      item-size="66"
      .minViewportHeight="${this.minViewportHeight_}"
      .scrollTarget="${this.scrollTarget_}"
      @keydown="${this.onTabClick_}"
      @viewport-filled="${this.updateFocusedItem_}"
      .restoreFocusElement="${this.focusedItem_}"
      .template="${
      (item: Object, _: number) => html`<tab-search-item class="mwb-list-item"
          hide-close-button
          hide-timestamp
          size="large"
          .data="${item}"
          @click="${this.onTabClick_}">
        </tab-search-item>`}">
  </cr-lazy-list>
</div>
<!--_html_template_end_-->`;
}
