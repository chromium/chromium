// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import type {SettingsIbanListEntryElement} from './iban_list_entry.js';

export function getHtml(this: SettingsIbanListEntryElement) {
  return html`<!--_html_template_start_-->
<div class="list-item type-column" role="row">
  <img id="ibanImage" src="chrome://settings/images/iban.svg" alt="">
  <div class="summary-column screen-reader-only-host-node" role="cell">
    <div class="screen-reader-only">
      ${this.getA11yIbanDescription_()}, ${this.iban.nickname}
    </div>
    <div id="label" class="ellipses" aria-hidden="true">
      ${this.getLabel_()}
    </div>
    <div id="subLabel" class="ellipses sub-label" aria-hidden="true">
      ${this.getSubLabel_()}
    </div>
  </div>
  <div role="cell" class="second-column">
    <div id="paymentsIndicator"
        ?hidden="${!this.shouldShowGooglePaymentsIndicator_()}">
<if expr="_google_chrome">
      <picture id="paymentsIcon">
        <source
            srcset="${this.getGooglePayDarkModeLogoSrcSet_()}"
            media="(prefers-color-scheme: dark)">
        <img alt=""
            srcset="${this.getGooglePayLightModeLogoSrcSet_()}">
      </picture>
</if>
<if expr="not _google_chrome">
      <span class="sub-label">$i18n{googlePayments}</span>
</if>
    </div>
    ${this.showDotsMenu_() ? html`
      <cr-icon-button class="icon-more-vert" id="ibanMenu"
           title="${this.getMoreActionsTitle_()}"
           @click="${this.onDotsMenuClick_}">
      </cr-icon-button>
    ` : ''}
    ${this.shouldShowOutlinkWithWalletBranding_() ? html`
      <cr-icon-button class="icon-external" id="remoteIbanLink"
          title="$i18n{remotePaymentMethodsWalletLinkLabel}" role="link"
          @click="${this.onRemoteEditClick_}"
          aria-description="$i18n{opensInNewTab}"></cr-icon-button>
    ` : ''}
    ${this.shouldShowOutlinkWithoutWalletBranding_() ? html`
      <cr-icon-button class="icon-external" id="remoteIbanLink"
          title="$i18n{remotePaymentMethodsLinkLabel}" role="link"
          @click="${this.onRemoteEditClick_}"
          aria-description="$i18n{opensInNewTab}"></cr-icon-button>
    ` : ''}
  </div>
</div>
<!--_html_template_end_-->`;
}
