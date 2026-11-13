// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import type {SettingsLoyaltyCardsListElement} from './loyalty_cards_list.js';

export function getHtml(this: SettingsLoyaltyCardsListElement) {
  return html`<!--_html_template_start_-->
<div id="loyaltyCardsHeader" class="cr-row">
  <h2 class="flex">$i18n{loyaltyCardsTitle}</h2>
</div>
<ul id="loyaltyCards" class="list-frame vertical-list">
  ${this.loyaltyCards_.map((item, index) => html`
    <li class="list-item loyalty-card">
      <div class="logo">
        <cr-icon icon="settings20:loyalty-programs"></cr-icon>
      </div>
      <div class="start">
        <div class="ellipses merchant-name">${item.merchantName}</div>
        <div class="ellipses cr-secondary-text card-number">
          ${item.loyaltyCardNumber}
        </div>
      </div>
      <div class="wallet-indicator">
<if expr="_google_chrome">
        ${this.autofillEnableGradientGoogleLogos_ ? html`
          <img class="wallet-icon"
              srcset="${this.getScaledSrcSet_(
                  'chrome://theme/IDR_AUTOFILL_GOOGLE_WALLET_ICON_WITH_GRADIENT')}"
              alt="" role="presentation">
        ` : html`
          <cr-icon aria-hidden="true" icon="settings-internal:google-wallet">
          </cr-icon>
        `}
</if>
<if expr="not _google_chrome">
        <span class="sub-label">$i18n{googleWallet}</span>
</if>
      </div>
      <cr-icon-button class="icon-external wallet-link"
          title="$i18n{remoteWalletPassesLinkLabel}" role="link"
          data-index="${index}" @click="${this.onWalletLinkClick_}"
          aria-description="$i18n{opensInNewTab}">
      </cr-icon-button>
    </li>
  `)}
  <li id="loyaltyCardsNone" class="list-item"
      ?hidden="${this.loyaltyCards_.length > 0}">
    $i18n{autofillAiEntityInstancesNone}
  </li>
</ul>
<!--_html_template_end_-->`;
}
