// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from 'chrome://resources/lit/v3_0/lit.rollup.js';

// <if expr="not is_chromeos">
import {ChromeSigninAccessPoint} from '/shared/settings/people_page/sync_browser_proxy.js';
// </if>

import type {SettingsAccountCardElement} from './account_card.js';

export function getHtml(this: SettingsAccountCardElement) {
  return html`<!--_html_template_start_-->
<if expr="not is_chromeos">
  ${this.shouldShowSyncAccountControl_() ? html`
    <div id="account-card">
      <settings-sync-account-control
          .syncStatus="${this.syncStatus!}"
          promo-label-with-account="$i18n{peopleSignInPrompt}"
          promo-label-with-no-account="$i18n{peopleSignInPrompt}"
          promo-secondary-label-with-account=
              "$i18n{peopleSignInPromptSecondaryWithAccount}"
          promo-secondary-label-with-no-account=
              "$i18n{peopleSignInPromptSecondaryWithNoAccount}"
          .accessPoint="${ChromeSigninAccessPoint.SETTINGS_YOUR_SAVED_INFO}">
      </settings-sync-account-control>
    </div>
  ` : ''}
  ${this.shouldLinkToAccountSettingsPage_() ? html`
    <div id="account-card">
      <cr-link-row id="account-subpage-row" class="cr-row first"
          @click="${this.onAccountClick_}">
        <div id="profile-icon"
            style="background-image: ${this.getIconImageSet_(
                this.primaryAccountIconUrl_)}">
        </div>
        <div class="cr-row-gap cr-padded-text flex no-min-width">
          <div id="account-name" class="text-elide">
            ${this.primaryAccountName_}
          </div>
          <div id="account-subtitle" class="secondary">
            ${this.getAccountRowSubtitle_()}
          </div>
        </div>
      </cr-link-row>
    </div>
  ` : ''}
</if>

<if expr="is_chromeos">
  <div id="account-card">
    <div id="profile-row" class="cr-row first two-line"
        ?actionable="${this.isProfileActionable_}"
        @click="${this.onProfileClick_}">
      ${this.syncStatus ? html`
        <div id="profile-icon"
            style="background-image: ${this.getIconImageSet_(
                this.profileIconUrl_)}">
        </div>
        <div class="flex cr-row-gap cr-padded-text text-elide">
          <span id="profile-name">${this.profileName_}</span>
          <div class="secondary" ?hidden="${!this.isSyncing_()}">
            ${this.syncStatus.signedInUsername}
          </div>
        </div>
        <cr-icon-button class="icon-external"
            id="profile-subpage-arrow"
            ?hidden="${!this.isProfileActionable_}"
            aria-label="$i18n{accountManagerSubMenuLabel}"
            aria-describedby="profile-name"></cr-icon-button>
      ` : ''}
    </div>
  </div>
</if>
<!--_html_template_end_-->`;
}
