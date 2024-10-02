// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {TabOrganizationSelectorElement} from './tab_organization_selector.js';
import {TabOrganizationFeature} from './tab_search.mojom-webui.js';

export function getHtml(this: TabOrganizationSelectorElement) {
  // clang-format off
  return html`
<!--_html_template_start_-->
<div ?hidden=${this.selectedState_ !== TabOrganizationFeature.kSelector}>
  <div id="buttonContainer">
    <tab-organization-selector-button id="autoTabGroupsButton"
        top="true"
        heading="$i18n{autoTabGroupsSelectorHeading}"
        subheading="$i18n{autoTabGroupsSelectorSubheading}"
        icon="tab-search:auto-tab-groups"
        @click="${this.onAutoTabGroupsClick_}">
    </tab-organization-selector-button>
    <tab-organization-selector-button id="declutterButton"
        bottom="true"
        heading="${this.declutterHeading_}"
        subheading="$i18n{declutterSelectorSubheading}"
        icon="tab-search:declutter"
        ?disabled="${this.disableDeclutter_}"
        @click="${this.onDeclutterClick_}">
    </tab-organization-selector-button>
  </div>
</div>

<div ?hidden=${this.selectedState_ !== TabOrganizationFeature.kAutoTabGroups}>
  <auto-tab-groups-page ?show-back-button="${true}"
      @back-click="${this.onBackClick_}">
  </auto-tab-groups-page>
</div>

<div ?hidden=${this.selectedState_ !== TabOrganizationFeature.kDeclutter}>
  <declutter-page ?show-back-button="${true}"
      @back-click="${this.onBackClick_}">
  </declutter-page>
</div>
<!--_html_template_end_-->`;
  // clang-format on
}
