// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {PrivacyGuidePromoElement} from './privacy_guide_promo.js';

export function getHtml(this: PrivacyGuidePromoElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
<div id="wrapper">
  <div id="controlsColumn">
    <h2 id="title">$i18n{privacyGuidePromoHeader}</h2>
    <div id="bodyText" class="cr-secondary-text">
      $i18n{privacyGuidePromoBody}
    </div>
    <cr-button class="action-button" id="startButton" role="button"
        aria-describedby="title bodyText" @click="${this.onPrivacyGuideStartClick_}">
      $i18n{privacyGuidePromoStartButton}
    </cr-button>
    <cr-button id="noThanksButton" role="button"
        @click="${this.onNoThanksButtonClick_}">
      $i18n{noThanks}
    </cr-button>
  </div>
  <picture>
    <source class="banner"
        srcset="./images/privacy_guide/promo_banner_dark.svg"
        media="(prefers-color-scheme: dark)">
    <img class="banner" alt=""
        src="./images/privacy_guide/promo_banner.svg">
  </picture>
</div>
<!--_html_template_end_-->`;
  // clang-format on
}
