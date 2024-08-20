// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {DeclutterPageElement} from './declutter_page.js';

export function getHtml(this: DeclutterPageElement) {
  return html`<!--_html_template_start_-->
  <div id="tabList">
    ${this.staleTabDatas_.map((item, index) => html`
        <tab-search-item class="mwb-list-item" .data="${item}"
            role="option"
            data-index="${index}">
        </tab-search-item>
    `)}
  </div><!--_html_template_end_-->`;
}
