// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {asyncMap} from 'chrome://resources/cr_elements/async_map_directive.js';
import {html} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import type {CheckupDetailsSectionElement} from './checkup_details_section.js';

export function getHtml(this: CheckupDetailsSectionElement) {
  return html`<!--_html_template_start_-->
<div id="header">
  <cr-icon-button class="icon-arrow-back back-button" id="backButton"
      @click="${this.onBackButtonClick_}" aria-label="$i18n{backToCheckup}">
  </cr-icon-button>
  <h2 id="title" class="page-title">${this.pageTitle_}</h2>
</div>
<div id="body">
  <div id="subtitle" class="label"> ${this.pageSubtitle_} </div>
  <div id="description" class="cr-secondary-text label">
    ${this.getDescription_()}
  </div>
</div>
${!this.isReusedType() ? html`
  ${asyncMap(this.shownInsecureCredentials_, (item, index) => html`
    <checkup-list-item .item="${item}" ?first="${!index}"
        .group="${this.getCurrentGroup_(item.id)}"
        ?show-details="${this.isCompromisedType()}"
        ?show-already-changed="${this.clickedChangePassword_(item)}"
        .passwordChangeState="${this.getPasswordChangeStateFor_(item.id)}"
        @more-actions-click="${this.onMoreActionsClick_}"
        @change-password-clicked="${this.onChangePasswordClicked_}">
    </checkup-list-item>
  `, 50)}
  ${this.mutedCompromisedCredentials_.length ? html`
    <cr-expand-button id="expandMutedCompromisedCredentialsButton"
        class="cr-row list-item" no-hover
        ?expanded="${this.mutedLeakedCredentialsExpanded_}"
        @expanded-changed="${this.onMutedLeakedCredentialsExpandedChanged_}">
      $i18n{mutedCompromisedCredentials}
    </cr-expand-button>
    <cr-collapse id="mutedCredentialsList"
        ?opened="${this.mutedLeakedCredentialsExpanded_}">
      ${this.mutedCompromisedCredentials_.map((item, index) => html`
        <checkup-list-item .item="${item}" ?first="${!index}"
            .group="${this.getCurrentGroup_(item.id)}"
            ?show-details="${this.isCompromisedType()}"
            ?show-already-changed="${this.clickedChangePassword_(item)}"
            .passwordChangeState="${this.getPasswordChangeStateFor_(item.id)}"
            @more-actions-click="${this.onMoreActionsClick_}"
            @change-password-clicked="${this.onChangePasswordClicked_}">
        </checkup-list-item>
      `)}
    </cr-collapse>
  ` : ''}
` : html`
  ${asyncMap(this.credentialsWithReusedPassword_, item => html`
    <div class="cr-secondary-text label reuse-title">
      ${item.title}
    </div>
    ${item.credentials.map((subItem, index) => html`
      <checkup-list-item .item="${subItem}" ?first="${!index}"
          .group="${this.getCurrentGroup_(subItem.id)}"
          ?show-already-changed="${this.clickedChangePassword_(subItem)}"
          .passwordChangeState="${this.getPasswordChangeStateFor_(subItem.id)}"
          @more-actions-click="${this.onMoreActionsClick_}"
          @change-password-clicked="${this.onChangePasswordClicked_}">
      </checkup-list-item>
    `)}
  `, 50)}
`}
<cr-action-menu id="moreActionsMenu" role-description="$i18n{menu}"
    accessibility-label="$i18n{moreActions}">
  <button id="menuShowPassword" class="dropdown-item"
      @click="${this.onMenuShowPasswordClick_}">
    ${this.getShowHideTitle_()}
  </button>
  <button id="menuEditPassword" class="dropdown-item"
      @click="${this.onMenuEditPasswordClick_}">
    $i18n{editPassword}
  </button>
  <button id="menuDeletePassword" class="dropdown-item"
      @click="${this.onMenuDeletePasswordClick_}">
    $i18n{deletePassword}
  </button>
  ${this.isCompromisedType() ? html`
    <button id="menuMuteUnmuteButton" class="dropdown-item"
        @click="${this.onMenuMuteUnmuteClick_}"
        ?disabled="${this.isMutingDisabled_}">
      ${this.getMuteUnmuteLabel_()}
      ${this.isMutingDisabled_ ? html`
        <cr-icon icon="cr20:domain" class="policy-disabled"></cr-icon>
      ` : ''}
    </button>
  ` : ''}
</cr-action-menu>
<!--_html_template_end_-->`;
}
