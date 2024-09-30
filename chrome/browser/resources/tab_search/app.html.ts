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
      .tabIcons="${this.tabIcons_}"
      .selected="${this.selectedTabIndex_}"
      @selected-changed="${this.onSelectedTabChanged_}">
  </cr-tabs>
  <cr-page-selector .selected="${this.selectedTabIndex_}">
    <tab-search-page></tab-search-page>
    ${getOrganizationPage(this.tabOrganizationEnabled_, this.declutterEnabled_)}
  </cr-page-selector>
` : html`
  <tab-search-page></tab-search-page>
`}`;
  // clang-format on
}

function getOrganizationPage(
    organizationEnabled: boolean, declutterEnabled: boolean) {
  if (organizationEnabled && declutterEnabled) {
    return html`<tab-organization-selector></tab-organization-selector>`;
  } else if (organizationEnabled) {
    return html`<auto-tab-groups-page></auto-tab-groups-page>`;
  } else if (declutterEnabled) {
    return html`<declutter-page></declutter-page>`;
  } else {
    return '';
  }
}
