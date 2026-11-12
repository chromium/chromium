// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import type {SettingsAutofillPageV2Element} from './autofill_page_v2.js';

export function getHtml(this: SettingsAutofillPageV2Element) {
  return html`<!--_html_template_start_-->

<settings-section page-title="$i18n{autofillPageTitle}">
  <cr-link-row label="$i18n{localPasswordManager}"
      sub-label="$i18n{passwordManagerSublabel}" external
      @click="${this.onPasswordManagerClick_}"
      start-icon="cr20:password-manager">
    <span slot="icon-label">${this.passwordManagerCount_}</span>
  </cr-link-row>
  <cr-link-row id="paymentManagerButton" label="$i18n{paymentsTitle}"
      sub-label="$i18n{paymentsSublabel}" @click="${this.onPaymentsClick_}"
      start-icon="settings20:credit-card">
    <span slot="icon-label">${this.paymentsCount_}</span>
  </cr-link-row>
  <cr-link-row id="addressesManagerButton" label="$i18n{addresses}"
      sub-label="$i18n{contactInfoSublabel}" start-icon="settings20:location-on"
      @click="${this.onContactInfoClick_}">
    <span slot="icon-label">${this.contactInfoCount_}</span>
  </cr-link-row>
  <cr-link-row id="identityManagerButton" label="$i18n{identityDocsCardTitle}"
      sub-label="$i18n{identityDocsSublabel}" start-icon="settings20:id-card"
      @click="${this.onIdentityDocsClick_}">
    <span slot="icon-label">${this.identityDocsCount_}</span>
  </cr-link-row>
  <cr-link-row id="travelManagerButton" label="$i18n{travelCardTitle}"
      sub-label="$i18n{travelSublabel}" start-icon="settings20:travel"
      @click="${this.onTravelClick_}">
    <span slot="icon-label">${this.travelCount_}</span>
  </cr-link-row>
  ${this.isOffersAndLoyaltyCardsEnabled_ ? html`
    <cr-link-row id="offersAndLoyaltyCardsManagerButton"
        label="$i18n{offersAndLoyaltyCardsCardTitle}"
        sub-label="$i18n{offersAndLoyaltyCardsSublabel}"
        start-icon="settings20:loyalty-programs"
        @click="${this.onOffersAndLoyaltyCardsClick_}">
      <span slot="icon-label">${this.offersAndLoyaltyCardsCount_}</span>
    </cr-link-row>
  ` : ''}
  ${this.isShoppingEnabled_ ? html`
    <cr-link-row id="shoppingManagerButton" label="$i18n{shoppingCardTitle}"
        start-icon="settings20:shopping-bag" @click="${this.onShoppingClick_}">
      <span slot="icon-label">${this.shoppingCount_}</span>
    </cr-link-row>
  ` : ''}
  ${this.showSuggestionsFromGeminiSettings_ ? html`
    <cr-link-row id="suggestionsFromGeminiLinkRow"
        label="$i18n{autofillPersonalContextSettingsTitle}"
        start-icon="${this.spark_}"
        @click="${this.onSuggestionsFromGeminiClick_}">
    </cr-link-row>
  ` : ''}
</settings-section>


<settings-section>
  <collapsible-autofill-settings-card></collapsible-autofill-settings-card>
</settings-section>

<settings-section page-title="$i18n{yourSavedInfoRelatedServicesTitle}">
  <cr-link-row id="passwordManagerButton" label="$i18n{localPasswordManager}"
      @click="${this.onPasswordManagerRelatedServiceClick_}"
      start-icon="cr20:password-manager" external>
  </cr-link-row>
  <cr-link-row id="googleWalletButton" label="$i18n{googleWalletTitle}"
      @click="${this.onGoogleWalletRelatedServiceClick_}"
      start-icon="settings20:wallet" external>
  </cr-link-row>
  <cr-link-row id="googleAccountButton" label="$i18n{googleAccount}"
      @click="${this.onGoogleAccountRelatedServiceClick_}"
      start-icon="settings20:google" external>
  </cr-link-row>
</settings-section>
<!--_html_template_end_-->`;
}
