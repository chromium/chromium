// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {TabData} from '../tab_data.js';

import type {SplitNewTabPageAppElement} from './app.js';

export function getHtml(this: SplitNewTabPageAppElement) {
  return html`<!--_html_template_start_-->
<cr-icon-button id="closeButton"
    iron-icon="tab-search:close"
    @click="${this.onClose_}">
</cr-icon-button>
<div class="title">$i18n{splitViewTitle}</div>
<div class="contents">
  ${
      this.mediaTabs_.length > 0 ? html`
    <div class="heading">$i18n{splitViewMediaHeading}</div>
    ${getTabList(this.mediaTabs_)}
  ` :
                                   html``}
  <div class="heading">$i18n{splitViewOpenHeading}</div>
  ${getTabList(this.openTabs_)}
  </div>
</div>
<!--_html_template_end_-->`;
}

function getTabList(tabDatas: TabData[]) {
  return html`
    <div class="tab-list">
      ${
      tabDatas.map(
          data => html`
        <tab-search-item class="mwb-list-item" .data="${
              data}"></tab-search-item>
      `)}
    </div>
  `;
}
