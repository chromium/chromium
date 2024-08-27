// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {TabOrganizationSelectorElement} from './tab_organization_selector.js';
import {OrganizationFeature} from './tab_organization_selector.js';

export function getHtml(this: TabOrganizationSelectorElement) {
  // clang-format off
  return html`
<!--_html_template_start_-->
<div id="buttonContainer"
    ?hidden=${this.selectedState_ !== OrganizationFeature.NONE}>
  <cr-button id="autoTabGroupsButton" @click="${this.onAutoTabGroupsClick_}">
    Auto tab groups
  </cr-button>
  <cr-button id="declutterButton" @click="${this.onDeclutterClick_}">
    Declutter
  </cr-button>
</div>

<div ?hidden=${this.selectedState_ !== OrganizationFeature.AUTO_TAB_GROUPS}>
  <auto-tab-groups-page></auto-tab-groups-page>
</div>

<div ?hidden=${this.selectedState_ !== OrganizationFeature.DECLUTTER}>
  <declutter-page></declutter-page>
</div>
<!--_html_template_end_-->`;
  // clang-format on
}
