// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import {ariaLabel} from '../tab_data.js';
import type {TabData} from '../tab_data.js';

import type {SplitNewTabPageAppElement} from './app.js';

export function getHtml(this: SplitNewTabPageAppElement) {
  return html`<!--_html_template_start_-->
<div id="header">
  <cr-icon-button id="closeButton"
      iron-icon="tab-search:close"
      title="$i18n{splitViewCloseButtonAriaLabel}"
      @click="${this.onClose_}">
  </cr-icon-button>
  ${
      this.allEligibleTabs_.length === 0 ? html`
        <picture>
          <source media="(prefers-color-scheme: dark)"
              srcset="./split_view/images/empty_dark.svg">
          <img id="product-logo" srcset="./split_view/images/empty.svg" alt="">
        </picture>
      ` :
                                           html``}
  <h1 class="title">${this.title_}</h1>
  ${
      this.allEligibleTabs_.length === 0 ?
          html`<div class="body">$i18n{splitViewEmptyBody}</div>` :
          html``}
</div>
<div class="tab-list" ?hidden="${this.allEligibleTabs_.length === 0}">
  <selectable-lazy-list id="splitTabsList" class="scroller"
      .items="${this.allEligibleTabs_}"
      item-size="66"
      max-height="${this.minViewportHeight_}"
      role="listbox"
      .template="${
      (item: TabData, index: number) =>
          html`<tab-search-item class="mwb-list-item selectable"
          hide-close-button
          hide-timestamp
          size="large"
          .data="${item}"
          data-index="${index}"
          @click="${this.onTabClick_}"
          @focus="${this.onTabFocus_}"
          @focusout="${this.onTabFocusOut_}"
          @keydown="${this.onTabKeyDown_}"
          role="option"
          aria-label="${ariaLabel(item)}"
          tabindex="0">
        </tab-search-item>`}">
  </selectable-lazy-list>
</div>
<!--_html_template_end_-->`;
}
