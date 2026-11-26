// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import type {SettingsOffersListElement} from './offers_list.js';

export function getHtml(this: SettingsOffersListElement) {
  return html`<!--_html_template_start_-->
<div id="offersHeader" class="cr-row">
  <h2 class="flex">$i18n{offersTitle}</h2>
</div>
<ul id="offers" class="list-frame vertical-list">
  ${this.offers_.map((item, index) => html`
    <li class="list-item offer">
      <div class="logo">
        <cr-icon aria-hidden="true" icon="settings20:sell"></cr-icon>
      </div>
      <div class="start">
        <div class="ellipses issuer-name">${item.issuerName}</div>
        <div class="ellipses cr-secondary-text short-title">
          ${item.shortTitle}
        </div>
        <div class="ellipses cr-secondary-text code-and-expiration">
          <span class="promo-code">${this.getPromoCodeLabel_(item)}</span>
          <span class="code-expiration-separator" aria-hidden="true">•</span>
          <span class="expiration">${this.getExpirationLabel_(item)}</span>
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
  <li id="offersNone" class="list-item" ?hidden="${this.offers_.length > 0}">
    $i18n{autofillAiEntityInstancesNone}
  </li>
</ul>
<!--_html_template_end_-->`;
}
