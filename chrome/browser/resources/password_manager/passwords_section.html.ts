// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {asyncMap} from 'chrome://resources/cr_elements/async_map_directive.js';
import {html} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import type {PasswordsSectionElement} from './passwords_section.js';

export function getHtml(this: PasswordsSectionElement) {
  return html`<!--_html_template_start_-->
${this.computeShouldShowNotificationCard_() ? html`
  <notification-card id="notificationCard" class="card"
      .card="${this.notificationCard_!}" role="region"
      aria-label="$i18n{notificationCardAriaLabel}"
      @card-closed="${this.onCardClosed_}">
  </notification-card>
` : ''}
<div id="header">
  <h2 class="page-title">$i18n{passwords}</h2>
  <cr-button id="addPasswordButton" @click="${this.onAddPasswordClick_}"
      title="$i18n{addPasswordTitle}"
      ?hidden="${this.passwordSavingBlockedByPolicy_}">
    $i18n{addPassword}
  </cr-button>
</div>
<div id="descriptionLabel" class="cr-secondary-text"
    ?hidden="${!this.computeShowPasswordsDescription_()}"
    .innerHTML="${this.i18nAdvanced('passwordsSectionDescription')}">
</div>
<div id="trustedVaultUnlock" class="cr-secondary-text"
    ?hidden="${!this.showTrustedVaultUnlockOption_()}"
    .innerHTML="${this.i18nAdvanced('emptyStateTrustedVaultKeyNeeded')}"
    @click="${this.onTrustedVaultUnlockClick_}">
</div>
<div id="importPasswords" class="cr-secondary-text"
    ?hidden="${!this.showImportPasswordsOption_()}"
    .innerHTML="${this.computeImportPasswordsText_()}"
    @click="${this.onImportPasswordsClick_}">
</div>
<div id="noPasswordsFound" class="cr-secondary-text"
    ?hidden="${!this.showNoPasswordsFound_()}">
  $i18n{noPasswordsFound}
</div>
<div class="card" id="passwords" role="list" ?hidden="${this.hideGroupsList_()}"
    aria-label="$i18n{passwordListAriaLabel}">
  ${asyncMap(this.filteredGroups_, (item, index) => html`
    <password-list-item .item="${item}" ?first="${!index}"
        ?is-account-store-user="${this.isAccountStoreUser}"
        @password-details-shown="${this.onPasswordDetailsShown_}"
        search-term="${this.searchTerm_}" role="listitem">
    </password-list-item>
  `, 50)}
</div>
${this.showAddPasswordDialog_ ? html`
  <add-password-dialog @close="${this.onAddPasswordDialogClose_}"
      id="addPasswordDialog">
  </add-password-dialog>
` : ''}
${this.showAuthTimedOutDialog_ ? html`
  <auth-timed-out-dialog @close="${this.onAuthTimedOutDialogClose_}"
      id="authTimedOutDialog">
  </auth-timed-out-dialog>
` : ''}
<!--_html_template_end_-->`;
}
