// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import {TabOrganizationModelStrategy} from '../tab_search.mojom-webui.js';

import type {AutoTabGroupsNotStartedElement} from './auto_tab_groups_not_started.js';

export function getHtml(this: AutoTabGroupsNotStartedElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
<div class="auto-tab-groups-container">
  <auto-tab-groups-not-started-image></auto-tab-groups-not-started-image>
  <div class="auto-tab-groups-text-container">
    <div class="auto-tab-groups-body">
      ${this.getBody_()}
      ${this.showFre ? html`
        <table class="bullet-list">
          <tr>
            <td>
              <cr-icon icon="tab-search:plant" aria-hidden="true"></cr-icon>
            </td>
            <td>$i18n{notStartedBodyFREBullet1}</td>
          </tr>
          <tr>
            <td>
              <cr-icon icon="tab-search:google" aria-hidden="true"></cr-icon>
            </td>
            <td>$i18n{notStartedBodyFREBullet2}</td>
          </tr>
          <tr>
            <td>
              <cr-icon icon="tab-search:frame" aria-hidden="true"></cr-icon>
            </td>
            <td>$i18n{notStartedBodyFREBullet3}</td>
          </tr>
        </table>
        <a class="auto-tab-groups-link"
            role="link"
            tabindex="0"
            @click="${this.onLearnMoreClick_}">
          $i18n{learnMore}
        </a>
      ` : ''}
    </div>
  </div>
  ${this.tabOrganizationModelStrategyEnabled_ ? html`
    <div class="auto-tab-groups-header">Update model strategy preference</div>
    <cr-radio-group
        selected="${this.modelStrategy}"
        @selected-changed="${this.onModelStrategyChange_}">
      <cr-radio-button
          name="${TabOrganizationModelStrategy.kTopic}"
          label="Topic/Theme"></cr-radio-button>
      <cr-radio-button
          name="${TabOrganizationModelStrategy.kTask}"
          label="Task"></cr-radio-button>
      <cr-radio-button
          name="${TabOrganizationModelStrategy.kDomain}"
          label="Domain/Subdomain"></cr-radio-button>
    </cr-radio-group>
  ` : ''}
  <cr-button class="action-button"
      aria-label="${this.getActionButtonAriaLabel_()}"
      @click="${this.onButtonClick_}">
    ${this.getActionButtonText_()}
  </cr-button>
</div>
<!--_html_template_end_-->`;
  // clang-format on
}
