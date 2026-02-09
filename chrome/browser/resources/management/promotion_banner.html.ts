// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {PromotionBannerElement} from './promotion_banner.js';

export function getHtml(this: PromotionBannerElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
<section id="promotion-banner-section-main" class="banner-section"
    aria-label="$i18n{promotionBannerAriaLabel}">
  <div id="promotion-banner-section" class="banner-section-container">
    <div id="banner-actions" class="banner-actions-container">
      <div id="promotion-main-text" class="promotion-main-text-container">
        <div id="promotion-text" class="promotion-text-container">
          <h2 id="promotion-banner-title" class="banner-title">
            $i18n{promotionBannerTitle}
          </h2>
          <div id="promotion-banner-description" class="banner-description">
            $i18n{promotionBannerDesc}
          </div>
        </div>
        <button id="promotion-redirect-button" class="blue-pill-button"
            @click="${this.onPromotionRedirectClick_}"
            aria-description="$i18n{promotionBannerNewTabAriaDescription}">
          $i18n{promotionBannerBtn}
        </button>
      </div>
      <button id="promotion-dismiss-button" class="dismiss-button"
          tabindex="0" @click="${this.onDismissPromotionClick_}"
          aria-label="$i18n{promotionBannerDismissAriaLabel}">
        <div id="close-icon-container"></div>
      </button>
    </div>
  </div>
</section>
<!--_html_template_end_-->`;
  // clang-format on
}
