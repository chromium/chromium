// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {HistoryClustersAppElement} from './app.js';

export function getHtml(this: HistoryClustersAppElement) {
  return html`<!--_html_template_start_-->
<cr-toolbar-search-field id="searchbox"
    @search-changed="${this.onSearchChanged_}"
    label="$i18n{historyClustersSearchPrompt}"
    clear-label="$i18n{clearSearch}"
    @contextmenu="${this.onContextMenu_}">
</cr-toolbar-search-field>
<history-clusters id="historyClusters"
    query="${this.query}"
    path="journeys"
    @query-changed-by-user="${this.onQueryChangedByUser_}"
    class="sp-scroller sp-scroller-bottom-of-page"
    .scrollTarget="${this.scrollTarget_}">
</history-clusters>
<!--_html_template_end_-->`;
}
