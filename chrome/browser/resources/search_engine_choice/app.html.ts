// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';
import type {AppElement} from './app.js';

export function getHtml(this: AppElement) {
  return html`<!--_html_template_start_-->
<img class="tangible-sync-style-left-banner" id="leftBanner" alt="">
<img class="tangible-sync-style-right-banner" id="rightBanner" alt="">
<div class="content-container">
  <img class="product-logo" src="images/product-logo.svg"
      alt="$i18n{productLogoAltText}" aria-hidden="true">
  <h1 class="title">$i18n{title}</h1>
  <p class="subtitle">
    $i18n{subtitle}
    <a id="infoLink" href="" @click="${this.onLinkClicked_}"
        aria-label="$i18n{subtitleInfoLinkA11yLabel}">
      $i18n{subtitleInfoLink}
    </a>
  </p>
  <cr-radio-group id="choiceList"
      selected="${this.selectedChoice_}"
      @selected-changed="${this.onSelectedChoiceChangedByUser_}"
      aria-label="$i18n{choiceListA11yLabel}" role="list">
    ${this.choiceList_.map(item => html`
      <cr-radio-button aria-label="${item.name}" role="listitem"
          class="label-first hoverable"
          name="${item.prepopulateId}">
        <div class="choice">
          <div class="choice-icon"
              .style="background-image: ${item.iconPath};"></div>
          <div class="choice-text">
            <div class="search-engine-name">${item.name}</div>
            <div class="marketing-snippet
                ${this.getMarketingSnippetClass_(item)}">
              ${item.marketingSnippet}
            </div>
          </div>
        </div>
      </cr-radio-button>
    `)}
  </cr-radio-group>
</div>
<div id="buttonContainer">
  <cr-checkbox ?hidden="${!this.showGuestCheckbox_}"
      ?checked="${this.saveGuestModeSearchEngineChoice_}"
      @checked-changed="${this.onCheckboxStateChange_}">
    $i18n{guestCheckboxText}
  </cr-checkbox>
  <cr-button class="action-button" id="actionButton"
      @click="${this.onActionButtonClicked_}"
      ?disabled="${this.isActionButtonDisabled_}">
    <div class="cr-icon" slot="prefix-icon" title=""
        ?hidden="${this.hasUserScrolledToTheBottom_}">
    </div>
    ${this.actionButtonText_}
  </cr-button>
</div>

${
      this.showInfoDialog_ ? html`
  <cr-dialog id="infoDialog" show-on-attach>
    <div slot="title">
      <img class="info-dialog-illustration" alt="">
      <div>$i18n{infoDialogTitle}</div>
    </div>
    <div slot="body">
      <p>$i18n{infoDialogFirstParagraph}</p>
      <p>$i18nRaw{infoDialogSecondParagraph}</p>
      <p>$i18n{infoDialogThirdParagraph}</p>
    </div>
    <div slot="button-container">
      <cr-button class="action-button" id="infoDialogButton"
          @click="${this.onInfoDialogButtonClicked_}">
        $i18n{infoDialogButtonText}
      </cr-button>
    </div>
  </cr-dialog>
` :
                             ''}
<!--_html_template_end_-->`;
}
