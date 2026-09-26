// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import type {SettingsPayOverTimeIssuerListEntryElement} from './pay_over_time_issuer_list_entry.js';

export function getHtml(this: SettingsPayOverTimeIssuerListEntryElement) {
  return html`<!--_html_template_start_-->
<div class="list-item" role="row">
  <div class="type-column" role="cell">
    <picture id="payOverTimeIssuerImage">
      <source
          srcset="${this.getIssuerImage_(this.payOverTimeIssuer.imageSrcDark)}"
          media="(prefers-color-scheme: dark)">
      <img alt=""
          srcset="${this.getIssuerImage_(this.payOverTimeIssuer.imageSrc)}">
    </picture>
    <div class="summary-column screen-reader-only-host-node" role="cell">
      <div id="summaryLabel" class="ellipses">
        ${this.payOverTimeIssuer.displayName}
      </div>
    </div>
  </div>
  <div class="misc-column" role="cell">
    <div id="paymentsIndicator">
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
    <cr-icon-button class="icon-external" id="remotePayOverTimeIssuerLink"
        title="${this.getRemotePaymentMethodsLinkLabel_()}" role="link"
        @click="${this.onRemoteEditClick_}"
        aria-description="$i18n{opensInNewTab}">
    </cr-icon-button>
  </div>
</div>
<!--_html_template_end_-->`;
}
