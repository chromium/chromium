// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import type {ViewerPageSelectorElement} from './viewer_page_selector.js';

export function getHtml(this: ViewerPageSelectorElement) {
  return html`<!--_html_template_start_-->
<div id="content">
  <input part="input" type="text" id="pageSelector" .value="${this.pageNo}"
      @pointerup="${this.select}" @input="${this.onInput_}"
      @change="${this.pageNoCommitted}" aria-label="$i18n{labelPageNumber}">
  <span id="divider">/</span>
  <span id="pagelength">${this.docLength}</span>
</div>
<!--_html_template_end_-->`;
}
