// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {asyncMap} from 'chrome://resources/cr_elements/async_map_directive.js';
import {html} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import type {PasswordDetailsSectionElement} from './password_details_section.js';

export function getHtml(this: PasswordDetailsSectionElement) {
  return html`<!--_html_template_start_-->
<div id="header">
  <cr-icon-button class="icon-arrow-back back-button" id="backButton"
      @click="${this.onBackButtonClick_}" aria-label="$i18n{backToPasswords}">
  </cr-icon-button>
  <!-- TODO(crbug.com/40234318): Support icons for android apps. -->
  <site-favicon id="favicon" .url="${this.selectedGroup_?.iconUrl || ''}"
      .domain="${this.selectedGroup_?.name || ''}" aria-hidden="true">
  </site-favicon>
  <h2 id="title" class="page-title text-elide">
    ${this.selectedGroup_?.name || ''}
  </h2>
</div>
${this.selectedGroup_?.name ? html`
  ${asyncMap(this.selectedGroup_.entries, (item, index) => html`
    ${item.isPasskey ? html`
      <passkey-details-card .passkey="${item}"></passkey-details-card>
    ` : html`
      <password-details-card .password="${item}"
          .groupName="${this.selectedGroup_!.name}"
          .iconUrl="${this.selectedGroup_!.iconUrl}"
          ?should-register-sharing-promo="${index === 0}"
          ?is-using-account-store="${this.isAccountStoreUser}">
      </password-details-card>
    `}
    ${item.backupPassword ? html`
      <backup-password-details-card .password="${item}">
      </backup-password-details-card>
    ` : ''}
  `, 10)}
` : ''}
<!--_html_template_end_-->`;
}
