// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import type {SettingsSearchEnginesListElement} from './search_engines_list.js';

export function getHtml(this: SettingsSearchEnginesListElement) {
  return html`<!--_html_template_start_-->
<div id="outer" class="list-frame" role="table">
  <div role="rowgroup">
    <div role="row" id="headers" class="column-header">
      <span class="name" role="columnheader">$i18n{searchEnginesName}</span>
      <span class="additional-info-column-group">
        <span class="shortcut" role="columnheader"
              ?hidden="${!this.showShortcut}">
          $i18n{searchEnginesShortcut}
        </span>
        <span class="url" role="columnheader"
              ?hidden="${!this.showQueryUrl}">
          $i18n{searchEnginesQueryURL}
        </span>
        <span class="controls-group">
          <span class="icon-placeholder"></span>
          <span class="icon-placeholder"></span>
        </span>
      </span>
    </div>
  </div>
  ${!this.collapseList ? html`
    <div id="container" class="scroll-container"
        ?scrollable="${this.fixedHeight}">
      <div role="rowgroup">
        ${this.engines.map(item => html`
          <settings-search-engine-entry .engine="${item}"
              ?show-query-url="${this.showQueryUrl}"
              ?show-shortcut="${this.showShortcut}">
          </settings-search-engine-entry>
        `)}
      </div>
    </div>
  ` : ''}

  ${this.collapseList ? html`
    <div id="containerWithCollapsibleSection" class="scroll-container"
        ?hidden="${!this.collapseList}"
        ?scrollable="${this.fixedHeight}">
      <div role="rowgroup">
        ${this.visibleEngines_.map(item => html`
          <settings-search-engine-entry .engine="${item}"
              ?show-shortcut="${this.showShortcut}"
              ?show-query-url="${this.showQueryUrl}">
          </settings-search-engine-entry>
        `)}
      </div>

      <cr-expand-button no-hover class="cr-row"
          ?hidden="${!this.collapsedEngines_.length}"
          ?expanded="${this.enginesListExpanded_}"
          @expanded-changed="${this.onEnginesListExpandedChanged_}">
        <div>${this.expandListText}</div>
      </cr-expand-button>
      <cr-collapse ?opened="${this.enginesListExpanded_}">
        <div role="rowgroup">
          ${this.collapsedEngines_.map(item => html`
            <settings-search-engine-entry .engine="${item}"
                ?show-shortcut="${this.showShortcut}"
                ?show-query-url="${this.showQueryUrl}">
            </settings-search-engine-entry>
          `)}
        </div>
      </cr-collapse>
    </div>
  ` : ''}
</div>
<!--_html_template_end_-->`;
}
