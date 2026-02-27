// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {TabSearchAppElement} from './app.js';

export function getHtml(this: TabSearchAppElement) {
  // clang-format off
  return html`
${(this.tabOrganizationEnabled_ || this.declutterEnabled_) ? html`
  <cr-tabs
      .tabNames="${this.tabNames_}"
      .selected="${this.sectionToIndex_(this.selectedTabSection_)}"
      @selected-changed="${this.onCrTabsSelectedChanged_}">
  </cr-tabs>
  <cr-page-selector
      .selected="${this.sectionToIndex_(this.selectedTabSection_)}">
    <tab-search-page available-height="${this.availableHeight_}">
    </tab-search-page>
    ${this.tabOrganizationEnabled_ ? html`
      ${this.declutterEnabled_ ? html`
        <tab-organization-selector available-height="${this.availableHeight_}">
        </tab-organization-selector>
      ` : html`
        <auto-tab-groups-page available-height="${this.availableHeight_}">
        </auto-tab-groups-page>
      `}
    ` : html`
      ${this.declutterEnabled_ ? html`
        <declutter-page available-height="${this.availableHeight_}">
        </declutter-page>` : ''
      }
    `}
  </cr-page-selector>
` : html`
  <tab-search-page available-height="${this.availableHeight_}">
  </tab-search-page>
`}`;
  // clang-format on
}
