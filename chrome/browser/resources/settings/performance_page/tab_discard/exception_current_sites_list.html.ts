// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';
import type {ExceptionCurrentSitesListElement} from './exception_current_sites_list.js';

export function getHtml(this: ExceptionCurrentSitesListElement) {
  return html`<!--_html_template_start_-->
<div id="container" class="cr-scrollable" scrollable>
  <div class="cr-scrollable-top"></div>
  <div id="list" role="listbox" focusgroup="listbox block"
      ?hidden="${!this.currentSites_.length}">
    ${this.currentSites_.map(item => html`
      <cr-checkbox ?checked="${this.isSelectedSite_(item)}"
          data-site="${item}"
          @change="${this.onSelectionChange_}">
        <div class="label-slot">
          <site-favicon .url="${item}"></site-favicon>
          <div class="checkbox-label text-elide">${item}</div>
        </div>
      </cr-checkbox>
    `)}
    <div class="cr-scrollable-bottom"></div>
  </div>
  <div id="emptyText" ?hidden="${!!this.currentSites_.length}">
    $i18n{tabDiscardingExceptionsAddDialogCurrentTabsEmpty}
  </div>
</div>
<!--_html_template_end_-->`;
}
