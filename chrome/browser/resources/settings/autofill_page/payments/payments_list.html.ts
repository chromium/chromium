// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import type {SettingsPaymentsListElement} from './payments_list.js';

export function getHtml(this: SettingsPaymentsListElement) {
  return html`<!--_html_template_start_-->
<div role="table">
  <div class="vertical-list list-with-header" role="rowgroup">
    ${this.creditCards.map((item, index) => html`
      <settings-credit-card-list-entry id="${this.getCreditCardId_(index)}"
          class="payment-method" .creditCard="${item}">
      </settings-credit-card-list-entry>
    `)}
  </div>
  <div class="vertical-list list-with-header" role="rowgroup">
    ${this.ibans.map((item, index) => html`
      <settings-iban-list-entry id="${this.getIbanId_(index)}"
          class="payment-method" .iban="${item}">
      </settings-iban-list-entry>
    `)}
  </div>
  <div class="vertical-list list-with-header" role="rowgroup">
    ${this.payOverTimeIssuers.map(item => html`
      <settings-pay-over-time-issuer-list-entry class="payment-method"
          .payOverTimeIssuer="${item}">
      </settings-pay-over-time-issuer-list-entry>
    `)}
  </div>
</div>
<div id="noPaymentMethodsLabel" class="list-item"
    ?hidden="${this.showAnyPaymentMethods_()}">
  $i18n{noPaymentMethodsFound}
</div>
<!--_html_template_end_-->`;
}
