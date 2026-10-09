// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import type {SettingsAutofillPageElement} from './autofill_page.js';

export function getHtml(this: SettingsAutofillPageElement) {
  return html`<!--_html_template_start_-->
<h1 id="title">$i18n{autofillPageTitle}</h1>
<div id="subtitle" class="secondary">$i18n{yourSavedInfoPageDescription}</div>
<settings-account-card></settings-account-card>

<h2 id="autofillPageTitle" class="section-header">
  $i18n{autofillPageTitle}
</h2>
<div class="card-container" @data-category-click="${this.onDataCategoryClick_}"
    @data-chip-click="${this.onDataChipClick_}">
  <category-reference-card card-title="$i18n{localPasswordManager}"
      is-external
      .categoryId="${this.hierarchy_.passwordManager.id}"
      .chips="${this.getVisibleChips_(this.hierarchy_.passwordManager.chips)}">
  </category-reference-card>
  <category-reference-card id="paymentManagerButton"
      card-title="$i18n{paymentsTitle}"
      .categoryId="${this.hierarchy_.payments.id}"
      .chips="${this.getVisibleChips_(this.hierarchy_.payments.chips)}">
  </category-reference-card>
  <category-reference-card id="addressesManagerButton"
      card-title="$i18n{contactInfoTitle}"
      .categoryId="${this.hierarchy_.contactInfo.id}"
      .chips="${this.getVisibleChips_(this.hierarchy_.contactInfo.chips)}">
  </category-reference-card>
  <category-reference-card id="identityManagerButton"
      card-title="$i18n{identityDocsCardTitle}"
      .categoryId="${this.hierarchy_.identityDocs.id}"
      .chips="${this.getVisibleChips_(this.hierarchy_.identityDocs.chips)}">
  </category-reference-card>
  <category-reference-card id="travelManagerButton"
      card-title="$i18n{travelCardTitle}"
      .categoryId="${this.hierarchy_.travel.id}"
      .chips="${this.getVisibleChips_(this.hierarchy_.travel.chips)}">
  </category-reference-card>
  ${this.isShoppingEnabled_ ? html`
    <category-reference-card id="shoppingManagerButton"
        card-title="$i18n{shoppingCardTitle}"
        .categoryId="${this.hierarchy_.shopping.id}"
        .chips="${this.getVisibleChips_(this.hierarchy_.shopping.chips)}">
    </category-reference-card>
  ` : ''}
  ${this.showSuggestionsFromGeminiSettings_ ? html`
    <div id="suggestionsFromGeminiCard">
      <cr-link-row id="suggestionsFromGeminiLinkRow"
          label="$i18n{autofillPersonalContextSettingsTitle}"
          @click="${this.onSuggestionsFromGeminiClick_}">
      </cr-link-row>
      <hr>
      <div id="suggestionsFromGeminiSubLabel" class="secondary">
        <cr-icon icon="${this.spark_}" aria-hidden="true"></cr-icon>
        $i18n{autofillPersonalContextSettingsSummary}
      </div>
    </div>
  ` : ''}
</div>

<settings-section page-title="$i18n{yourSavedInfoAutofillSettingsLabel}">
  <collapsible-autofill-settings-card></collapsible-autofill-settings-card>
</settings-section>

<settings-section page-title="$i18n{yourSavedInfoRelatedServicesTitle}">
  <div route-path="default">
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
  </div>
</settings-section>
<!--_html_template_end_-->`;
}
