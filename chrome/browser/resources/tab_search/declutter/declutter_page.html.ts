// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {DeclutterPageElement} from './declutter_page.js';

export function getHtml(this: DeclutterPageElement) {
  return html`<!--_html_template_start_-->
  <div id="declutterPage">
    <div id="header">
      <cr-icon-button iron-icon="cr:arrow-back" @click="${this.onBackClick_}">
      </cr-icon-button>
      <div id="headerText">
        <div class="title">$i18n{declutterTitle}</div>
        <div class="subheading">Tabs not used for 7 days or more</div>
      </div>
    </div>
    <div id="tabList">
      ${this.staleTabDatas_.map((item, index) => html`
          <tab-search-item class="mwb-list-item" .data="${item}"
              role="option"
              data-index="${index}"
              @close="${this.onTabRemove_}">
          </tab-search-item>
      `)}
    </div>
    <cr-button class="action-button" @click="${this.onCloseTabsClick_}">
      Close tabs
    </cr-button>
  </div><!--_html_template_end_-->`;
}
