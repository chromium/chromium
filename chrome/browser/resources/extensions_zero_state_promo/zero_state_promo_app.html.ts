// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {ZeroStatePromoAppElement} from './zero_state_promo_app.js';

export function getHtml(this: ZeroStatePromoAppElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
<div id="headerContainer">
  <h3 id="title">$i18n{extensionsZeroStateIphHeader}</h3>
  <cr-icon-button id="dismissButton"
      class="icon-clear"
      title="$i18n{extensionsZeroStateIphDismissButtonTitle}"
      @click="${this.onDismissButtonClick_}">
  </cr-icon-button>
</div>
${this.showChipsUi_ ? html`
  <div id="sectionHeaderContainer">
    $i18nRaw{extensionsZeroStateChipsIphDesc}
  </div>
  <div id="labelContainer">
    <cr-chip id="couponsButton" chip-role="link"
        @click="${this.onCouponsButtonClick_}">
      <cr-icon icon="zero-state-promo:coupons"></cr-icon>
      $i18n{extensionsZeroStateIphShoppingCategoryLabel}
    </cr-chip>
    <cr-chip id="writingButton" chip-role="link"
        @click="${this.onWritingButtonClick_}">
      <cr-icon icon="zero-state-promo:writing"></cr-icon>
      $i18n{extensionsZeroStateIphWritingHelpCollectionLabel}
    </cr-chip>
    <cr-chip id="productivityButton"
        @click="${this.onProductivityButtonClick_}">
      <cr-icon icon="zero-state-promo:productivity"></cr-icon>
      $i18n{extensionsZeroStateIphProductivityCategoryLabel}
    </cr-chip>
    <cr-chip id="aiButton"
        @click="${this.onAiButtonClick_}">
      <cr-icon icon="zero-state-promo:ai"></cr-icon>
      $i18n{extensionsZeroStateIphAiProductivityCollectionLabel}
    </cr-chip>
  </div>` : html`
  <div id="anchorContainer">
    $i18nRaw{extensionsZeroStatePlainLinkIphDesc}
    <a id="couponsLink" @click="${this.onCouponsButtonClick_}"
        is="action-link">
      $i18n{extensionsZeroStateIphShoppingCategoryLink},
    </a>
    <a id="writingLink" @click="${this.onWritingButtonClick_}"
        is="action-link">
      $i18n{extensionsZeroStateIphWritingHelpCollectionLink},
    </a>
    <a id="productivityLink" @click="${this.onProductivityButtonClick_}"
        is="action-link">
      $i18n{extensionsZeroStateIphProductivityCategoryLink},
    </a>
    <a id="aiLink" @click="${this.onAiButtonClick_}" is="action-link">
      $i18n{extensionsZeroStateIphAiProductivityCollectionLink}
    </a>
  </div>
  <div id="buttonContainer">
    <cr-button id="closeButton"
        @click="${this.onDismissButtonClick_}">
      $i18n{extensionsZeroStateIphCloseButtonLabel}
    </cr-button>
    <cr-button id="customActionButton"
        @click="${this.onChromeWebStoreButtonClick_}">
      $i18n{extensionsZeroStateIphCustomActionButtonLabel}
    </cr-button>
  </div>`}
<!--_html_template_end_-->`;
  // clang-format on
}
