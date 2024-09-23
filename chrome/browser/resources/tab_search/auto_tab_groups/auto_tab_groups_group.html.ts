// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {AutoTabGroupsGroupElement} from './auto_tab_groups_group.js';

export function getHtml(this: AutoTabGroupsGroupElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
<div class="group">
  ${this.multiTabOrganization ? html`
    <div class="group-header-row">
      ${this.showInput_ ? html`
        <cr-input id="multiOrganizationInput" type="text" .value="${this.name}"
            @value-changed="${this.onNameChanged_}"
            aria-label="${this.getInputAriaLabel_()}"
            @focus="${this.onInputFocus_}"
            @blur="${this.onInputBlur_}"
            @keydown="${this.onInputKeyDown_}">
        </cr-input>
      ` : html`
        <div class="group-name-row">
          <div class="auto-tab-groups-header group-name">${this.name}</div>
          <cr-icon-button class="icon-edit"
              aria-label="${this.getEditButtonAriaLabel_()}"
              title="${this.getEditButtonAriaLabel_()}"
              @click="${this.onEditClick_}">
          </cr-icon-button>
        </div>
      `}
      ${this.showReject ? html`
        <cr-icon-button id="rejectButton"
            aria-label="${this.getRejectButtonAriaLabel_()}"
            title="${this.getRejectButtonAriaLabel_()}"
            iron-icon="tab-search:close"
            @click="${this.onRejectGroupClick_}">
        </cr-icon-button>
      ` : ''}
    </div>
    <div class="divider"></div>
  ` : html`
    <cr-input id="singleOrganizationInput" type="text" .value="${this.name}"
        @value-changed="${this.onNameChanged_}"
        aria-label="${this.getInputAriaLabel_()}"
        @focus="${this.onInputFocus_}"
        @blur="${this.onInputBlur_}"
        @keydown="${this.onInputKeyDown_}">
    </cr-input>
  `}
  <cr-page-selector id="selector" role="listbox" show-all
      @keydown="${this.onListKeyDown_}"
      selectable="tab-search-item"
      @iron-select="${this.onSelectedChanged_}">
    ${this.tabDatas_.map((item, index) => html`
      ${this.showNewTabSectionHeader_(index) ? html`
        <auto-tab-groups-new-badge></auto-tab-groups-new-badge>
      ` : ''}
      <tab-search-item class="mwb-list-item" .data="${item}"
          role="option"
          ?compact="${this.multiTabOrganization}"
          tabindex="${this.getTabIndex_(index)}"
          data-index="${index}"
          @close="${this.onTabRemove_}"
          @focus="${this.onTabFocus_}"
          @blur="${this.onTabBlur_}"
          close-button-icon="tab-search:remove"
          in-suggested-group>
      </tab-search-item>
    `)}
  </cr-page-selector>
  ${!this.multiTabOrganization ? html`
    <auto-tab-groups-results-actions
        @create-group-click="${this.onCreateGroupClick_}">
    </auto-tab-groups-results-actions>
  ` : ''}
</div>
<!--_html_template_end_-->`;
  // clang-format on
}
