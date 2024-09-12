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
<div ?hidden=${this.selectedState_ !== OrganizationFeature.NONE}>
  <div id="buttonContainer">
    <tab-organization-selector-button id="autoTabGroupsButton"
        top="true"
        heading="$i18n{autoTabGroupsSelectorHeading}"
        subheading="$i18n{autoTabGroupsSelectorSubheading}"
        icon="cr:group"
        @click="${this.onAutoTabGroupsClick_}">
    </tab-organization-selector-button>
    <tab-organization-selector-button id="declutterButton"
        bottom="true"
        heading="${this.declutterHeading_}"
        subheading="$i18n{declutterSelectorSubheading}"
        icon="cr:delete"
        ?disabled="${this.disableDeclutter_}"
        @click="${this.onDeclutterClick_}">
    </tab-organization-selector-button>
  </div>
</div>

<div ?hidden=${this.selectedState_ !== OrganizationFeature.AUTO_TAB_GROUPS}>
  <auto-tab-groups-page @back-click="${this.onBackClick_}">
  </auto-tab-groups-page>
</div>

<div ?hidden=${this.selectedState_ !== OrganizationFeature.DECLUTTER}>
  <declutter-page @back-click="${this.onBackClick_}"></declutter-page>
</div>
<!--_html_template_end_-->`;
  // clang-format on
}
